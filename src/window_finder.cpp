#include "svc/window_finder.h"
#include "svc/logger.h"

namespace {

std::string asciiLower(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

} // namespace

void WindowFinder::setTargetTitle(const std::string& title) {
    target_title_ = title;
    LOG_INFO("target title set: \"" + title + "\"");
}

// Try an exact match (FindWindowA), then a partial match (EnumWindows).
bool WindowFinder::find() {
    if (target_title_.empty()) {
        LOG_ERROR("target title not set");
        return false;
    }

    HWND hwnd = FindWindowA(nullptr, target_title_.c_str());
    if (hwnd) {
        info_.hwnd = hwnd;
        info_.valid = true;
        return update();
    }

    info_.hwnd = nullptr;
    EnumWindows(enumCallback, reinterpret_cast<LPARAM>(this));

    if (info_.hwnd) {
        info_.valid = true;
        return update();
    }

    LOG_WARN("window not found: \"" + target_title_ + "\"");
    info_.valid = false;
    return false;
}

BOOL CALLBACK WindowFinder::enumCallback(HWND hwnd, LPARAM lparam) {
    auto* self = reinterpret_cast<WindowFinder*>(lparam);

    char title[MAX_TITLE_LENGTH];
    GetWindowTextA(hwnd, title, sizeof(title));

    std::string window_title(title);
    std::string haystack = asciiLower(window_title);
    std::string needle = asciiLower(self->target_title_);
    if (!window_title.empty() && haystack.find(needle) != std::string::npos) {
        self->info_.hwnd = hwnd;
        self->info_.title = window_title;
        return FALSE;  // stop enumeration
    }
    return TRUE;
}

bool WindowFinder::update() {
    if (!info_.hwnd || !IsWindow(info_.hwnd)) {
        info_.valid = false;
        return false;
    }

    info_.minimized = IsIconic(info_.hwnd) != 0;

    RECT client_rect;
    if (!GetClientRect(info_.hwnd, &client_rect)) {
        info_.valid = false;
        return false;
    }

    POINT top_left = { 0, 0 };
    ClientToScreen(info_.hwnd, &top_left);

    info_.x = top_left.x;
    info_.y = top_left.y;
    info_.width = client_rect.right - client_rect.left;
    info_.height = client_rect.bottom - client_rect.top;
    info_.valid = true;
    return true;
}

const WindowInfo& WindowFinder::info() const {
    return info_;
}

bool WindowFinder::isReady() const {
    return info_.valid && !info_.minimized && info_.hwnd != nullptr;
}
