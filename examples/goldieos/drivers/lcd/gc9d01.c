#include "spi_lcd.h"
#include "driver_manager.h"
#include "system_res/pics/boot_pic.h"

#ifdef SUPPORT_FT6636_TOUCH
extern void ft6636_entry(void);
#else
#ifdef SUPPORT_CST816D_TOUCH
	extern void cst816d_entry(void);
#endif
#endif

#ifdef LCM_USE_EXT_GPIO
static GpioDrvExt * gpio_ext_drv;
static void init_ext_gpio()
{
	gpio_ext_drv = wait_drv(GPIO_EXT_DRV_INDEX);
	gpio_ext_drv->drv_init();
    gpio_ext_drv->set_gpio_func(RST_EXT_GPIO, GOLDIE_GPIO_DIRECTION_OUTPUT);
    gpio_ext_drv->set_gpio_func(LED_EXT_GPIO, GOLDIE_GPIO_DIRECTION_OUTPUT);
    gpio_ext_drv->set_gpio_value(RST_EXT_GPIO, 0);
    gpio_ext_drv->set_gpio_value(LED_EXT_GPIO, 0);
}

static void ext_gpio_rst_set(int value)
{
	if(value == 0){
		gpio_ext_drv->set_gpio_value(RST_EXT_GPIO, 0);
	}else{
		gpio_ext_drv->set_gpio_value(RST_EXT_GPIO, 1);
	}
}

static void ext_gpio_led_set(int value)
{
	if(value == 0){
		gpio_ext_drv->set_gpio_value(LED_EXT_GPIO, 1);
	}else{
		gpio_ext_drv->set_gpio_value(LED_EXT_GPIO, 0);
	}
}

#define LCD_LED_ON         ext_gpio_led_set(1)
#define LCD_LED_OFF        ext_gpio_led_set(0)
#define LCD_RST_SET        ext_gpio_rst_set(1)
#define LCD_RST_CLR        ext_gpio_rst_set(0)
#endif

static SpiDrv * spi_drv = 0;
#define FB_SIZE  4096
#define SAFETY_FB_SIZE  (FB_SIZE-16)
static uint32_t fb_buffer[FB_SIZE/4]; 
static goldie_mutex display_window_mutex;
static goldie_mutex display_drv_mutex;
static void * task_handle = 0;
static int lcm_id = 0;
static int init_flag = 0;
static int g_dual_screen_done = 0;
_lcd_dev lcddev;

static void cs_ctrl_clr(int lcm_id)
{
 	switch(lcm_id){
		case 0:
			goldie_gpio_setval(LCD_CS, 0);
			break;
		case 1:
			goldie_gpio_setval(LCD_CS1, 0);
			break;
		case 2:
			goldie_gpio_setval(LCD_CS, 0);
			goldie_gpio_setval(LCD_CS1, 0);
			break;
		default:
			break;
 	}
}

static void cs_ctrl_set(int lcm_id)
{
 	switch(lcm_id){
		case 0:
			goldie_gpio_setval(LCD_CS, 1);
			break;
		case 1:
			goldie_gpio_setval(LCD_CS1, 1);
			break;
		case 2:
			goldie_gpio_setval(LCD_CS, 1);
			goldie_gpio_setval(LCD_CS1, 1);
			break;
		default:
			break;
 	}
}

// ========== 底层 SPI 操作 ==========
static void LCD_WR_REG(uint8_t data)
{ 
	if(init_flag == 0){
		cs_ctrl_clr(0);
		cs_ctrl_clr(1);
	}else{
		cs_ctrl_clr(lcm_id);
	}
  	
	LCD_RS_CLR;	  
	spi_drv->spi_write(&data, 1);
	if(init_flag == 0){
		cs_ctrl_set(0);
		cs_ctrl_set(1);
	}else{
		cs_ctrl_set(lcm_id);
	}
 	
}

static void LCD_WR_DATA(uint8_t data)
{
	if(init_flag == 0){
		cs_ctrl_clr(0);
		cs_ctrl_clr(1);
	}else{
		cs_ctrl_clr(lcm_id);
	}
	
	LCD_RS_SET;
	spi_drv->spi_write(&data, 1);
	if(init_flag == 0){
		cs_ctrl_set(0);
		cs_ctrl_set(1);
	}else{
		cs_ctrl_set(lcm_id);
	}
}

