#include "svc/window_capture.h"
#include "svc/logger.h"

// Every GDI object created here MUST be released; leaking them degrades the
// whole desktop's performance over time.
cv::Mat WindowCapture::capture(const WindowInfo& info) {
    if (!info.valid || info.minimized) {
        return cv::Mat();
    }

    int w = info.width;
    int h = info.height;
    if (w <= 0 || h <= 0) {
        return cv::Mat();
    }

    HDC hwnd_dc = GetDC(info.hwnd);
    if (!hwnd_dc) {
        LOG_ERROR("failed to get window DC");
        return cv::Mat();
    }

    HDC mem_dc = CreateCompatibleDC(hwnd_dc);
    if (!mem_dc) {
        ReleaseDC(info.hwnd, hwnd_dc);
        LOG_ERROR("failed to create memory DC");
        return cv::Mat();
    }

    HBITMAP bitmap = CreateCompatibleBitmap(hwnd_dc, w, h);
    if (!bitmap) {
        DeleteDC(mem_dc);
        ReleaseDC(info.hwnd, hwnd_dc);
        LOG_ERROR("failed to create bitmap");
        return cv::Mat();
    }

    HBITMAP old_bitmap = (HBITMAP)SelectObject(mem_dc, bitmap);
    BitBlt(mem_dc, 0, 0, w, h, hwnd_dc, 0, 0, SRCCOPY);

    BITMAPINFOHEADER bi = {};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = w;
    bi.biHeight = -h;   // negative = top-down (origin at top-left)
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    cv::Mat frame(h, w, CV_8UC4);
    GetDIBits(mem_dc, bitmap, 0, h, frame.data,
              reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);

    cv::cvtColor(frame, last_frame_, cv::COLOR_BGRA2BGR);

    // Restore the old bitmap BEFORE deleting ours.
    SelectObject(mem_dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(mem_dc);
    ReleaseDC(info.hwnd, hwnd_dc);

    return last_frame_;
}

const cv::Mat& WindowCapture::lastFrame() const {
    return last_frame_;
}

bool WindowCapture::hasValidFrame() const {
    return !last_frame_.empty();
}
