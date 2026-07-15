#pragma once
#include "view.h"
#include <functional>
#include "window.h"
#include "frame_view.h"
#include "prj_config.h"

class SwitchView : public View {
public: 
    using ClickCallback = std::function<void(void*)>;
    using BackgroundSampler = std::function<void(int x, int y, uint32_t* buffer)>;
    
	SwitchView();
    SwitchView(BackgroundSampler background_sampler);
    
    // Click handling
    void setOnClick(ClickCallback callback);
	void setChecked(int on_off);
	int isChecked();
    // Set background sampler for transparent background support
    void setBackgroundSampler(BackgroundSampler background_sampler);
    // Set background images for pressed and released states
    void setPressedImage(const uint16_t* pressed_image);
    void setReleasedImage(const uint16_t* released_image);
	
    // Overrides from View
    int getPixel8(int x, int y, uint32_t *buffer) override;
    bool handleEvent(int key, int eventX, int eventY) override;

private:
	int checked_ = 0;
    ClickCallback onClick_;
    BackgroundSampler background_sampler_;
    // Icon dimensions (fixed 48x32)
      int SWITCH_ICON_WIDTH = 48;
      int SWITCH_ICON_HEIGHT = 32;
    // Icon buffers - will be set from header files
     const uint16_t* icon_on_ = nullptr;
     const uint16_t* icon_off_ = nullptr;
};
