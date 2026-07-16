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
        gpio_ext_drv->set_gpio_func(RST_EXT_GPIO,GOLDIE_GPIO_DIRECTION_OUTPUT);
       gpio_ext_drv->set_gpio_func(LED_EXT_GPIO,GOLDIE_GPIO_DIRECTION_OUTPUT);
       gpio_ext_drv->set_gpio_value(RST_EXT_GPIO,0);
      gpio_ext_drv->set_gpio_value(LED_EXT_GPIO,0);
}

static void ext_gpio_rst_set(int value)
{
	if(value ==0){
		gpio_ext_drv->set_gpio_value(RST_EXT_GPIO,0);
	}else{
		gpio_ext_drv->set_gpio_value(RST_EXT_GPIO,1);
	}
}

static void ext_gpio_led_set(int value)
{
	if(value ==0){
		gpio_ext_drv->set_gpio_value(LED_EXT_GPIO,1);
	}else{
		gpio_ext_drv->set_gpio_value(LED_EXT_GPIO,0);
	}
}

#define  LCD_LED_ON         ext_gpio_led_set(1)
#define  LCD_LED_OFF        ext_gpio_led_set(0)

#define	LCD_RST_SET     ext_gpio_rst_set(1)
#define	LCD_RST_CLR     ext_gpio_rst_set(0)
#endif

static SpiDrv * spi_drv = 0;
#define FB_SIZE  4096
#define SAFETY_FB_SIZE  (FB_SIZE-16)  //return to user space
static uint32_t fb_buffer[FB_SIZE/4]; 
static goldie_mutex display_window_mutex;
static goldie_mutex display_drv_mutex;
static void * task_handle = 0;

_lcd_dev lcddev;
/*****************************************************************************
 * @name       :void LCD_WR_REG(uint8_t data)
 * @date       :2018-08-09 
 * @function   :Write an 8-bit command to the LCD screen
 * @parameters :data:Command value to be written
 * @retvalue   :None
******************************************************************************/
static void LCD_WR_REG(uint8_t data)
{ 
  	LCD_CS_CLR;     
	LCD_RS_CLR;	  
	spi_drv->spi_write(&data,1);
 	LCD_CS_SET;
}

/*****************************************************************************
 * @name       :void LCD_WR_DATA(uint8_t data)
 * @date       :2018-08-09 
 * @function   :Write an 8-bit data to the LCD screen
 * @parameters :data:data value to be written
 * @retvalue   :None
******************************************************************************/
static void LCD_WR_DATA(uint8_t data)
{
	LCD_CS_CLR;
	LCD_RS_SET;
	spi_drv->spi_write(&data,1);
	LCD_CS_SET;
}

static void LCD_WriteReg(uint8_t LCD_Reg, uint16_t LCD_RegValue)
{	
	LCD_WR_REG(LCD_Reg);  
	LCD_WR_DATA(LCD_RegValue);	    		 
}	   

/*****************************************************************************
 * @name       :void LCD_WriteRAM_Prepare(void)
 * @date       :2018-08-09 
 * @function   :Write GRAM
 * @parameters :None
 * @retvalue   :None
******************************************************************************/	 
static void LCD_WriteRAM_Prepare(void)
{
	LCD_WR_REG(lcddev.wramcmd);
}	 

/*****************************************************************************
 * @name       :void LCD_SetWindows(uint16_t xStar, uint16_t yStar,uint16_t xEnd,uint16_t yEnd)
 * @date       :2018-08-09 
 * @function   :Setting LCD display window
 * @parameters :xStar:the bebinning x coordinate of the LCD display window
								yStar:the bebinning y coordinate of the LCD display window
								xEnd:the endning x coordinate of the LCD display window
								yEnd:the endning y coordinate of the LCD display window
 * @retvalue   :None
******************************************************************************/ 
static void LCD_SetWindows(uint16_t xStar, uint16_t yStar,uint16_t xEnd,uint16_t yEnd)
{	
	LCD_WR_REG(lcddev.setxcmd);	
	LCD_WR_DATA(xStar>>8);
	LCD_WR_DATA(0x00FF&xStar);		
	LCD_WR_DATA(xEnd>>8);
	LCD_WR_DATA(0x00FF&xEnd);

	LCD_WR_REG(lcddev.setycmd);	
	LCD_WR_DATA(yStar>>8);
	LCD_WR_DATA(0x00FF&yStar);		
	LCD_WR_DATA(yEnd>>8);
	LCD_WR_DATA(0x00FF&yEnd);

	LCD_WriteRAM_Prepare();			
}   

