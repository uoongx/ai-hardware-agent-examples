#pragma once
#include "label_view.h"
#include <cstdio>

// WiFi icon dimensions
#define WIFI_ICON_WIDTH 32
#define WIFI_ICON_HEIGHT 32

// WiFi signal strength levels
enum WifiSignalLevel {
    WIFI_SIGNAL_NONE = 0,   // No signal / disconnected
    WIFI_SIGNAL_WEAK = 1,   // Weak signal (1 bar)
    WIFI_SIGNAL_MEDIUM = 2, // Medium signal (2 bars)
    WIFI_SIGNAL_STRONG = 3  // Strong signal (3 bars)
};

// Transparent color (white background in icons)
#define WIFI_COLOR_TRANSPARENT 0xFFFF

/**
 * WifiView - A specialized LabelView for displaying WiFi signal status.
 * 
 * Displays a WiFi signal icon centered in the view.
 * Signal levels: None, Weak (1 bar), Medium (2 bars), Strong (3 bars)
 * Uses PNG-based icons for each signal level.
 */
class WifiView : public LabelView {
public:
    WifiView(int width, int height);
    ~WifiView();
    
    // Set WiFi signal level (0-3)
    void setSignalLevel(WifiSignalLevel level);
    WifiSignalLevel getSignalLevel() const { return signalLevel_; }
        
private:
    void updateDisplay();
    void loadWifiIcon();
    
    WifiSignalLevel signalLevel_ = WIFI_SIGNAL_STRONG;
    const uint16_t* currentIcon_ = nullptr;
    
    // Offset for centering icon
    int iconOffsetX_ = 0;
    int iconOffsetY_ = 0;
};
