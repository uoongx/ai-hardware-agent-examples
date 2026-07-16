#pragma once
#include <memory>
#include <cstring>
#include "prj_config.h"
extern "C"
{	
	#include "driver_manager.h"
}

class RenderService {
public:
    RenderService();   
    void lock_region(int start_x, int start_y, int width, int height);
    void update_region(void);
	int get_framebuffer(char** fb);
    void fill_pixels(uint16_t* buffer, uint32_t count);
    void init_Default_Display();
	void setDisplayDriver(struct DispDrv* display_drv);
	
   static void char_to_rgb565(uint8_t input, uint32_t* output);
   static bool gettextpixes8(const uint8_t* rgb2data, int x, int y, int buffer_width,uint32_t* output);
   static uint8_t* allocateTextBuffer(int width, int height);
   static  void freeTextBuffer(uint8_t* buffer);
   static int enlargeText(const uint8_t* original_textbuffer, int w, int h, int scale, 
                     int* new_w, int* new_h,uint8_t* new_textbuffer);
   static unsigned int str_display_width(const char *str);
   static  uint32_t get_unicode_from_utf8(const uint8_t **p);
   static  int  find_font_index(uint32_t unicode, const uint32_t * font_index,int font_index_count);
   static  void filltextpixes(uint8_t* rgb2data, uint16_t rgb565_data, int x, int y,int buffer_width);
   static  void drawtext2buffer(const char *utf8buffer,uint8_t * pixel_buffer,
   unsigned int sx, unsigned int sy,int buffer_width, int buffer_height,uint16_t text_color);
       // Boundary table processing functions
 static   uint16_t blendColors(uint16_t fg_color, uint16_t bg_color, float acc,int distance);
 static   void processBoundaryPixels(int x, int y, int width,int height,uint32_t* fg_buffer, uint32_t* bg_buffer,const uint16_t* boundary_buffer);

private:
    DispDrv* mdisp_drver = nullptr;
};
