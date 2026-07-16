#pragma once

#include "frame_view.h"
#include "button_view.h"
#include "label_view.h"

#include <memory>
#include <functional>

// A FrameView with a fixed title bar (64px) on top and a client area below.
// - Overall background: pure white
// - addView(): by default adds children into client area (coords are client-relative)
class CaptionFrameView : public FrameView {
public:
    using ClickCallback = std::function<void(void*)>;

      int kTitleBarHeight = 64;
      int kBackButtonSize = 32;
      int kTitleBgWidth = 288;
      int kTitleBgHeight = 48;
      int kBackButtonPadding = 16;

    // Factory: ensures safe use of shared_from_this() internally.
    static std::shared_ptr<CaptionFrameView> Create(int width, int height);

    // By default, put child into client area. Coordinates are client-relative.
    void addView(std::shared_ptr<View> child, int x, int y);

    // Title handling: empty title hides the label, non-empty shows it.
    void setTitle(const char* title);

    // Back button visibility control.
    void setBackButtonVisible(bool visible);

    // Back button click callback.
    void setOnBack(ClickCallback callback);

    // Accessors if callers need direct control.
    std::shared_ptr<FrameView> getTitleBar() const { return title_bar_; }
    std::shared_ptr<FrameView> getClient() const { return client_; }
    std::shared_ptr<ButtonView> getBackButton() const { return back_button_; }
    std::shared_ptr<LabelView> getTitleLabel() const { return title_label_; }

private:
    struct PrivateTag {};
    CaptionFrameView(int width, int height, PrivateTag);
    void postInit();
    void layoutTitleBar();

private:
    std::shared_ptr<FrameView> title_bar_;
    std::shared_ptr<FrameView> client_;
    std::shared_ptr<ButtonView> back_button_;
    // Title bar background and text: TransparentLabelView handles both.
    // Background image (288x48) with 0xFFFF transparency, text overlaid on top.
    std::shared_ptr<LabelView> title_label_;
    ClickCallback on_back_;
};

