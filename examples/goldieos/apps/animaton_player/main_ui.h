#ifndef INCLUDE_MAIN_UI_H
#define INCLUDE_MAIN_UI_H
#include <iostream>
#include <unistd.h>
#include <memory>
#include "window.h"
#include "frame_view.h"
#include "label_view.h"
#include "goldie_display.h"
#include "ellipse_88_88_mask.h"
#define ANIMATION_WINDOW_START_X 115
#define ANIMATION_WINDOW_START_Y 100
#define APP_WINDOW_WIDTH_ 88
#define APP_WINDOW_HEIGHT_ 88

static std::shared_ptr<Window> window;
static std::shared_ptr<FrameView> frame1;
static std::shared_ptr<LabelView> label1;
static void main_ui_init(void) {
    // Create window
    window = std::make_shared<Window>(ANIMATION_WINDOW_START_X,ANIMATION_WINDOW_START_Y,APP_WINDOW_WIDTH_, APP_WINDOW_HEIGHT_);
    
    frame1 = std::make_shared<FrameView>(APP_WINDOW_WIDTH_, APP_WINDOW_HEIGHT_);
    frame1->setColor(RGB565(0, 0, 0));
	
    // Label 1
    label1 = std::make_shared<LabelView>(APP_WINDOW_WIDTH_, APP_WINDOW_HEIGHT_);
	label1->setMaskBuffer(ellipse_88_88_mask);
    label1->setColor(RGB565(0, 0, 0));
	
	frame1->addView(label1,0,0);
    window->addView(frame1,0,0);
    window->flush(0,0,APP_WINDOW_WIDTH_,APP_WINDOW_HEIGHT_);
}


static void window_exit(void)
{
	if(!window)
	{
		return;
	}
	window->setVisible(false);
	window->removeAllViews();
	frame1->removeAllChilds(); 
	
	frame1.reset();
	label1.reset();	
	window.reset();	
}

static void window_suspend(void)
{
	if(!window)
	{
		return;
	}
	window->setVisible(false);
}

static void window_resume(void)
{
	if(!window)
	{
		return;
	}
	window->setVisible(true);
	window->flush(0,0,APP_WINDOW_WIDTH_,APP_WINDOW_HEIGHT_);
}
#endif
