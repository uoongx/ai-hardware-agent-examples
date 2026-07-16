#pragma once
#include "frame_view.h"
#include "spinner_view.h"
#include "label_view.h"
#include "prj_config.h"
#include <vector>
#include <memory>
#include <functional>
#include <ctime>

enum DateTimePickerMode {
    DATETIME_PICKER_DATE_ONLY = 0,    // 只选择日期
    DATETIME_PICKER_TIME_ONLY = 1     // 只选择时间
};

class DateTimePickerView : public FrameView {
public:
    using DateTimeCallback = std::function<void(int year, int month, int day, int hour, int minute, int second)>;
    
    DateTimePickerView(int width, int height);
    
    // Mode management
    void setMode(DateTimePickerMode mode);
    DateTimePickerMode getMode() const { return mode_; }
    
    // Date/Time setting and getting
    void setDateTime(int year, int month, int day, int hour = 0, int minute = 0, int second = 0);
    void getDateTime(int& year, int& month, int& day, int& hour, int& minute, int& second) const;
    
    // Callback
    void setOnConfirm(DateTimeCallback callback);
    
    // Initialization (must be called after object is managed by shared_ptr)
    void initializeViews();
    
    // Overrides
    ViewType getViewType() const override { return VIEW_TYPE_DATETIME_PICKER; }
    void flush(int sx, int sy, int w, int h) override;

private:
    DateTimePickerMode mode_;
    
    // Date mode controls
    std::shared_ptr<SpinnerView> yearSpinner_;
    std::shared_ptr<LabelView> separator1_;  // "-"
    std::shared_ptr<SpinnerView> monthSpinner_;
    std::shared_ptr<LabelView> separator2_;  // "-"
    std::shared_ptr<SpinnerView> daySpinner_;
    
    // Time mode controls
    std::shared_ptr<SpinnerView> hourSpinner_;
    std::shared_ptr<LabelView> timeSeparator1_;  // ":"
    std::shared_ptr<SpinnerView> minuteSpinner_;
    std::shared_ptr<LabelView> timeSeparator2_;  // ":"
    std::shared_ptr<SpinnerView> secondSpinner_;
    
    // Callback
    DateTimeCallback onConfirmCallback_;
    
    // Current date/time values
    int year_;
    int month_;
    int day_;
    int hour_;
    int minute_;
    int second_;
    
    // Helper methods
    void updateDateModeLayout();
    void updateTimeModeLayout();
    void updateYearSpinner();
    void updateMonthSpinner();
    void updateDaySpinner();  // Update based on year and month (consider leap year and month length)
    void updateHourSpinner();
    void updateMinuteSpinner();
    void updateSecondSpinner();
    
    // Event handlers
    void onYearSelected(int index, const char* text);
    void onMonthSelected(int index, const char* text);
    void onDaySelected(int index, const char* text);
    void onHourSelected(int index, const char* text);
    void onMinuteSelected(int index, const char* text);
    void onSecondSelected(int index, const char* text);
    
    // Utility methods
    int getDaysInMonth(int year, int month) const;
    bool isLeapYear(int year) const;
    int getCurrentYear() const;
};
