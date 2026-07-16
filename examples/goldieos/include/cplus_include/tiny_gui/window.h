#pragma once
#include <vector>
#include <memory>
#include "view.h"
#include "render_service.h"
#include "prj_config.h"
class Window : public View, public std::enable_shared_from_this<Window>{
public:
    Window(int width, int height);
	Window(int sx,int sy,int width, int height);
    Window(std::shared_ptr<RenderService> rs,int sx,int sy,int width, int height);
	
	// Type identification
	ViewType getViewType() const override { return VIEW_TYPE_WINDOW; }
	
	void addView(std::shared_ptr<View> view,int x,int y, int z_order = -1);
    void removeView(std::shared_ptr<View> view);
	void removeAllViews();
    void setViewZOrder(std::shared_ptr<View> view, int z_order);
	void setVisible(bool visible);
    // Event handling
    bool handleEvent(int key,int x, int y) override;    
	void flush(int start_x, int start_y, int width, int height) override;

    // Pixel retrieval for rendering
    uint16_t getPixel(int x, int y) const;
    int getPixel8(int x, int y,uint32_t* buffer) override;
private:
    uint32_t* pixel_buffer = nullptr; 
	uint32_t fb_size = 0;
    struct ViewEntry {
        std::shared_ptr<View> view;
        int z_order;
    };
    std::shared_ptr<RenderService> renderer;
    std::vector<ViewEntry> views_;	
	std::weak_ptr<View> first_touched_view;
	bool first_touched_ = false;
};
