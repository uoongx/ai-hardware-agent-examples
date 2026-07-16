#pragma once
#include "frame_view.h"
#include "label_view.h"
#include "button_view.h"
#include "list_view.h"
#include "window.h"
#include "prj_config.h"
#include "goldie_display.h"
#include <vector>
#include <memory>
#include <functional>
#include <string>

// Default configuration constants for SpinnerView
// These can be overridden by user settings
#ifndef SPINNER_BUTTON_WIDTH_RATIO
#define SPINNER_BUTTON_WIDTH_RATIO 8  // Button width = spinner width / 8 (default)
#endif

#ifndef SPINNER_POPUP_DEFAULT_HEIGHT
#define SPINNER_POPUP_DEFAULT_HEIGHT 200  // Default popup height in pixels
#endif

#ifndef SPINNER_POPUP_MIN_HEIGHT
#define SPINNER_POPUP_MIN_HEIGHT 100  // Minimum popup height in pixels
#endif

#ifndef SPINNER_ITEM_HEIGHT
#define SPINNER_ITEM_HEIGHT 32  // Height of each item in list view
#endif

#ifndef SPINNER_POPUP_SPACING
#define SPINNER_POPUP_SPACING 2  // Spacing between spinner and popup
#endif

#ifndef SPINNER_LIST_MARGIN
#define SPINNER_LIST_MARGIN 8  // Margin around list view inside popup frame
#endif

#ifndef SPINNER_TEXT_HEIGHT
#define SPINNER_TEXT_HEIGHT 16  // Text height for label
#endif

#ifndef SPINNER_POPUP_BORDER
#define SPINNER_POPUP_BORDER 8  // Border size around popup list_view when displayed full screen (centered)
#endif

class SpinnerView : public FrameView {
public:
    using ItemSelectCallback = std::function<void(int index, const char* text)>;
    
    SpinnerView(int width, int height);
    
    // Item management
    void addItem(const char* text, int itemId = -1);
    void clearItems();
    void changeItem(const char* text, int itemId);
    bool removeItem(int itemId);
    void setItems(const std::vector<std::string>& items);
    
    // Selection
    void setSelectedIndex(int index);
    int getSelectedIndex() const { return selectedIndex_; }
    const char* getSelectedText() const;
    
    // Callback
    void setOnItemSelect(ItemSelectCallback callback);
    
    // Appearance
    void setLabelColor(uint16_t color);
    void setButtonColor(uint16_t color);
    void setTriangleColor(uint16_t color);
    void setListBackgroundColor(uint16_t color);
    
    // Configuration - allow user to customize dimensions
    void setButtonWidth(int width);  // Set button width explicitly (overrides ratio)
    void setButtonWidthRatio(int ratio);  // Set button width as ratio (width / ratio)
    void setButtonVisible(bool visible);  // Show or hide the button (when hidden, label fills entire width)
    void setPopupDefaultHeight(int height);
    void setPopupMinHeight(int height);
    void setItemHeight(int height);
    void setPopupSpacing(int spacing);
    void setListMargin(int margin);
    
    // Overrides
    ViewType getViewType() const override { return VIEW_TYPE_SPINNER; }
    void flush(int sx, int sy, int w, int h) override;
    bool handleEvent(int key, int eventX, int eventY) override;
    int getPixel8(int x, int y, uint32_t *buffer) override;
    
    // Initialization (must be called after object is managed by shared_ptr)
    void initializeViews();
    void destoryViews();

private:
    struct SpinnerItem {
        std::string text;
        int itemId;
    };
    
    std::shared_ptr<LabelView> labelView_;
    std::shared_ptr<ButtonView> buttonView_;
    std::shared_ptr<ListView> listView_;
    std::shared_ptr<FrameView> popupFrame_;
    std::shared_ptr<Window> popupWindow_;
    
    std::vector<SpinnerItem> items_;
    int selectedIndex_;
    bool isPopupVisible_;
    
    uint16_t labelColor_;
    uint16_t buttonColor_;
    uint16_t triangleColor_;
    uint16_t listBackgroundColor_;
    
    ItemSelectCallback onItemSelectCallback_;
    
    // Configuration values (can be customized by user)
    int buttonWidth_;  // Button width (0 means use ratio)
    int buttonWidthRatio_;  // Button width ratio (width / ratio)
    bool buttonVisible_;  // Whether button is visible (default true)
    int popupDefaultHeight_;
    int popupMinHeight_;
    int itemHeight_;
    int popupSpacing_;
    int listMargin_;
     int viewsMargin = 16;
    
    // Helper methods
    void updateLabel();
    void showPopup();
    void hidePopup();
    void onButtonClick();
    void onListItemClick(int itemId);
    void drawTriangle(int x, int y, int width, int height, uint32_t *buffer, int valid_pixels);
    int getScreenHeight() const;  // Get screen height from parent window or display config
    int getButtonWidth() const;  // Calculate button width based on configuration
    
    // Safe type checking helpers (without RTTI)
    // These methods attempt to identify the type by checking for type-specific methods
    // Since static_cast doesn't validate types, we use a safer approach:
    // Try to call Window-specific methods to verify it's actually a Window
    static Window* tryCastToWindow(View* view);
    static FrameView* tryCastToFrameView(View* view);
};
