#pragma once
#include "view.h"
#include <functional>
#include "window.h"
#include "frame_view.h"
#include "prj_config.h"

class CheckboxView : public View {
public: 
    using ClickCallback = std::function<void(void*)>;
	CheckboxView(int width, int height);
    // Click handling
    void setOnClick(ClickCallback callback);
	void setChecked(int on_off);
	int isChecked();
    // Overrides from View
    ViewType getViewType() const override { return VIEW_TYPE_CHECKBOX; }
    int getPixel8(int x, int y,uint32_t *buffer) override;
    bool handleEvent(int key,int eventX, int eventY) override;

private:
    bool pressed_ = false;
	int checked = 0;
    ClickCallback onClick_;
    // Darken color by 30% when pressed
    void darkenColor8(uint32_t* color) const;
};
