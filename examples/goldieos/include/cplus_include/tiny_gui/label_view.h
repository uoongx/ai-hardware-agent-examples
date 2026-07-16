#pragma once
#include "view.h"
#include <string>
#include <vector>
#include "window.h"
#include "frame_view.h"
#include "prj_config.h"

class LabelView : public View {
public:
	LabelView(int width, int height);
	void setIcon(const uint16_t* buf,int w,int h);
	void setIconVisible(bool visible){icon_visible = visible;};
	void setText(const char* text) override;   
	void setText(const char* text,int sx,int sy) override;
	// Overrides from View
	ViewType getViewType() const override { return VIEW_TYPE_LABEL; }
	int getPixel8(int x, int y,uint32_t *buffer) override;

protected:
	const uint16_t* icon_buf=nullptr;
	int icon_w=0;
	int icon_h=0;
	bool icon_visible = true;
};
