#pragma once

#include "frame_view.h"
#include "label_view.h"
#include "switch_view.h"

#include <memory>
#include <functional>

// A FrameView for list items with:
// - Fixed height: 56px
// - Variable width
// - Background: parent's background color + 3-part background images (left circle, middle tile, right circle)
// - Left: 32x32 icon area (optional, vertically centered)
// - Middle: text label (transparent background, left-aligned with 8px padding)
// - Right: 48x32 area (can display SwitchView or icon)
class ListItemFrameView : public FrameView {
public:
    using ClickCallback = std::function<void(void*)>;

    int kItemHeight = 56;
    int kLeftIconSize = 32;
    int kRightAreaWidth = 48;
    int kRightAreaHeight = 32;
    int kLeftCircleWidth = 24;
    int kLeftCircleHeight = 56;
    int kMiddleTileWidth = 8;
    int kMiddleTileHeight = 56;
    int kRightCircleWidth = 24;
    int kRightCircleHeight = 56;
    int kIconPadding = 8;  // Padding between icon and text
    int kTextPadding = 8;  // Left padding inside text area; text is left-aligned

    // Factory: ensures safe use of shared_from_this() internally.
    static std::shared_ptr<ListItemFrameView> Create(int width);

    // Text handling: empty text hides the label, non-empty shows it.
    void setText(const char* text);

    // Left icon handling
    void setLeftIcon(const uint16_t* icon_buffer);
    void setLeftIconVisible(bool visible);

    // Right area: can set SwitchView or icon
    void setRightSwitch(std::shared_ptr<SwitchView> switch_view);
    void setRightIcon(const uint16_t* icon_buffer);
    void setRightAreaVisible(bool visible);

    // Next arrow icon (32x32, right-aligned in right area)
    void setNextArrowVisible(bool visible);

    // Click callback
    void setOnClick(ClickCallback callback);

    // Override setSize to handle width changes and maintain fixed height
    void setSize(int width, int height);

    // Accessors
    std::shared_ptr<LabelView> getTextLabel() const { return text_label_; }
    std::shared_ptr<View> getLeftIconView() const { return left_icon_view_; }
    std::shared_ptr<View> getRightAreaView() const { return right_area_view_; }

    // Sample background at list-item-local (x,y); used by icon so transparent areas match all 3 parts
    void getBackgroundPixel8(int x, int y, uint32_t* buffer);

private:
    struct PrivateTag {};
    ListItemFrameView(int width, PrivateTag);
    void postInit();
    void layoutBackground();
    void layoutContent();

private:
    // Background views for 3-part background
    std::shared_ptr<View> left_circle_view_;
    std::shared_ptr<View> middle_tile_view_;
    std::shared_ptr<View> right_circle_view_;
    
    // Content views
    std::shared_ptr<LabelView> text_label_;
    std::shared_ptr<View> left_icon_view_;
    std::shared_ptr<View> right_area_view_;
    std::shared_ptr<View> next_arrow_view_;  // Next arrow icon (32x32, right-aligned)
    
    ClickCallback onClick_;
    
    // Cached values for layout
    int left_icon_x_ = 0;
    int text_x_ = 0;
    int text_width_ = 0;
    int right_area_x_ = 0;
};
