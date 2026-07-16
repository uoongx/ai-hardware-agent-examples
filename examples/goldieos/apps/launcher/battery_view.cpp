#include "battery_view.h"
#include <cstdlib>
#include <cstring>


#include "rgb16_battery_chg_48_16.h"
#include "rgb16_norm_48_16.h"
#include "rgb16_full_48_16.h"
#include "rgb16_low_48_16.h"
#include "rgb16_high_48_16.h"
#include "rgb16_least_48_16.h"



BatteryView::BatteryView(int width, int height)
    : LabelView(width, height) {
    loadBatteryIcon();
    updateDisplay();
}

BatteryView::~BatteryView() {
    // No dynamic memory to free - icons are static const arrays
}

void BatteryView::setBatteryLevel(int level) {
    if (level < 0) level = 0;
    if (level > 100) level = 100;
    
    if (level != batteryLevel_) {
        batteryLevel_ = level;
        loadBatteryIcon();
        updateDisplay();
    }
}

void BatteryView::setCharging(bool charging) {
    if (charging != isCharging_) {
        isCharging_ = charging;
        loadBatteryIcon();
        updateDisplay();
    }
}

void BatteryView::loadBatteryIcon() {
   if(isCharging_)
   {
	currentIcon_ =  (uint16_t*)&rgb16_battery_chg_48_16;
   }else  if (batteryLevel_ <= 20) {
        currentIcon_ = (uint16_t*)&rgb16_least_48_16;
    } else if (batteryLevel_ >20 && batteryLevel_<40 ) {
        currentIcon_ = (uint16_t*)&rgb16_low_48_16;
    } else if (batteryLevel_ >=40 && batteryLevel_<=60 ) {
        currentIcon_ = (uint16_t*)&rgb16_norm_48_16;
    } else if (batteryLevel_ >60 && batteryLevel_<80 ) {
        currentIcon_ = (uint16_t*)&rgb16_high_48_16;
    }  else if (batteryLevel_ >=80) {
        currentIcon_ = (uint16_t*)&rgb16_full_48_16;
    } 

    View::setImageBuffer(currentIcon_);
}

void BatteryView::updateDisplay() {
    View::flush(0, 0, width_, height_);
}
