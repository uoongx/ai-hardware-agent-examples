#pragma once
#include "frame_view.h"
#include <memory>

/**
 * VerticalScrollView - A container that supports vertical scrolling via touch drag.
 * 
 * Usage:
 *   auto scroll = std::make_shared<VerticalScrollView>(width, height);
 *   scroll->setContentHeight(totalContentHeight);  // Set the total scrollable height
 *   scroll->addView(child, x, y);  // Add children as usual
 *   // Children with y >= height will be scrolled into view when user drags
 */
class VerticalScrollView : public FrameView {
public:
    VerticalScrollView(int width, int height);
    
    // Set the total content height (can be larger than view height)
    void setContentHeight(int contentHeight);
    int getContentHeight() const { return contentHeight_; }
    
    // Get/set current scroll offset (0 = top, positive = scrolled down)
    int getScrollOffset() const { return scrollOffset_; }
    void setScrollOffset(int offset);
    
    // Scroll by delta pixels (positive = scroll down, negative = scroll up)
    void scrollBy(int deltaY);
    
    // Scroll to make a specific Y position visible
    void scrollToY(int y);
    
    // Enable/disable scrolling
    void setScrollEnabled(bool enabled) { scrollEnabled_ = enabled; }
    bool isScrollEnabled() const { return scrollEnabled_; }
    
    // Overrides
    int getPixel8(int x, int y, uint32_t* buffer) override;
    bool handleEvent(int key, int eventX, int eventY) override;
    
private:
    void clampScrollOffset();
    
    int contentHeight_ = 0;        // Total content height (set by user)
    int scrollOffset_ = 0;          // Current scroll position (pixels from top)
    bool scrollEnabled_ = true;
    
    // Touch tracking for drag scrolling
    bool isDragging_ = false;
    int lastTouchY_ = 0;
    int touchStartY_ = 0;
    int scrollStartOffset_ = 0;
    
    // Minimum drag distance to trigger scroll (avoid accidental scroll on tap)
    static const int DRAG_THRESHOLD = 8;
    bool dragThresholdMet_ = false;
};
