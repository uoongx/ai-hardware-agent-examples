#pragma once
#include "label_view.h"
#include <ctime>
#include <memory>
#include <string>

// DateTimeView: Custom view for displaying date and time (launcher specific)
// - First row: Time in hh:mm format using custom number icons (24x40 pixels each)
// - Second row: Date in "mm月dd日 周X" format using standard font
// - Both rows are horizontally centered
// - Both rows together are vertically centered within the view
class DateTimeView : public LabelView {
public:
    DateTimeView(int width, int height);
    virtual ~DateTimeView();

    // Update the display with current time (call this every second for blinking colon)
    void updateTime();

    // Set the time manually (for testing or external time source)
    void setTime(int hour, int minute);
    void setDate(int month, int day, int weekday);

    // Override getPixel8 to render custom icons
    int getPixel8(int x, int y, uint32_t* buffer) override;

private:
    // Time values
    int hour_ = 0;
    int minute_ = 0;
    int second_ = 0;  // Used for colon blinking
    
    // Date values
    int month_ = 1;
    int day_ = 1;
    int weekday_ = 0;  // 0=Sunday, 1=Monday, ... 6=Saturday

    // Colon blinking state
    bool colon_visible_ = true;

    // Layout constants
    static const int DIGIT_WIDTH = 24;
    static const int DIGIT_HEIGHT = 40;
    static const int CHAR_PADDING = 0;   // No padding between characters
    
    // Helper methods
    void updateDateLabel();
    int getTimeRowWidth() const;  // Calculate actual width of time display
    int getTimeRowStartX() const; // Calculate X offset for centering time row
    int getTimeRowStartY() const; // Calculate Y offset for vertical centering
    int getDateRowStartY() const; // Calculate Y offset for date row
};