static void LCD_WriteReg(uint8_t LCD_Reg, uint16_t LCD_RegValue)
{	
	LCD_WR_REG(LCD_Reg);  
	LCD_WR_DATA(LCD_RegValue);	    		 
}	   

static void LCD_WriteRAM_Prepare(void)
{
	LCD_WR_REG(lcddev.wramcmd);
}	 

static void LCD_SetWindows(uint16_t xStar, uint16_t yStar, uint16_t xEnd, uint16_t yEnd)
{	
	LCD_WR_REG(lcddev.setxcmd);	
	LCD_WR_DATA(xStar >> 8);
	LCD_WR_DATA(0x00FF & xStar);		
	LCD_WR_DATA(xEnd >> 8);
	LCD_WR_DATA(0x00FF & xEnd);

	LCD_WR_REG(lcddev.setycmd);	
	LCD_WR_DATA(yStar >> 8);
	LCD_WR_DATA(0x00FF & yStar);		
	LCD_WR_DATA(yEnd >> 8);
	LCD_WR_DATA(0x00FF & yEnd);

	LCD_WriteRAM_Prepare();			
}   

static void LCD_SetWindows_2(uint16_t xStar, uint16_t yStar, uint16_t xEnd, uint16_t yEnd)
{	
	LCD_WR_REG(lcddev.setxcmd);	
	LCD_WR_DATA(xStar >> 8);
	LCD_WR_DATA(0x00FF & xStar);		
	LCD_WR_DATA(xEnd >> 8);
	LCD_WR_DATA(0x00FF & xEnd);

	LCD_WR_REG(lcddev.setycmd);	
	LCD_WR_DATA(yStar >> 8);
	LCD_WR_DATA(0x00FF & yStar);		
	LCD_WR_DATA(yEnd >> 8);
	LCD_WR_DATA(0x00FF & yEnd);
} 

static void LCD_direction(uint8_t direction)
{ 
	lcddev.setxcmd = 0x2A;
	lcddev.setycmd = 0x2B;
	lcddev.wramcmd = 0x2C;
    
    switch(direction){		  
        case 0:
            lcddev.width = LCD_W;
            lcddev.height = LCD_H;		
            LCD_WriteReg(0x36, 0x00);
            break;
        case 1:
            lcddev.width = LCD_H;
            lcddev.height = LCD_W;
            LCD_WriteReg(0x36, 0x60);
            break;
        case 2:
            lcddev.width = LCD_W;
            lcddev.height = LCD_H;	
            LCD_WriteReg(0x36, 0xC0);
            break;
        case 3:
            lcddev.width = LCD_H;
            lcddev.height = LCD_W;
            LCD_WriteReg(0x36, 0xA0);
            break;	
        default:
            break;
    }		
}	 

static void LCD_RESET(void)
{
	LCD_RST_CLR;
	goldie_msleep(100);
	LCD_RST_SET;
	goldie_msleep(50);
}

static void lcd_gpio_init(void)
{
	goldie_gpio_setfunc(LCD_RS, 0);
	goldie_gpio_setdir(LCD_RS, GOLDIE_GPIO_DIRECTION_OUTPUT);
	goldie_gpio_setval(LCD_RS, 0);

	goldie_gpio_setfunc(LCD_CS, 0);
	goldie_gpio_setdir(LCD_CS, GOLDIE_GPIO_DIRECTION_OUTPUT);
	goldie_gpio_setval(LCD_CS, 1);

	goldie_gpio_setfunc(LCD_CS1, 0);
	goldie_gpio_setdir(LCD_CS1, GOLDIE_GPIO_DIRECTION_OUTPUT);
	goldie_gpio_setval(LCD_CS1, 1);

#ifdef LCM_USE_EXT_GPIO
	init_ext_gpio();
#else
	goldie_gpio_setfunc(LCD_RST, 0);
	goldie_gpio_setdir(LCD_RST, GOLDIE_GPIO_DIRECTION_OUTPUT);
	goldie_gpio_setval(LCD_RST, 0);
	goldie_gpio_setfunc(LED, 0);
	goldie_gpio_setdir(LED, GOLDIE_GPIO_DIRECTION_OUTPUT);
	goldie_gpio_setval(LED, 0);
#endif
}

