#include "spi_lcd.h"
#include "driver_manager.h"
#include "system_res/pics/boot_pic.h"

static SpiDrv * spi_drv = 0;
#define FB_SIZE  4096
#define SAFETY_FB_SIZE  (FB_SIZE-16)  //return to user space
static uint32_t fb_buffer[FB_SIZE/4]; 
static goldie_mutex display_window_mutex;
static goldie_mutex display_drv_mutex;
static void * task_handle = 0;
static _lcd_dev lcddev;
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
    spi_drv->spi_write(&data, 1);
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
		case 0:						 	 		
			lcddev.width=LCD_W;
			lcddev.height=LCD_H;		
			LCD_WriteReg(0x36,(1<<3)|(0<<6)|(0<<7));//BGR==1,MY==0,MX==0,MV==0
		break;
		case 1:
			lcddev.width=LCD_H;
			lcddev.height=LCD_W;
			LCD_WriteReg(0x36,(1<<3)|(0<<7)|(1<<6)|(1<<5));//BGR==1,MY==1,MX==0,MV==1
		break;
		case 2:						 	 		
			lcddev.width=LCD_W;
			lcddev.height=LCD_H;	
			LCD_WriteReg(0x36,(1<<3)|(1<<6)|(1<<7));//BGR==1,MY==0,MX==0,MV==0
		break;
		case 3:
			lcddev.width=LCD_H;
			lcddev.height=LCD_W;
			LCD_WriteReg(0x36,(1<<3)|(1<<7)|(1<<5));//BGR==1,MY==1,MX==0,MV==1
		break;	
		default:break;
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

	goldie_gpio_setfunc(LCD_RST,0);
	goldie_gpio_setdir(LCD_RST, GOLDIE_GPIO_DIRECTION_OUTPUT);
	goldie_gpio_setval(LCD_RST, 0);

	goldie_gpio_setfunc(LED,0);
	goldie_gpio_setdir(LED, GOLDIE_GPIO_DIRECTION_INPUT);
	goldie_gpio_setval(LED, 0);
}
	
