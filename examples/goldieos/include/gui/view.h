#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include "render_service.h"
#include "prj_config.h"
#define FONT_HEIGHT    16
#define HALF_FONT_WIDTH     8
#define RGB565_swap(r,g,b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
#define RGB565(r,g,b) (((RGB565_swap(r,g,b)&0xff)<<8)|((RGB565_swap(r,g,b)&0xff00)>>8))

#define TRANSPARENT_COLOR 0xFFFF
#define TOUCH_EVENT_UP  0X0
#define TOUCH_EVENT_DOWN  0X1
#define TOUCH_EVENT_CLICK  0X2

enum{
	COLOR_GRADIENT_NONE = 0,
	COLOR_GRADIENT_HORIZONTAL,
	COLOR_GRADIENT_VERTICAL
};

enum{
	VIEW_SMALL_FONT=0,
	VIEW_BIG_FONT,
	VIEW_MAX_FONT,
};
#define BIG_FONT_SCALE  2

// View type identifiers for safe type checking without RTTI
enum ViewType {
    VIEW_TYPE_BASE = 0,
    VIEW_TYPE_WINDOW = 1,
    VIEW_TYPE_FRAME = 2,
    VIEW_TYPE_LABEL = 3,
    VIEW_TYPE_BUTTON = 4,
    VIEW_TYPE_LIST = 5,
    VIEW_TYPE_SPINNER = 6,
    VIEW_TYPE_DATETIME_PICKER = 7,
    VIEW_TYPE_PROGRESSBAR = 8,
    VIEW_TYPE_TEXTEDIT = 9,
    VIEW_TYPE_MSGBOX = 10,
    VIEW_TYPE_INPUTMETHOD = 11,
    VIEW_TYPE_CHECKBOX = 12,
    VIEW_TYPE_IMGBUTTON = 13,
};

class View {
public:
    View(int x, int y, int width, int height);
	View(int width, int height);
    virtual ~View();

	void setParent(const std::shared_ptr<View>& parent);
	std::shared_ptr<View> getParent() const { return parent_; }
    
    // Type identification for safe type checking without RTTI
    virtual ViewType getViewType() const { return VIEW_TYPE_BASE; }
    
    // Position and size management
    void* getId(){return viewid_ptr;};
    void setId(void* id) { viewid_ptr = id; }
    void setPosition(int x, int y);
    // Position and size getters
    int getX() const { return x_; }
    int getY() const { return y_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    virtual void setSize(int width, int height);
    void setVisible(bool visible);
    
    // Buffer management
    virtual void setImageBuffer(const uint16_t* buffer);
    virtual void setColor(uint16_t color);
    virtual void setTextColor(uint16_t wb_Color);
    virtual void setText(const char* text);
    virtual void setTransparent(bool transp){transparent = transp;};
    virtual void  setFontSize(int size){font_size = size;};
    virtual void setText(const char* text,int sx,int sy);
    void appendText(const char* text,int sx,int sy);
    virtual bool isVisible(){return visible_;}
    virtual void setSlected(bool slected) ;	

    virtual void setMaskBuffer(const uint8_t* buffer) ;
    virtual void setBoundaryBuffer(const uint16_t* buffer){boundary_buffer = buffer;};

    virtual void flush(int sx,int sy,int w,int h);
    // Pixel retrieval - implements the priority logic
    virtual int getPixel8(int x, int y,uint32_t* buffer);

    virtual int getSelfPixel8(int x, int y,uint32_t* buffer);
    // Event handling
    virtual bool handleEvent(int key,int eventX, int eventY);
    virtual void setColorGradient(int gtype){ color_gradient_type = gtype;};
    virtual void setGradientStart(uint16_t scolor);

    void Transparent_effect(int x, int y,uint32_t *buffer);
    void darkenColor8(uint32_t* color);
	
	

protected:
    int m_text_start_x = 0;
    int m_text_start_y = 0;
#ifdef PLATFORM_TYPE_WIN_CREATOR
    int font_size = VIEW_BIG_FONT;
#else
    int font_size = VIEW_SMALL_FONT;
#endif
    int big_font_w;
    int big_font_h;
    uint8_t* big_font_buffer = nullptr;
    int x_, y_;
    int width_, height_;
    bool visible_ = true;    
    bool transparent = false;
    std::string text_;
    // --- Lazy text rendering cache ---
    // We only store the string on setText(). Actual layout/buffer build happens lazily in getPixel8().
    bool text_dirty_ = false;
    bool text_manual_pos_ = false;
    int text_manual_sx_ = 0;
    int text_manual_sy_ = 0;

    // Allocated text buffer dimensions (stride/height for text_buffer_). Can be >= current width_/height_.
    int text_buf_w_ = 0;
    int text_buf_h_ = 0;

    // Cache key for skipping recompute when text properties unchanged
    uint32_t text_cache_hash_ = 0;

    void ensureTextPrepared_();
    // Buffer pointers (only one active between image and color)
    const uint16_t* image_buffer_ = nullptr;
    const uint8_t* mask_buffer_ = nullptr;
    const uint16_t* boundary_buffer = nullptr;

    uint16_t color_ = 0xFFFF;
    uint16_t Textcolor_ = 0x0;
    uint16_t start_color = 0xFFFF;
    unsigned char color_r = 0xff;
    unsigned char color_g = 0xff;
    unsigned char color_b = 0xff;

    unsigned char color_r_start = 0xff;
    unsigned char color_g_start = 0xff;
    unsigned char color_b_start = 0xff;

	float gradient_r = 0.0f;
	float gradient_g = 0.0f;
	float gradient_b = 0.0f;

    unsigned int textbuffer_size = 0;
    unsigned int big_textbuffer_size = 0;
	
    bool selected_ = false;
    int color_gradient_type = COLOR_GRADIENT_NONE;
    uint8_t* text_buffer_ = nullptr;
    void* viewid_ptr = NULL;
    std::shared_ptr<View> parent_;
};