// ========== GC9D01 初始化 ==========
void LCD_Init(void)
{  
	printf("GC9D01 LCD_Init start\r\n");
	
	lcd_gpio_init(); 
	
	// 复位时序
	LCD_RST_SET;
	goldie_msleep(200);
	LCD_RST_CLR;
	goldie_msleep(200);
	LCD_RST_SET;
	goldie_msleep(200);
	goldie_msleep(500);

	LCD_RESET(); // LCD reset

	// GC9D01 初始化命令序列
	LCD_WR_REG(0xFE);
	LCD_WR_REG(0xEF);

	LCD_WR_REG(0x80); LCD_WR_DATA(0xFF);
	LCD_WR_REG(0x81); LCD_WR_DATA(0xFF);
	LCD_WR_REG(0x82); LCD_WR_DATA(0xFF);
	LCD_WR_REG(0x83); LCD_WR_DATA(0xFF);
	LCD_WR_REG(0x84); LCD_WR_DATA(0xFF);
	LCD_WR_REG(0x85); LCD_WR_DATA(0xFF);
	LCD_WR_REG(0x86); LCD_WR_DATA(0xFF);
	LCD_WR_REG(0x87); LCD_WR_DATA(0xFF);
	LCD_WR_REG(0x88); LCD_WR_DATA(0xFF);
	LCD_WR_REG(0x89); LCD_WR_DATA(0xFF);
	LCD_WR_REG(0x8A); LCD_WR_DATA(0xFF);
	LCD_WR_REG(0x8B); LCD_WR_DATA(0xFF);
	LCD_WR_REG(0x8C); LCD_WR_DATA(0xFF);
	LCD_WR_REG(0x8D); LCD_WR_DATA(0xFF);
	LCD_WR_REG(0x8E); LCD_WR_DATA(0xFF);
	LCD_WR_REG(0x8F); LCD_WR_DATA(0xFF);

	LCD_WR_REG(0x3A); LCD_WR_DATA(0x05);
	LCD_WR_REG(0xEC); LCD_WR_DATA(0x01);

	LCD_WR_REG(0x74);
	LCD_WR_DATA(0x02); LCD_WR_DATA(0x0E); LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x00); LCD_WR_DATA(0x00); LCD_WR_DATA(0x00); LCD_WR_DATA(0x00);

	LCD_WR_REG(0x98); LCD_WR_DATA(0x3E);
	LCD_WR_REG(0x99); LCD_WR_DATA(0x3E);

	LCD_WR_REG(0xB5); LCD_WR_DATA(0x0D); LCD_WR_DATA(0x0D);

	LCD_WR_REG(0x60); LCD_WR_DATA(0x38); LCD_WR_DATA(0x0F); LCD_WR_DATA(0x79); LCD_WR_DATA(0x67);
	LCD_WR_REG(0x61); LCD_WR_DATA(0x38); LCD_WR_DATA(0x11); LCD_WR_DATA(0x79); LCD_WR_DATA(0x67);

	LCD_WR_REG(0x64);
	LCD_WR_DATA(0x38); LCD_WR_DATA(0x17); LCD_WR_DATA(0x71);
	LCD_WR_DATA(0x5F); LCD_WR_DATA(0x79); LCD_WR_DATA(0x67);

	LCD_WR_REG(0x65);
	LCD_WR_DATA(0x38); LCD_WR_DATA(0x13); LCD_WR_DATA(0x71);
	LCD_WR_DATA(0x5B); LCD_WR_DATA(0x79); LCD_WR_DATA(0x67);

	LCD_WR_REG(0x6A); LCD_WR_DATA(0x00); LCD_WR_DATA(0x00);

	LCD_WR_REG(0x6C);
	LCD_WR_DATA(0x22); LCD_WR_DATA(0x02); LCD_WR_DATA(0x22);
	LCD_WR_DATA(0x02); LCD_WR_DATA(0x22); LCD_WR_DATA(0x22); LCD_WR_DATA(0x50);

	LCD_WR_REG(0x6E);
	LCD_WR_DATA(0x03); LCD_WR_DATA(0x03); LCD_WR_DATA(0x01); LCD_WR_DATA(0x01);
	LCD_WR_DATA(0x00); LCD_WR_DATA(0x00); LCD_WR_DATA(0x0f); LCD_WR_DATA(0x0f);
	LCD_WR_DATA(0x0d); LCD_WR_DATA(0x0d); LCD_WR_DATA(0x0b); LCD_WR_DATA(0x0b);
	LCD_WR_DATA(0x09); LCD_WR_DATA(0x09); LCD_WR_DATA(0x00); LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x00); LCD_WR_DATA(0x00); LCD_WR_DATA(0x0a); LCD_WR_DATA(0x0a);
	LCD_WR_DATA(0x0c); LCD_WR_DATA(0x0c); LCD_WR_DATA(0x0e); LCD_WR_DATA(0x0e);
	LCD_WR_DATA(0x10); LCD_WR_DATA(0x10); LCD_WR_DATA(0x00); LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x02); LCD_WR_DATA(0x02); LCD_WR_DATA(0x04); LCD_WR_DATA(0x04);

	LCD_WR_REG(0xbf); LCD_WR_DATA(0x01);
	LCD_WR_REG(0xF9); LCD_WR_DATA(0x40);
	LCD_WR_REG(0x9b); LCD_WR_DATA(0x3b);
	LCD_WR_REG(0x93); LCD_WR_DATA(0x33); LCD_WR_DATA(0x7f); LCD_WR_DATA(0x00);
	LCD_WR_REG(0x7E); LCD_WR_DATA(0x30);

	LCD_WR_REG(0x70);
	LCD_WR_DATA(0x0d); LCD_WR_DATA(0x02); LCD_WR_DATA(0x08);
	LCD_WR_DATA(0x0d); LCD_WR_DATA(0x02); LCD_WR_DATA(0x08);

	LCD_WR_REG(0x71); LCD_WR_DATA(0x0d); LCD_WR_DATA(0x02); LCD_WR_DATA(0x08);
	LCD_WR_REG(0x91); LCD_WR_DATA(0x0E); LCD_WR_DATA(0x09);

	LCD_WR_REG(0xc3); LCD_WR_DATA(0x18);
	LCD_WR_REG(0xc4); LCD_WR_DATA(0x18);
	LCD_WR_REG(0xc9); LCD_WR_DATA(0x3c);

	LCD_WR_REG(0xf0); LCD_WR_DATA(0x13); LCD_WR_DATA(0x15); LCD_WR_DATA(0x04);
	LCD_WR_DATA(0x05); LCD_WR_DATA(0x01); LCD_WR_DATA(0x38);

	LCD_WR_REG(0xf2); LCD_WR_DATA(0x13); LCD_WR_DATA(0x15); LCD_WR_DATA(0x04);
	LCD_WR_DATA(0x05); LCD_WR_DATA(0x01); LCD_WR_DATA(0x34);

	LCD_WR_REG(0xf1); LCD_WR_DATA(0x4b); LCD_WR_DATA(0xb8); LCD_WR_DATA(0x7b);
	LCD_WR_DATA(0x34); LCD_WR_DATA(0x35); LCD_WR_DATA(0xef);

	LCD_WR_REG(0xf3); LCD_WR_DATA(0x47); LCD_WR_DATA(0xb4); LCD_WR_DATA(0x72);
	LCD_WR_DATA(0x34); LCD_WR_DATA(0x35); LCD_WR_DATA(0xda);

	LCD_WR_REG(0x36); LCD_WR_DATA(0x00);

	LCD_WR_REG(0x11);  // Sleep out
	goldie_msleep(200);
	
	LCD_WR_REG(0x29);  // Display on
	goldie_msleep(100);

  	LCD_direction(USE_HORIZONTAL);
	init_flag = 1;
	printf("GC9D01 LCD_Init finish\r\n");
}

