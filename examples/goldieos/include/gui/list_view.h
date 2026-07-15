#pragma once
#include "frame_view.h"
#include "button_view.h"
#include <vector>
#include <memory>
#include <functional>

#include <memory>

class ListView : public FrameView {
public:
    using ItemClickCallback = std::function<void(int itemId)>;
    ListView(int width, int height);
    void setItemIcon(int index,const uint16_t* icon,int w,int h);
    int getSize();

    // Item management
    void addItem(const char* text, int itemId);
    void changeItem(const char* text, int itemId);	
    bool removeItem(int itemId);
    void clearItems();
    // Click handling
    void setOnItemClick(ItemClickCallback callback);
    int getSelectedIndex();
    void setSelected(int index);
    int setSelectedIcon(const uint16_t* icon,int w,int h);
    
    // Margin and spacing settings
    void setItemMargin(int margin);
    void setItemHeight(int height){itemHeight_ = height;};
    int getItemMargin() const;
    int  getItemHeight(){ return itemHeight_;};	
    
    // Overrides
    void flush(int sx, int sy, int w, int h) override;
    virtual bool handleEvent(int key, int eventX, int eventY) override;

private:
    // 虚拟滚动 - Item数据结构
    struct ItemData {
        char* text;           // item的文本
        int itemId;           // item的ID
        const uint16_t* icon; // item的图标
        int iconW, iconH;     // 图标宽高
    };

    // Button复用 - 记录Button当前显示的item索引
    struct ButtonState {
        std::shared_ptr<ButtonView> button;
        int displayedItemIndex; // 当前button显示的是第几个item (-1表示未使用)
    };

    ItemClickCallback itemClickCallback_;
    
    // 虚拟滚动相关数据
    std::vector<ItemData> itemDataList_;        // 存储所有item的数据
    std::vector<ButtonState> buttonPool_;       // Button复用池
    int maxButtons_ = 0;                         // 最多创建的button数量 = 可视数量 + 4(上下各2个buffer)
    float scrollOffset_ = 0.0f;                  // 虚拟滑动轴的偏移量(用float表示像素精度)

    const uint16_t* selected_icon = nullptr;
    int selected_item = -1;

    int itemHeight_ = 32;   // 单个item的高度
    int item_margin = 24;   // item的左右边距
    
    // 触摸事件相关
    int list_head_positon = 8;
    int lastTouchY_ = -1;
    bool isTouching_ = false;
    bool hasMovedDuringTouch_ = false;  // 记录本次触摸过程中是否发生过较大位移
	
    int TOUCH_SLOP = 5;               // 触摸移动阈值
    int listSize = 0;
    int totalContentHeight;
    int maxScrollPosition = 0;
    int minScrollPosition = 0;
	
    // 私有方法
    void handleTouchUp(int totalContentHeight);
    void updateScrollPosition(int deltaY);
    void handleTouchDown(int eventY);
    void handleItemClick(void* context);
    
    // 虚拟滚动相关私有方法
    void ensureButtonPool();                        // 确保button池已创建
    void updateVisibleItems();                      // 更新可见范围内的items
    int getItemIndexFromButtonClick(void* context); // 从button的click事件获取item索引
    void updateButtonContent(int buttonIndex, int itemIndex); // 更新button显示的content
};
