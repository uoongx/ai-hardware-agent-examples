#pragma once
#include "label_view.h"
#include <cstdio>

// NearLink icon dimensions
#define NEARLINK_ICON_WIDTH 32
#define NEARLINK_ICON_HEIGHT 32

// NearLink status
enum NearLinkStatus {
    NEARLINK_DISABLED = 0,
    NEARLINK_ENABLED = 1
};

// RGB565 colors (for reference, actual icons are pre-generated from PNG files)
#define NEARLINK_COLOR_WHITE   0xFFFF   // RGB565 white (transparent background)

/**
 * NearLinkView - A specialized LabelView for displaying NearLink (星闪) status.
 * 
 * Displays a NearLink icon centered in the view.
 * Status: Enabled (green), Disabled (gray)
 */
class NearLinkView : public LabelView {
public:
    NearLinkView(int width, int height);
    ~NearLinkView();
    
    // Set NearLink status
    void setStatus(NearLinkStatus status);
    NearLinkStatus getStatus() const { return status_; }
    
    // Convenience methods
    void setEnabled(bool enabled) { setStatus(enabled ? NEARLINK_ENABLED : NEARLINK_DISABLED); }
    bool isEnabled() const { return status_ == NEARLINK_ENABLED; }
        
private:
    void updateDisplay();
    void loadNearLinkIcon();
    
    NearLinkStatus status_ = NEARLINK_DISABLED;    
    // Offset for centering icon
    int iconOffsetX_ = 0;
    int iconOffsetY_ = 0;
};