static void LCD_SetWindows_2(uint16_t xStar, uint16_t yStar,uint16_t xEnd,uint16_t yEnd)
{	
	LCD_WR_REG(lcddev.setxcmd);	
	LCD_WR_DATA(xStar>>8);
	LCD_WR_DATA(0x00FF&xStar);		
	LCD_WR_DATA(xEnd>>8);
	LCD_WR_DATA(0x00FF&xEnd);

	LCD_WR_REG(lcddev.setycmd);	
	LCD_WR_DATA(yStar>>8);
	LCD_WR_DATA(0x00FF&yStar);		
	LCD_WR_DATA(yEnd>>8);
	LCD_WR_DATA(0x00FF&yEnd);
} 

/*****************************************************************************
 * @name       :void LCD_SetCursor(uint16_t Xpos, uint16_t Ypos)
 * @date       :2018-08-09 
 * @function   :Set coordinate value
 * @parameters :Xpos:the  x coordinate of the pixel
								Ypos:the  y coordinate of the pixel
 * @retvalue   :None
******************************************************************************/ 
static void LCD_SetCursor_2(uint16_t Xpos, uint16_t Ypos)
{	  	    			
	LCD_WR_REG(lcddev.setxcmd); 
	LCD_WR_DATA(Xpos>>8);
	LCD_WR_DATA(0x00FF&Xpos);		
	
	LCD_WR_REG(lcddev.setycmd); 
	LCD_WR_DATA(Ypos>>8);
	LCD_WR_DATA(0x00FF&Ypos);		
	LCD_WriteRAM_Prepare(); 
} 

