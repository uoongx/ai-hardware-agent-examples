#pragma once
#include "view.h"
#include <functional>
#include "window.h"
#include "frame_view.h"
#include "prj_config.h"

class ImgButtonView : public View {
public: 
    // Callback function signature - returns current state (1=pressed, 0=released) and control ID
    using ClickCallback = std::function<void(void*)>;
    ImgButtonView(int width, int height);
    
    // Set background images for pressed and released states
    void setPressedImage(const uint16_t* pressed_image);
    void setReleasedImage(const uint16_t* released_image);
    
    // Set self-locking property
    void setSelfLocking(bool self_locking);
    bool isSelfLocking() const { return self_locking_; }
    
    // Get current locked state
    bool isLocked() const { return locked_state_; }
    void setLocked(bool status);
    // Click handling
    void setOnClick(ClickCallback callback);
    
    // Overrides from View
    ViewType getViewType() const override { return VIEW_TYPE_IMGBUTTON; }
    int getPixel8(int x, int y, uint32_t *buffer) override;
    bool handleEvent(int key, int eventX, int eventY) override;

private:
    bool pressed_ = false;        // Temporary press state for visual feedback
    bool locked_state_ = false;   // Persistent locked state (for self-locking mode)
    bool self_locking_ = false;   // false = auto-release, true = stay pressed until clicked again
    
    const uint16_t* pressed_image_ = nullptr;
    const uint16_t* released_image_ = nullptr;
    
    ClickCallback onClick_;
};
