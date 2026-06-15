#pragma once
// ============================================================================
// WindowFinder - Locate and track a window by its title.
//
// Uses the Win32 API to find a window (FindWindow / EnumWindows), obtain its
// HWND, query its client-area position and size, and check whether it still
// exists or is minimized.
//
//   - HWND: unique handle to a window; every window operation needs it.
//   - Client area: the content region, excluding borders and title bar.
// ============================================================================

#include <windows.h>
#include <string>

struct WindowInfo {
    HWND hwnd = nullptr;
    int x = 0, y = 0;          // top-left of the client area, in screen coords
    int width = 0, height = 0; // client-area size
    bool valid = false;
    bool minimized = false;
    std::string title;
};

class WindowFinder {
public:
    static constexpr int MAX_TITLE_LENGTH = 256;

    // Title to look for. Matching is exact first (FindWindow), then falls back
    // to a case-insensitive substring search over all top-level windows.
    void setTargetTitle(const std::string& title);

    bool find();
    const WindowInfo& info() const;

    // Refresh position/size; call once per loop iteration.
    bool update();

    // True when the window exists and is not minimized.
    bool isReady() const;

private:
    std::string target_title_;
    WindowInfo info_;

    static BOOL CALLBACK enumCallback(HWND hwnd, LPARAM lparam);
};