/*****************************************************************************
 * @name       :void LCD_direction(uint8_t direction)
 * @date       :2018-08-09 
 * @function   :Setting the display direction of LCD screen
 * @parameters :direction:0-0 degree
                          1-90 degree
													2-180 degree
													3-270 degree
 * @retvalue   :None
******************************************************************************/ 
static void LCD_direction(uint8_t direction)
{ 
	lcddev.setxcmd=0x2A;
	lcddev.setycmd=0x2B;
	lcddev.wramcmd=0x2C;
    
    switch(direction){		  
        case 0:	// 0 degree					 	 		
            lcddev.width=LCD_W;
            lcddev.height=LCD_H;		
            LCD_WriteReg(0x36, 0x00);  // MADCTL: MY=0, MX=0, MV=0, ML=0, RGB=0, MH=0
            break;
        case 1:	// 90 degree
            lcddev.width=LCD_H;
            lcddev.height=LCD_W;
            LCD_WriteReg(0x36, 0x60);  // MADCTL: MY=0, MX=0, MV=1, ML=0, RGB=0, MH=0
            break;
        case 2:	// 180 degree					 	 		
            lcddev.width=LCD_W;
            lcddev.height=LCD_H;	
            LCD_WriteReg(0x36, 0xC0);  // MADCTL: MY=1, MX=1, MV=0, ML=0, RGB=0, MH=0
            break;
        case 3:	// 270 degree
            lcddev.width=LCD_H;
            lcddev.height=LCD_W;
            LCD_WriteReg(0x36, 0xA0);  // MADCTL: MY=1, MX=0, MV=1, ML=0, RGB=0, MH=0
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
	goldie_gpio_setfunc(LCD_RS,0);
	goldie_gpio_setdir(LCD_RS, GOLDIE_GPIO_DIRECTION_OUTPUT);
	goldie_gpio_setval(LCD_RS, 0);

#ifdef LCM_USE_EXT_GPIO
init_ext_gpio();
#else
goldie_gpio_setfunc(LCD_RST,0);
goldie_gpio_setdir(LCD_RST, GOLDIE_GPIO_DIRECTION_OUTPUT);
goldie_gpio_setval(LCD_RST, 0);

goldie_gpio_setfunc(LED,0);
goldie_gpio_setdir(LED, GOLDIE_GPIO_DIRECTION_OUTPUT);
goldie_gpio_setval(LED, 0);

#endif
}
	
void LCD_Init(void)
{  
	printf("ST7789 LCD_Init start\r\n");
	lcd_gpio_init(); 
	LCD_RST_SET;
	goldie_msleep(200);
	LCD_RST_CLR;
	goldie_msleep(200);
	LCD_RST_SET;
	goldie_msleep(200);
	goldie_msleep(500);

	LCD_RESET(); //LCD reset

	// ST7789 initialization sequence
	LCD_WR_REG(0x01);  // SWRESET: Software reset
	goldie_msleep(150);

	LCD_WR_REG(0x11);  // SLPOUT: Sleep out
	goldie_msleep(500);

	LCD_WR_REG(0x3A);  // COLMOD: Interface Pixel Format
	LCD_WR_DATA(0x55); // 16-bit/pixel (RGB565)

	LCD_WR_REG(0x36);  // MADCTL: Memory Data Access Control
	LCD_WR_DATA(0x00); // MY=0, MX=0, MV=0, ML=0, RGB=0, MH=0

	LCD_WR_REG(0xB2);  // PORCTRL: Porch setting
	LCD_WR_DATA(0x0C);
	LCD_WR_DATA(0x0C);
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x33);
	LCD_WR_DATA(0x33);

	LCD_WR_REG(0xB7);  // GCTRL: Gate Control
	LCD_WR_DATA(0x35); 

	LCD_WR_REG(0xBB);  // VCOMS: VCOM setting
	LCD_WR_DATA(0x2B); 

	LCD_WR_REG(0xC0);  // LCMCTRL: LCM Control
	LCD_WR_DATA(0x2C); 

	LCD_WR_REG(0xC2);  // VDVVRHEN: VDV and VRH Command Enable
	LCD_WR_DATA(0x01); 

	LCD_WR_REG(0xC3);  // VRHS: VRH Set
	LCD_WR_DATA(0x0F); 

	LCD_WR_REG(0xC4);  // VDVS: VDV Set
	LCD_WR_DATA(0x20); 

	LCD_WR_REG(0xC6);  // FRCTRL2: Frame Rate Control in Normal Mode
	LCD_WR_DATA(0x0F); 

	LCD_WR_REG(0xD0);  // PWCTRL1: Power Control 1
	LCD_WR_DATA(0xA4);
	LCD_WR_DATA(0xA1);

	LCD_WR_REG(0xE0);  // PVGAMCTRL: Positive Voltage Gamma Control
	LCD_WR_DATA(0xD0);
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x02);
	LCD_WR_DATA(0x07);
	LCD_WR_DATA(0x0A);
	LCD_WR_DATA(0x28);
	LCD_WR_DATA(0x32);
	LCD_WR_DATA(0x44);
	LCD_WR_DATA(0x42);
	LCD_WR_DATA(0x06);
	LCD_WR_DATA(0x0E);
	LCD_WR_DATA(0x12);
	LCD_WR_DATA(0x14);
	LCD_WR_DATA(0x17);

	LCD_WR_REG(0xE1);  // NVGAMCTRL: Negative Voltage Gamma Control
	LCD_WR_DATA(0xD0);
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x02);
	LCD_WR_DATA(0x07);
	LCD_WR_DATA(0x0A);
	LCD_WR_DATA(0x28);
	LCD_WR_DATA(0x31);
	LCD_WR_DATA(0x54);
	LCD_WR_DATA(0x47);
	LCD_WR_DATA(0x0E);
	LCD_WR_DATA(0x1C);
	LCD_WR_DATA(0x17);
	LCD_WR_DATA(0x1B);
	LCD_WR_DATA(0x1E);

	// Set display area (adjust according to your ST7789 resolution)
	LCD_WR_REG(0x2A);  // CASET: Column Address Set
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0xEF);  // 240 columns (0x00EF = 239)

	LCD_WR_REG(0x2B);  // RASET: Row Address Set
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x01);
	LCD_WR_DATA(0x3F);  // 320 rows (0x013F = 319)

	LCD_WR_REG(0x21);  // INVON: Display Inversion On
	LCD_WR_REG(0x29);  // DISPON: Display On
	goldie_msleep(100);

  	LCD_direction(USE_HORIZONTAL);
	printf("ST7789 LCD_Screen_Init finish\r\n");
}