static void LCD_Init(void)
{  
	printf("LCD_Init start\r\n");
	lcd_gpio_init(); 
	LCD_RST_SET;
	goldie_msleep(200);
	LCD_RST_CLR;
	goldie_msleep(200);
	LCD_RST_SET;
	goldie_msleep(200);
	goldie_msleep(500);

	LCD_RESET(); //LCD 复位
//*************3.2inch ILI9341初始化**********//
	LCD_WR_REG(0xCF);  
	LCD_WR_DATA(0x00); 
	LCD_WR_DATA(0xD9); //C1 
	LCD_WR_DATA(0X30); 
	LCD_WR_REG(0xED);  
	LCD_WR_DATA(0x64); 
	LCD_WR_DATA(0x03); 
	LCD_WR_DATA(0X12); 
	LCD_WR_DATA(0X81); 
	LCD_WR_REG(0xE8);  
	LCD_WR_DATA(0x85); 
	LCD_WR_DATA(0x10); 
	LCD_WR_DATA(0x7A); 
	LCD_WR_REG(0xCB);  
	LCD_WR_DATA(0x39); 
	LCD_WR_DATA(0x2C); 
	LCD_WR_DATA(0x00); 
	LCD_WR_DATA(0x34); 
	LCD_WR_DATA(0x02); 
	LCD_WR_REG(0xF7);  
	LCD_WR_DATA(0x20); 
	LCD_WR_REG(0xEA);  
	LCD_WR_DATA(0x00); 
	LCD_WR_DATA(0x00); 
	LCD_WR_REG(0xC0);    //Power control 
	LCD_WR_DATA(0x1B);   //VRH[5:0] 
	LCD_WR_REG(0xC1);    //Power control 
	LCD_WR_DATA(0x12);   //SAP[2:0];BT[3:0] //0x01
	LCD_WR_REG(0xC5);    //VCM control 
	LCD_WR_DATA(0x26); 	 //3F
	LCD_WR_DATA(0x26); 	 //3C
	LCD_WR_REG(0xC7);    //VCM control2 
	LCD_WR_DATA(0XB0); 

	LCD_WR_REG(0x36);    // Memory Access Control 
	//LCD_WR_DATA(0x08); 
	LCD_WR_DATA(0x48);
	
	LCD_WR_REG(0x3A);   
	LCD_WR_DATA(0x55); 
	
	LCD_WR_REG(0xB1);   
	LCD_WR_DATA(0x00);   
	//LCD_WR_DATA(0x1A); 
	LCD_WR_DATA(0x10); 

	LCD_WR_REG(0xB6);    // Display Function Control 
	LCD_WR_DATA(0x0A); 
	LCD_WR_DATA(0xA2); 
	LCD_WR_REG(0xF2);    // 3Gamma Function Disable 
	LCD_WR_DATA(0x00); 
	LCD_WR_REG(0x26);    //Gamma curve selected 
	LCD_WR_DATA(0x01); 
	LCD_WR_REG(0xE0); //Set Gamma
	LCD_WR_DATA(0x1F);
	LCD_WR_DATA(0x24);
	LCD_WR_DATA(0x24);
	LCD_WR_DATA(0x0D);
	LCD_WR_DATA(0x12);
	LCD_WR_DATA(0x09);
	LCD_WR_DATA(0x52);
	LCD_WR_DATA(0xB7);
	LCD_WR_DATA(0x3F);
	LCD_WR_DATA(0x0C);
	LCD_WR_DATA(0x15);
	LCD_WR_DATA(0x06);
	LCD_WR_DATA(0x0E);
	LCD_WR_DATA(0x08);
	LCD_WR_DATA(0x00);
	LCD_WR_REG(0XE1); //Set Gamma
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x1B);
	LCD_WR_DATA(0x1B);
	LCD_WR_DATA(0x02);
	LCD_WR_DATA(0x0E);
	LCD_WR_DATA(0x06);
	LCD_WR_DATA(0x2E);
	LCD_WR_DATA(0x48);
	LCD_WR_DATA(0x3F);
	LCD_WR_DATA(0x03);
	LCD_WR_DATA(0x0A);
	LCD_WR_DATA(0x09);
	LCD_WR_DATA(0x31);
	LCD_WR_DATA(0x37);
	LCD_WR_DATA(0x1F);

	LCD_WR_REG(0x2B); 
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x01);
	LCD_WR_DATA(0x3f);
	LCD_WR_REG(0x2A); 
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0xef);	 
	LCD_WR_REG(0x11); //Exit Sleep
	goldie_msleep(120);// delay_ms(120);
	LCD_WR_REG(0x29); //display on		
	goldie_msleep(1200);// delay_ms(120);
  	LCD_direction(USE_HORIZONTAL);//
	//LCD_LED_ON;
	printf("LCD_Screen_Init finish\r\n");
}



static void lcd_update_window_rgb565(uint16_t x0, uint16_t y0,int x1, int y1,char *frame_buffer_RBG565){
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
	spi_drv->spi_write(buffer,size*2);
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
	if(task_handle){
		goldie_thread_destroy(task_handle);
		task_handle = 0;
	}
	*fb = fb_buffer;
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
	printf("disp_drv_init ili9341 +++++++++++++\r\n");
	goldie_mutex_init(&display_drv_mutex);
	goldie_mutex_init(&display_window_mutex);
	spi_drv = (SpiDrv*)wait_drv(SPI1_DRV_INDEX);
	spi_drv->spi_init();
	LCD_Init();
	lcd_backlight(0);
	LCD_Fill_Color(0);
	lcd_update_window_rgb565(BOOT_PIC_START_X,BOOT_PIC_START_Y,BOOT_PIC_END_X-1,BOOT_PIC_END_Y-1,gImage_boot_pic);
	lcd_backlight(1);
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
