#include "wifi_view.h"
#include "rgb16_wifi_new_32_25.h"
#include "rgb16_offline1_32_25.h"
#include <cstdlib>
#include <cstring>

WifiView::WifiView(int width, int height)
    : LabelView(width, height) {
    loadWifiIcon();
    updateDisplay();
}

WifiView::~WifiView() {
    currentIcon_ = nullptr;
}

void WifiView::setSignalLevel(WifiSignalLevel level) {
    if (level != signalLevel_) {
        signalLevel_ = level;
        loadWifiIcon();
        updateDisplay();
    }
}

void WifiView::loadWifiIcon() {
    // Select the appropriate icon based on signal level
    switch (signalLevel_) {
        case WIFI_SIGNAL_NONE:
            currentIcon_ = (uint16_t*)&rgb16_offline1_32_25;
            break;
        case WIFI_SIGNAL_WEAK:
            currentIcon_ = (uint16_t*)&rgb16_wifi_new_32_25;
            break;
        case WIFI_SIGNAL_MEDIUM:
            currentIcon_ = (uint16_t*)&rgb16_wifi_new_32_25;
            break;
        case WIFI_SIGNAL_STRONG:
        default:
            currentIcon_ = (uint16_t*)&rgb16_wifi_new_32_25;
            break;
    }
	View::setImageBuffer(currentIcon_);
}

void WifiView::updateDisplay() {
    View::flush(0, 0, width_, height_);
}
