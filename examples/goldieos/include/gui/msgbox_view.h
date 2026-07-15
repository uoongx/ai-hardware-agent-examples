#include <string>
#include <vector>
#include "window.h"
#include "frame_view.h"
#include "prj_config.h"

#define MSG_ID_REMOTE 0
#define MSG_ID_LOCAL 1

#define MAX_LINE_LENGTH  162
#define MAX_LINES 10

class MsgBoxView : public View {
public:
	MsgBoxView(int width, int height);	
    // Overrides from View
    ViewType getViewType() const override { return VIEW_TYPE_MSGBOX; }
    int getPixel8(int x, int y,uint32_t *buffer) override;
    void scrollScreen(int src_y,int dst_y);
    void appendMsg(char* msg,int uid);
private:
    typedef struct {
        char bytes[4];
        int length;
        unsigned int code_point;
    } UTF8Char;

    typedef struct {
        char text[MAX_LINE_LENGTH];
        int width;
    } LineInfo;

    std::string font_name_ = "default";
    int font_size_ = 12;
    LineInfo g_lines[50];
    // Storage for text buffer
    std::vector<uint8_t> text_buffer_storage_;
    unsigned int current_y =0;
    char current_line[MAX_LINE_LENGTH] = {0};
    int split2Lines(const char *msg, LineInfo *lines);
    int getNextUchar(const char **str, UTF8Char *uc);
};
