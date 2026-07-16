#pragma once
#include "frame_view.h"
#include <memory>

/**
 * ScrollView - A container that supports horizontal scrolling via touch drag.
 * 
 * Usage:
 *   auto scroll = std::make_shared<ScrollView>(width, height);
 *   scroll->setContentWidth(totalContentWidth);  // Set the total scrollable width
 *   scroll->addView(child, x, y);  // Add children as usual
 *   // Children with x >= width will be scrolled into view when user drags
 */
class ScrollView : public FrameView {
public:
    ScrollView(int width, int height);
    
    // Set the total content width (can be larger than view width)
    void setContentWidth(int contentWidth);
    int getContentWidth() const { return contentWidth_; }
    
    // Get/set current scroll offset (0 = left, positive = scrolled right)
    int getScrollOffset() const { return scrollOffset_; }
    void setScrollOffset(int offset);
    
    // Scroll by delta pixels (positive = scroll right, negative = scroll left)
    void scrollBy(int deltaX);
    
    // Scroll to make a specific X position visible
    void scrollToX(int x);
    
    // Enable/disable scrolling
    void setScrollEnabled(bool enabled) { scrollEnabled_ = enabled; }
    bool isScrollEnabled() const { return scrollEnabled_; }
    
    // Overrides
    int getSelfPixel8(int x, int y,uint32_t* buffer);
    int getPixel8(int x, int y, uint32_t* buffer) override;
    bool handleEvent(int key, int eventX, int eventY) override;
    
private:
    void clampScrollOffset();
    
    int contentWidth_ = 0;        // Total content width (set by user)
    int scrollOffset_ = 0;        // Current scroll position (pixels from left)
    bool scrollEnabled_ = true;
    
    // Touch tracking for drag scrolling
    bool isDragging_ = false;
    int lastTouchX_ = 0;
    int touchStartX_ = 0;
    int scrollStartOffset_ = 0;
    
    // Minimum drag distance to trigger scroll (avoid accidental scroll on tap)
    static const int DRAG_THRESHOLD = 8;
    bool dragThresholdMet_ = false;
};