void lcd_update_window_rgb565(uint16_t x0, uint16_t y0, int x1, int y1, char *frame_buffer_RBG565)
{
	LCD_SetWindows_2(x0, y0, x1, y1);
	LCD_WriteRAM_Prepare();	
	cs_ctrl_clr(lcm_id);
	LCD_RS_SET;
	spi_drv->spi_write(frame_buffer_RBG565, (x1 - x0 + 1) * (y1 - y0 + 1) * 2);
	cs_ctrl_set(lcm_id);	
}

static void lcd_lock_region(unsigned int x0, unsigned int y0, unsigned int w, unsigned int h)
{
	goldie_mutex_lock(&display_window_mutex);
	//printf("X:%d,Y:%d,W:%d,H:%d\n",x0,y0,w,h);
	if(x0 == 0 && y0 == 0 && w == 160 && h == 160){
		if(g_dual_screen_done == 1){
			g_dual_screen_done = 0;
			lcm_id = 2;
		}else{
			lcm_id = 0;
		}
	}else if(x0 == 160 && y0 == 0 && w == 160 && h == 160){
		if(g_dual_screen_done == 1){
			g_dual_screen_done = 0;
			lcm_id = 2;
		}else{
			lcm_id = 1;
		}
	}
	else if(x0 == 0 && y0 == 0 && w == 320 && h == 160){
			g_dual_screen_done = 1;
			lcm_id = 2;	
	}
	LCD_SetWindows_2(0, 0, 160-1, 160-1);
	LCD_WriteRAM_Prepare(); 
	cs_ctrl_clr(lcm_id);
	LCD_RS_SET;	
}

