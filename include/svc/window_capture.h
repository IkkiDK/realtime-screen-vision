#pragma once
// ============================================================================
// WindowCapture - Grab the client area of a window as a cv::Mat.
//
// Uses GDI: GetDC -> CreateCompatibleDC -> CreateCompatibleBitmap -> BitBlt ->
// GetDIBits -> cv::Mat. Captures only the content region (no borders/title bar).
//
// Note: a minimized window has no rendered content, so capture returns empty.
// ============================================================================

#include <windows.h>
#include <opencv2/opencv.hpp>
#include "svc/window_finder.h"

class WindowCapture {
public:
    // Returns an empty Mat on failure.
    cv::Mat capture(const WindowInfo& info);

    const cv::Mat& lastFrame() const;
    bool hasValidFrame() const;

private:
    cv::Mat last_frame_;
};
