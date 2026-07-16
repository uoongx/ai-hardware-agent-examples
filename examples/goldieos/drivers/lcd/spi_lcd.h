#ifndef SPI_LCD_H
#define SPI_LCD_H
#include "service_manager.h"
#include "goldie_osal.h"
#include "goldie_display.h"

#define BOOT_PIC_WIDTH DISPLAY_WIDTH
#define BOOT_PIC_HEIGHT 80
#define BOOT_PIC_START_X ((DISPLAY_WIDTH - BOOT_PIC_WIDTH)/2)
#define BOOT_PIC_END_X ((DISPLAY_WIDTH - BOOT_PIC_WIDTH)/2) + BOOT_PIC_WIDTH
#define BOOT_PIC_START_Y 20
#define BOOT_PIC_END_Y (BOOT_PIC_START_Y+BOOT_PIC_HEIGHT)
#define BOOT_ANIMATION_START_Y (BOOT_PIC_END_Y + 20)
#define BOOT_ANIMATION_END_Y (BOOT_ANIMATION_START_Y + 90)

typedef struct  
{							    
	uint16_t width;			
	uint16_t height;	
	uint16_t id;			
	uint8_t  dir;
	uint16_t  wramcmd;
	uint16_t  setxcmd;
	uint16_t  setycmd;
}_lcd_dev; 	


#ifdef GC9D01_SPI_LCD
//GC9D01 圆形屏
#define USE_HORIZONTAL 0
#define LCD_W 160
#define LCD_H 160
#define LCD_CS   14   //CS0
#define LCD_CS1   2   //CS1
#else
//st7789
#define USE_HORIZONTAL 1
#define LCD_W 240
#define LCD_H 320
#define LCD_CS   2   //CS
#endif



#ifdef LCM_USE_EXT_GPIO
#define RST_EXT_GPIO  12   //P1_4
#define LED_EXT_GPIO  10  //P1_2
#endif


/*extern uint16_t  POINT_COLOR;
   extern uint16_t  BACK_COLOR;
*/
#ifndef LCM_USE_EXT_GPIO
#define LED      3  //12  //back light ctrl
#endif

#define LCD_RS   0       //RS DC

#ifndef LCM_USE_EXT_GPIO
#define LCD_RST  14       //RST
#endif

#ifndef LCM_USE_EXT_GPIO
//basic hardware op
#define	LCD_LED_ON    goldie_gpio_setval(LED, 1);
#define	LCD_LED_OFF   goldie_gpio_setval(LED, 0);
#endif

//GPIO set high
#define	LCD_CS_SET    goldie_gpio_setval(LCD_CS, 1);  //uapi_gpio_set_val(CONFIG_SPI_CS_MASTER_PIN, GPIO_LEVEL_HIGH); //GPIO_TYPE->BSRR=1<<LCD_CS    //Ƭѡ�˿�  	PB11 do nothing follow with spi 
#define	LCD_RS_SET	goldie_gpio_setval(LCD_RS, 1);  //    uapi_gpio_set_val(LCD_RS, GPIO_LEVEL_HIGH); //GPIO_TYPE->BSRR=1<<LCD_RS    //����/����    PB10	  

#ifndef LCM_USE_EXT_GPIO
#define	LCD_RST_SET	goldie_gpio_setval(LCD_RST, 1);  //    uapi_gpio_set_val(LCD_RST, GPIO_LEVEL_HIGH); //GPIO_TYPE->BSRR=1<<LCD_RST   //��λ			PB12
#endif

//GPIO set low				    
#define	LCD_CS_CLR   goldie_gpio_setval(LCD_CS, 0);  //uapi_gpio_set_val(CONFIG_SPI_CS_MASTER_PIN, GPIO_LEVEL_LOW);//GPIO_TYPE->BRR=1<<LCD_CS     //Ƭѡ�˿�  	PB11 do nothing follow with spi 
#define	LCD_RS_CLR	goldie_gpio_setval(LCD_RS, 0);  //uapi_gpio_set_val(LCD_RS, GPIO_LEVEL_LOW);  //GPIO_TYPE->BRR=1<<LCD_RS     //����/����    PB10	 
#ifndef LCM_USE_EXT_GPIO
#define	LCD_RST_CLR	goldie_gpio_setval(LCD_RST, 0);  //uapi_gpio_set_val(LCD_RST, GPIO_LEVEL_LOW); //GPIO_TYPE->BRR=1<<LCD_RST    //��λ			PB12
#endif
#endif