void lcd_update_window_rgb565(uint16_t x0, uint16_t y0,int x1, int y1,char *frame_buffer_RBG565){
	LCD_SetWindows_2(x0,y0,x1,y1);
	LCD_WriteRAM_Prepare();	
	LCD_CS_CLR;
	LCD_RS_SET;
	spi_drv->spi_write(frame_buffer_RBG565,(x1-x0+1)*(y1-y0+1)*2);
	LCD_CS_SET;	
};

static void lcd_lock_region(unsigned int x0,unsigned int y0,unsigned int w,unsigned int h)
{
	goldie_mutex_lock(&display_window_mutex);
	LCD_SetWindows_2(x0,y0,x0+w-1,y0+h-1);
	LCD_WriteRAM_Prepare(); 
	LCD_CS_CLR;
	LCD_RS_SET;	
}

static void lcd_update_region(void){	
	LCD_CS_SET;
	goldie_mutex_unlock(&display_window_mutex);
	return;
}

static void lcd_fill_pixels(uint16_t* buffer,uint32_t size){
	if(!spi_drv){
		return;
	}
	if(size>2040)
	{
		printf("error------------------size is too big \r\n");
		size=2040;
	}
	//goldie_mutex_lock(&display_drv_mutex);
	spi_drv->spi_write(buffer,size*2);
	//goldie_mutex_unlock(&display_drv_mutex);	
	return;
}

static void lcd_backlight(int onoff)
{
	//goldie_mutex_lock(&display_drv_mutex);
	if(onoff){
		LCD_LED_ON;
	}else{
		LCD_LED_OFF;
	}
	//goldie_mutex_unlock(&display_drv_mutex);	
}

static void LCD_Fill_Color(uint16_t color)
{  	
	int pixes_len = SAFETY_FB_SIZE/2;
	int tolta_len  = lcddev.width * lcddev.height*2;
	int loop = tolta_len /SAFETY_FB_SIZE;
	int left = tolta_len % SAFETY_FB_SIZE;
	uint16_t * uint16_buffer = (uint16_t*)fb_buffer;
	if(!spi_drv)
	{
		return 0;
	}
	
	LCD_SetWindows(0,0,lcddev.width-1,lcddev.height-1);
	LCD_CS_CLR;
	LCD_RS_SET;
	for(int i =0;i<pixes_len;i++)
	{
		uint16_buffer[i] = color;
	}
	
	for(int i =0;i<loop;i++)
	{
		spi_drv->spi_write((uint8_t*)uint16_buffer,pixes_len*2);
	}
	spi_drv->spi_write((uint8_t*)uint16_buffer,left);	
	LCD_CS_SET;  
}

static int get_framebuffer(char** fb)
{
	//goldie_mutex_lock(&display_drv_mutex);
	*fb = fb_buffer;
	//goldie_mutex_unlock(&display_drv_mutex);	
	return SAFETY_FB_SIZE;
}

static DispDrv mdsip_drv = {
	.lock_region = lcd_lock_region,
	.update_region = lcd_update_region,
	.fill_pixels = lcd_fill_pixels,
	.set_backlight = lcd_backlight,
	.get_framebuffer = get_framebuffer,
};

static void disp_drv_init(void)
{
	printf("disp_drv_init st7789 +++++++++++++\r\n");
	goldie_mutex_init(&display_drv_mutex);
	goldie_mutex_init(&display_window_mutex);
	spi_drv = (SpiDrv*)wait_drv(SPI1_DRV_INDEX);
	spi_drv->spi_init();
	LCD_Init();
	lcd_backlight(0);
	LCD_Fill_Color(0);
	lcd_update_window_rgb565(BOOT_PIC_START_X,BOOT_PIC_START_Y,BOOT_PIC_END_X-1,BOOT_PIC_END_Y-1,gImage_boot_pic);
	lcd_backlight(1);	
#ifdef SUPPORT_FT6636_TOUCH
ft6636_entry();
#else
#ifdef SUPPORT_CST816D_TOUCH
	cst816d_entry();
#endif
#endif	
	register_drv(DISP_DRV_INDEX, (void*)(&mdsip_drv));
   	 return;
}

static void disp_drv_entry(void){
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

