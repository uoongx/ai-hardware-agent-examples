#include "nearlink_view.h"
#include "rgb16_nearlink2_24_27.h"
#include "rgb16_nearlink21_24_27.h"
#include <cstdlib>
#include <cstring>

NearLinkView::NearLinkView(int width, int height)
    : LabelView(width, height) {
    loadNearLinkIcon();
    updateDisplay();
}

NearLinkView::~NearLinkView() {
}

void NearLinkView::setStatus(NearLinkStatus status) {
    if (status != status_) {
        status_ = status;
        loadNearLinkIcon();
        updateDisplay();
    }
}

void NearLinkView::loadNearLinkIcon() {    
    // Copy icon data from pre-generated arrays (from PNG files)
    const uint16_t* srcIcon = (status_ == NEARLINK_ENABLED) 
        ? (uint16_t*)&rgb16_nearlink2_24_27 
        : (uint16_t*)&rgb16_nearlink21_24_27;
    View::setImageBuffer(srcIcon);
}

void NearLinkView::updateDisplay() {    
    View::flush(0, 0, width_, height_);
}
