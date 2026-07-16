#pragma once
#include "label_view.h"
#include <cstdio>

// Transparent color marker (white in PNG background)
#define BATTERY_COLOR_TRANSPARENT 0xFFFF

/**
 * BatteryView - A specialized LabelView for displaying battery status.
 * 
 * Displays a battery icon followed by percentage text, centered.
 * Uses PNG-based icons for different battery levels.
 * Supports charging state overlay with lightning icon.
 */
class BatteryView : public LabelView {
public:
    BatteryView(int width, int height);
    ~BatteryView();
    
    // Set battery level (0-100)
    void setBatteryLevel(int level);
    int getBatteryLevel() const { return batteryLevel_; }
    
    // Set charging state
    void setCharging(bool charging);
    bool isCharging() const { return isCharging_; }
       
private:
    void updateDisplay();
    void loadBatteryIcon();
    
    int batteryLevel_ = 100;
    bool isCharging_ = false;
    const uint16_t* currentIcon_ = nullptr;
    
    // Offset for centering icon
    int iconOffsetX_ = 0;
    int iconOffsetY_ = 0;
};
