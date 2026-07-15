#pragma once
#include "view.h"
#include <string>
#include <vector>
#include "window.h"
#include "frame_view.h"
#include "prj_config.h"

class ProgressBarView : public View {
public:
    using valueChangeCallback = std::function<void(int value)>;
    ProgressBarView(int width, int height);
    
    // Progress management
    void setProgress(int progress);  // 0-100
    void setProgress(float progress);  // 0.0-1.0
    int getProgress() const { return progress_; }
    void setOnValueChange(valueChangeCallback callback);
    // Color management
    void setProgressColor(uint16_t color);
    void setBackgroundColor(uint16_t color);
    uint16_t getProgressColor() const { return progress_color_; }
    uint16_t getBackgroundColor() const { return background_color_; }
    
    // Border management
    void setBorderVisible(bool visible) { border_visible_ = visible; }
    void setBorderColor(uint16_t color) { border_color_ = color; }
    bool isBorderVisible() const { return border_visible_; }
    
    // Overrides from View
    ViewType getViewType() const override { return VIEW_TYPE_PROGRESSBAR; }
    int getPixel8(int x, int y, uint32_t *buffer) override;
    virtual bool handleEvent(int key, int eventX, int eventY) override;

private:
    int progress_;  // 0-100
    uint16_t progress_color_;
    uint16_t background_color_;
    uint16_t border_color_;
    bool border_visible_;
    int border_width_;
    valueChangeCallback vCallback_;
    
    void calculateProgressPixels();
    int progress_pixels_;  // Calculated progress in pixels
};
