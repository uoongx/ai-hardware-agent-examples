#ifndef INCLUDE_TEXTEDIT_VIEW
#define INCLUDE_TEXTEDIT_VIEW
#include <cstdint>
#include <cstring>
#include <string>
#include <functional>
#include "view.h"
#include "window.h"
#include "frame_view.h"

#include "render_service.h"
#include "prj_config.h"

class TextEditView : public View {
public:
	TextEditView(int width, int height);
    virtual ~TextEditView();

    // Text management
    std::string getText() const { return text_; }
    void appendText(const std::string& text);
    void backspace();
    void setReadOnly(bool en);
    void clear();

    // Visual properties
    void setBackgroundColor(uint16_t color);
    
    // Overrides from View
    ViewType getViewType() const override { return VIEW_TYPE_TEXTEDIT; }
    
    // Event handling
    virtual bool handleEvent(int key, int eventX, int eventY) override;
	
    virtual int getPixel8(int x, int y, uint32_t* buffer) override;    
    // Callbacks
    using FocusCallback = std::function<void(TextEditView*)>;
	
    void setOnFocusCallback(FocusCallback callback) { onFocus_ = callback; }
    
    using TextChangeCallback = std::function<void(TextEditView*)>;
    void setOnTextChangeCallback(TextChangeCallback callback) { onTextChange_ = callback; }

private:    
    std::string text_;
    int cursor_position_ = 0;
    bool selected = false;
    bool readonly = false;
    // Callbacks
    FocusCallback onFocus_ = nullptr;
    TextChangeCallback onTextChange_ = nullptr;
};
#endif
