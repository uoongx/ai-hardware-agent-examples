#pragma once
#include "label_view.h"
#include "view.h"
#include <functional>
#include "window.h"
#include "frame_view.h"
#include "prj_config.h"

class ButtonView : public LabelView {
public: 
    using ClickCallback = std::function<void(void*)>;
	ButtonView(int width, int height);
    // Click handling
    void setOnClick(ClickCallback callback);
    void setBottomLine(bool enable) {bottom_line_visible =enable; };
    void setTopLine(bool enable) {top_line_visible =enable; };
    void setLineColor(uint16_t color){ line_color = color;}; 
    // Overrides from View
    ViewType getViewType() const override { return VIEW_TYPE_BUTTON; }
    int getPixel8(int x, int y,uint32_t *buffer) override;
    bool handleEvent(int key,int eventX, int eventY) override;

private:
    bool pressed_ = false;
    bool bottom_line_visible = false;
    bool top_line_visible = false;
    uint16_t  line_color = 0x0;
    ClickCallback onClick_;
};
