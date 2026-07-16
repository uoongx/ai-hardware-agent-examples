#ifndef INCLUDE_INPUTMETHOD_VIEW
#define INCLUDE_INPUTMETHOD_VIEW

#include "frame_view.h"
#include "button_view.h"
#include <vector>
#include <memory>
#include <functional>
#include "textedit_view.h"
#include "label_view.h"

#define NORM_KEY_SIZE_WH 1
#define NORM_KEY_SIZE_UNIT 16

enum{
    KEY_BOARD_MODE_NUMBER=0,
    KEY_BOARD_MODE_LOWERCASE,
    KEY_BOARD_MODE_UPPERCASE,
};

class InputMethodView : public FrameView {
public:
    InputMethodView(int width,int height);
	~InputMethodView();
	
    // Overrides from FrameView
    ViewType getViewType() const override { return VIEW_TYPE_INPUTMETHOD; }
	
    void setTarget(const std::shared_ptr<TextEditView>& target);  // ������void��������
    void setKeysImageBuffer_w1(const uint16_t* buffer){keys_img_buffer_w1 = buffer;};
    void setKeysImageBuffer_w2(const uint16_t* buffer){keys_img_buffer_w2 = buffer;};
    void setKeysImageBuffer_w3(const uint16_t* buffer){keys_img_buffer_w3 = buffer;};
    void get_norm_key_size(int * w,int * h);
private:
    void create_keyboard_layout();
    void handle_shift();
    void handle_NC();
    void send_key_to_target(const char* key);
    void handle_backspace();
	void handle_Hide();
    
    struct KeyLayout {
        const char* label;
        const char* value;
        int width;
        int height;
    };

    const KeyLayout default_keys_layout[4] = {  // ��ȷָ�������С
		 {"Upper", "SHIFT", NORM_KEY_SIZE_WH*2, NORM_KEY_SIZE_WH}, 
		 {"DEL", "BACKSPACE", NORM_KEY_SIZE_WH*2, NORM_KEY_SIZE_WH},
		 {"关闭", "HIDE", NORM_KEY_SIZE_WH*2, NORM_KEY_SIZE_WH},
    };

    const KeyLayout number_layout[14] = {  // ��ȷָ�������С
        {"1", "1", NORM_KEY_SIZE_WH*2, NORM_KEY_SIZE_WH}, {"2", "2", NORM_KEY_SIZE_WH*2, NORM_KEY_SIZE_WH}, {"3", "3", NORM_KEY_SIZE_WH*2, NORM_KEY_SIZE_WH}, {"4", "4", NORM_KEY_SIZE_WH*2, NORM_KEY_SIZE_WH}, {"5", "5", NORM_KEY_SIZE_WH*2, NORM_KEY_SIZE_WH},
        {"6", "6", NORM_KEY_SIZE_WH*2, NORM_KEY_SIZE_WH}, {"7", "7", NORM_KEY_SIZE_WH*2, NORM_KEY_SIZE_WH}, {"8", "8", NORM_KEY_SIZE_WH*2, NORM_KEY_SIZE_WH}, {"9", "9", NORM_KEY_SIZE_WH*2, NORM_KEY_SIZE_WH}, {"0", "0", NORM_KEY_SIZE_WH*2, NORM_KEY_SIZE_WH},
        {",", ",", NORM_KEY_SIZE_WH*2, NORM_KEY_SIZE_WH}, {".", ".", NORM_KEY_SIZE_WH*2, NORM_KEY_SIZE_WH}, {"Space", " ", NORM_KEY_SIZE_WH*3, NORM_KEY_SIZE_WH},{"Num", "N/C", NORM_KEY_SIZE_WH*3, NORM_KEY_SIZE_WH},
	};
    
    const KeyLayout lowercase_layout[28] = {  // ��ȷָ�������С
        {"q", "q", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"w", "w", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"e", "e", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"r", "r", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"t", "t", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH},
        {"y", "y", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"u", "u", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"i", "i", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"o", "o", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"p", "p", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH},
        {"a", "a", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"s", "s", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"d", "d", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"f", "f", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"g", "g", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH},
        {"h", "h", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"j", "j", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"k", "k", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"l", "l", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, 
        {"z", "z", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"x", "x", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"c", "c", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"v", "v", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"b", "b", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH},
        {"n", "n", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"m", "m", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"Space", " ", NORM_KEY_SIZE_WH*2, NORM_KEY_SIZE_WH},{"Num", "N/C", NORM_KEY_SIZE_WH*2, NORM_KEY_SIZE_WH},
    };
    
    const KeyLayout uppercase_layout[28] = {  // ��ȷָ�������С
        {"Q", "Q", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"W", "W", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"E", "E", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"R", "R", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"T", "T", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH},
        {"Y", "Y", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"U", "U", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"I", "I", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"O", "O", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"P", "P", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH},
        {"A", "A", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"S", "S", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"D", "D", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"F", "F", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"G", "G", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH},
        {"H", "H", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"J", "J", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"K", "K", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"L", "L", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, 
        {"Z", "Z", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"X", "X", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"C", "C", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"V", "V", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"B", "B", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH},
        {"N", "N", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"M", "M", NORM_KEY_SIZE_WH, NORM_KEY_SIZE_WH}, {"Space", " ", NORM_KEY_SIZE_WH*2, NORM_KEY_SIZE_WH},{"Num", "N/C", NORM_KEY_SIZE_WH*2, NORM_KEY_SIZE_WH},
    };

    std::shared_ptr<LabelView> msg_lable;
    std::shared_ptr<TextEditView> target;  // �Ƴ������ã���Ϊ��ͨshared_ptr
    std::vector<std::shared_ptr<ButtonView>> key_buttons;
    int keyboard_mode = KEY_BOARD_MODE_LOWERCASE;
    const KeyLayout* current_layout = nullptr;
    int size_unit_h = 16;
    int size_unit_w = 16;
    const uint16_t* keys_img_buffer_w1 =nullptr;
    const uint16_t* keys_img_buffer_w2 =nullptr;
    const uint16_t* keys_img_buffer_w3 =nullptr;

};
#endif