static void lcd_update_region(void)
{	
	cs_ctrl_set(lcm_id);
	goldie_mutex_unlock(&display_window_mutex);
	return;
}

static void lcd_fill_pixels(uint16_t* buffer, uint32_t size)
{
	if(!spi_drv){
		return;
	}
	if(size > 2040)
	{
		printf("[lcd_error] size is too big \r\n");
		size = 2040;
	}
	spi_drv->spi_write((uint8_t*)buffer, size * 2);
	return;
}

static void lcd_backlight(int onoff)
{
	if(onoff){
		LCD_LED_ON;
	}else{
		LCD_LED_OFF;
	}
}

static void LCD_Fill_Color(uint16_t color)
{  	
	int pixes_len = SAFETY_FB_SIZE / 2;
	int tolta_len = lcddev.width * lcddev.height * 2;
	int loop = tolta_len / SAFETY_FB_SIZE;
	int left = tolta_len % SAFETY_FB_SIZE;
	uint16_t * uint16_buffer = (uint16_t*)fb_buffer;
	
	if(!spi_drv) {
		return;
	}
	
	LCD_SetWindows(0, 0, lcddev.width - 1, lcddev.height - 1);
	cs_ctrl_clr(lcm_id);
	LCD_RS_SET;
	
	for(int i = 0; i < pixes_len; i++) {
		uint16_buffer[i] = color;
	}
	
	for(int i = 0; i < loop; i++) {
		spi_drv->spi_write((uint8_t*)uint16_buffer, pixes_len * 2);
	}
	
	if(left > 0) {
		spi_drv->spi_write((uint8_t*)uint16_buffer, left);
	}
	
	cs_ctrl_set(lcm_id);  
}

static int get_framebuffer(char** fb)
{
	*fb = (char*)fb_buffer;
	return SAFETY_FB_SIZE;
}

static DispDrv gc9d01_drv = {
	.lock_region = lcd_lock_region,
	.update_region = lcd_update_region,
	.fill_pixels = lcd_fill_pixels,
	.set_backlight = lcd_backlight,
	.get_framebuffer = get_framebuffer,
};

static void disp_drv_init(void)
{
	printf("disp_drv_init GC9D01 +++++++++++++\r\n");
	goldie_mutex_init(&display_drv_mutex);
	goldie_mutex_init(&display_window_mutex);
	
	spi_drv = (SpiDrv*)wait_drv(SPI1_DRV_INDEX);
	spi_drv->spi_init();
	
	LCD_Init();
	
	lcd_backlight(0);
	lcm_id = 0;
	LCD_Fill_Color(0);
	LCD_Fill_Color(0xFFFF); 
	lcm_id = 1;
	LCD_Fill_Color(0);
	LCD_Fill_Color(0xFFFF); 
	lcm_id = 0;
	
	lcd_backlight(1);
	
#ifdef SUPPORT_FT6636_TOUCH
	ft6636_entry();
#else
#ifdef SUPPORT_CST816D_TOUCH
	cst816d_entry();
#endif
#endif
	
	register_drv(DISP_DRV_INDEX, (void*)(&gc9d01_drv));
	return;
}

static void disp_drv_entry(void)
{
	goldie_thread_lock();
	task_handle = goldie_thread_create(
		(goldie_thread_handler)disp_drv_init, 
		NULL, 
		"disp_drv_init",
		0x500);

	if(task_handle){
		goldie_thread_set_priority(task_handle, 24);
	}
	goldie_thread_unlock();
}

GOLDIE_INIT_CALL_(disp_drv_entry);