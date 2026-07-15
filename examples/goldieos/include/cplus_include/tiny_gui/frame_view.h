#pragma once
#include "view.h"
#include <vector>
#include <memory>
#include <functional>
#include "window.h"
#include "prj_config.h"

class FrameView : public View , public std::enable_shared_from_this<FrameView> {
public:
	using ClickCallback = std::function<void(void*)>;
	
	FrameView(int width, int height);
	
	// Type identification
	ViewType getViewType() const override { return VIEW_TYPE_FRAME; }
	
	void setParent(const std::shared_ptr<FrameView>& parent);
	void setParent(const std::shared_ptr<Window>& parent);

    // Child view management
    void addView(std::shared_ptr<View> child,int x,int y);
    void addChild(std::shared_ptr<View> child);
    void removeChild(std::shared_ptr<View> child);
	void removeAllChilds();
	
    // Click handling
    void setOnClick(ClickCallback callback);
	
    // Overrides from View
    int getPixel8(int x, int y,uint32_t *buffer) override;
    bool handleEvent(int key,int eventX, int eventY) override;
    
    // Access children list (for derived classes like ScrollView)
    std::vector<std::shared_ptr<View>>& getChildren() { return children_; }
    const std::vector<std::shared_ptr<View>>& getChildren() const { return children_; }

protected:
    std::vector<std::shared_ptr<View>> children_;
	std::weak_ptr<View> first_touched_view;
	bool first_touched_ = false;
	ClickCallback onClick_;
	bool pressed_ = false;  // Track if this FrameView itself was pressed
};
