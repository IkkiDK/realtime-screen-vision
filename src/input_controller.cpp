#include "svc/input_controller.h"
#include "svc/logger.h"
#include "svc/timer.h"

#include <iomanip>
#include <sstream>

static std::string vkHex(WORD vk) {
    std::ostringstream os;
    os << std::hex << std::uppercase << static_cast<int>(vk);
    return os.str();
}

static constexpr int KEY_PRESS_BASE_MS = 80;
static constexpr int KEY_PRESS_VARIATION_MS = 30;
static constexpr int QUICK_KEY_PRESS_MS = 12;
static constexpr int QUICK_MOD_DOWN_DELAY_MS = 8;
static constexpr int QUICK_MOD_UP_DELAY_MS = 4;
static constexpr int CLICK_DELAY_BASE_MS = 30;
static constexpr int CLICK_DELAY_VARIATION_MS = 10;

InputController::InputController() {}

InputController::~InputController() {
    releaseAllKeys();
}

void InputController::setTargetWindow(HWND hwnd) {
    std::lock_guard<std::mutex> lock(mutex_);
    target_hwnd_ = hwnd;
}

void InputController::setDryRun(bool enabled) {
    dry_run_ = enabled;
    LOG_INFO(std::string("dry run: ") + (enabled ? "on" : "off"));
}

bool InputController::isDryRun() const {
    return dry_run_;
}

void InputController::setBackgroundMode(bool enabled) {
    background_mode_ = enabled;
    LOG_INFO(std::string("background mode: ") + (enabled ? "on (PostMessage)" : "off (SendInput)"));
}

bool InputController::isBackgroundMode() const {
    return background_mode_;
}

// Build lParam for WM_KEYDOWN/WM_KEYUP per MSDN:
//   bits 0-15  repeat count (1)
//   bits 16-23 scan code
//   bit 30     previous key state (0 down, 1 up)
//   bit 31     transition state (0 down, 1 up)
static LPARAM buildKeyLParam(WORD vk_code, bool key_up) {
    UINT scan = MapVirtualKey(vk_code, MAPVK_VK_TO_VSC);
    LPARAM lp = 1;
    lp |= (static_cast<LPARAM>(scan) & 0xFF) << 16;
    if (key_up) {
        lp |= (1LL << 30);
        lp |= (1LL << 31);
    }
    return lp;
}

void InputController::pressKey(WORD vk_code) {
    pressKeyWithDelay(vk_code, KEY_PRESS_BASE_MS, KEY_PRESS_VARIATION_MS);
}

void InputController::pressChord(WORD vk_code, bool ctrl, bool alt, bool shift) {
    pressChordWithTimings(vk_code, ctrl, alt, shift,
                          KEY_PRESS_BASE_MS, KEY_PRESS_VARIATION_MS,
                          25, 8, 15, 5);
}

void InputController::pressChordQuick(WORD vk_code, bool ctrl, bool alt, bool shift) {
    pressChordWithTimings(vk_code, ctrl, alt, shift,
                          QUICK_KEY_PRESS_MS, 0,
                          QUICK_MOD_DOWN_DELAY_MS, 0,
                          QUICK_MOD_UP_DELAY_MS, 0);
}

void InputController::pressKeyWithDelay(WORD vk_code, int base_hold_ms, int hold_variation_ms) {
    if (vk_code == 0) return;
    keyDown(vk_code);
    if (hold_variation_ms > 0) {
        Timer::humanDelay(base_hold_ms, hold_variation_ms);
    } else if (base_hold_ms > 0) {
        Timer::delay(base_hold_ms);
    }
    keyUp(vk_code);
}

void InputController::pressChordWithTimings(WORD vk_code, bool ctrl, bool alt, bool shift,
                                            int base_hold_ms, int hold_variation_ms,
                                            int mod_down_delay_ms, int mod_down_variation_ms,
                                            int mod_up_delay_ms, int mod_up_variation_ms) {
    if (vk_code == 0) return;
    std::vector<WORD> mods;
    if (ctrl && vk_code != VK_CONTROL) mods.push_back(VK_CONTROL);
    if (alt && vk_code != VK_MENU) mods.push_back(VK_MENU);
    if (shift && vk_code != VK_SHIFT) mods.push_back(VK_SHIFT);

    for (WORD mod : mods) keyDown(mod);
    if (!mods.empty()) {
        if (mod_down_variation_ms > 0) {
            Timer::humanDelay(mod_down_delay_ms, mod_down_variation_ms);
        } else if (mod_down_delay_ms > 0) {
            Timer::delay(mod_down_delay_ms);
        }
    }
    pressKeyWithDelay(vk_code, base_hold_ms, hold_variation_ms);
    if (!mods.empty()) {
        if (mod_up_variation_ms > 0) {
            Timer::humanDelay(mod_up_delay_ms, mod_up_variation_ms);
        } else if (mod_up_delay_ms > 0) {
            Timer::delay(mod_up_delay_ms);
        }
    }
    for (auto it = mods.rbegin(); it != mods.rend(); ++it) keyUp(*it);
}

void InputController::keyDown(WORD vk_code) {
    logAction("KEY_DOWN: 0x" + vkHex(vk_code));

    {
        std::lock_guard<std::mutex> lock(mutex_);
        held_keys_.insert(vk_code);
    }

    if (dry_run_) return;

    if (background_mode_ && target_hwnd_) {
        LPARAM lp = buildKeyLParam(vk_code, false);
        if (!PostMessage(target_hwnd_, WM_KEYDOWN, vk_code, lp)) {
            LOG_WARN("PostMessage WM_KEYDOWN failed (err=" + std::to_string(GetLastError()) + ")");
        }
        return;
    }

    // Many apps and games only react to hardware scan codes, ignoring the
    // virtual key. wVk is ignored when KEYEVENTF_SCANCODE is set.
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = 0;
    input.ki.wScan = static_cast<WORD>(MapVirtualKey(vk_code, MAPVK_VK_TO_VSC));
    input.ki.dwFlags = KEYEVENTF_SCANCODE;
    if (SendInput(1, &input, sizeof(INPUT)) != 1) {
        LOG_WARN("SendInput KEY_DOWN blocked (err=" + std::to_string(GetLastError()) + ")");
    }
}

void InputController::keyUp(WORD vk_code) {
    logAction("KEY_UP: 0x" + vkHex(vk_code));

    {
        std::lock_guard<std::mutex> lock(mutex_);
        held_keys_.erase(vk_code);
    }

    if (dry_run_) return;

    if (background_mode_ && target_hwnd_) {
        LPARAM lp = buildKeyLParam(vk_code, true);
        if (!PostMessage(target_hwnd_, WM_KEYUP, vk_code, lp)) {
            LOG_WARN("PostMessage WM_KEYUP failed (err=" + std::to_string(GetLastError()) + ")");
        }
        return;
    }

    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = 0;
    input.ki.wScan = static_cast<WORD>(MapVirtualKey(vk_code, MAPVK_VK_TO_VSC));
    input.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
    if (SendInput(1, &input, sizeof(INPUT)) != 1) {
        LOG_WARN("SendInput KEY_UP blocked (err=" + std::to_string(GetLastError()) + ")");
    }
}

void InputController::click(int rel_x, int rel_y) {
    logAction("CLICK: (" + std::to_string(rel_x) + ", " + std::to_string(rel_y) + ")");
    if (dry_run_) return;

    if (background_mode_ && target_hwnd_) {
        LPARAM pos = MAKELPARAM(rel_x, rel_y);
        PostMessage(target_hwnd_, WM_MOUSEMOVE, 0, pos);
        Timer::humanDelay(CLICK_DELAY_BASE_MS, CLICK_DELAY_VARIATION_MS);
        PostMessage(target_hwnd_, WM_LBUTTONDOWN, MK_LBUTTON, pos);
        PostMessage(target_hwnd_, WM_LBUTTONUP, 0, pos);
        return;
    }

    POINT screen_pos = toScreen(rel_x, rel_y);
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    LONG norm_x = static_cast<LONG>((screen_pos.x * MOUSE_COORD_MAX) / screen_w);
    LONG norm_y = static_cast<LONG>((screen_pos.y * MOUSE_COORD_MAX) / screen_h);

    INPUT move = {};
    move.type = INPUT_MOUSE;
    move.mi.dx = norm_x;
    move.mi.dy = norm_y;
    move.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
    SendInput(1, &move, sizeof(INPUT));

    Timer::humanDelay(CLICK_DELAY_BASE_MS, CLICK_DELAY_VARIATION_MS);

    INPUT clicks[2] = {};
    clicks[0].type = INPUT_MOUSE;
    clicks[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    clicks[1].type = INPUT_MOUSE;
    clicks[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(2, clicks, sizeof(INPUT));
}

void InputController::rightClick(int rel_x, int rel_y) {
    logAction("RIGHT_CLICK: (" + std::to_string(rel_x) + ", " + std::to_string(rel_y) + ")");
    if (dry_run_) return;

    if (background_mode_ && target_hwnd_) {
        LPARAM pos = MAKELPARAM(rel_x, rel_y);
        PostMessage(target_hwnd_, WM_MOUSEMOVE, 0, pos);
        Timer::humanDelay(CLICK_DELAY_BASE_MS, CLICK_DELAY_VARIATION_MS);
        PostMessage(target_hwnd_, WM_RBUTTONDOWN, MK_RBUTTON, pos);
        PostMessage(target_hwnd_, WM_RBUTTONUP, 0, pos);
        return;
    }

    POINT screen_pos = toScreen(rel_x, rel_y);
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    LONG norm_x = static_cast<LONG>((screen_pos.x * MOUSE_COORD_MAX) / screen_w);
    LONG norm_y = static_cast<LONG>((screen_pos.y * MOUSE_COORD_MAX) / screen_h);

    INPUT move = {};
    move.type = INPUT_MOUSE;
    move.mi.dx = norm_x;
    move.mi.dy = norm_y;
    move.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
    SendInput(1, &move, sizeof(INPUT));

    Timer::humanDelay(CLICK_DELAY_BASE_MS, CLICK_DELAY_VARIATION_MS);

    INPUT clicks[2] = {};
    clicks[0].type = INPUT_MOUSE;
    clicks[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
    clicks[1].type = INPUT_MOUSE;
    clicks[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
    SendInput(2, clicks, sizeof(INPUT));
}

// Press at (x1,y1), interpolate the cursor to (x2,y2) in ~16ms (~60fps) steps,
// release. Useful for camera rotation or character steering gestures.
void InputController::drag(int x1, int y1, int x2, int y2, int hold_ms, bool right) {
    logAction("DRAG: (" + std::to_string(x1) + "," + std::to_string(y1)
              + ") -> (" + std::to_string(x2) + "," + std::to_string(y2)
              + ") hold=" + std::to_string(hold_ms) + "ms" + (right ? " right" : " left"));

    if (dry_run_) return;
    if (background_mode_) {
        LOG_WARN("drag: not supported in background mode, skipping");
        return;
    }

    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);

    auto sendMove = [&](int rel_x, int rel_y) {
        POINT s = toScreen(rel_x, rel_y);
        INPUT m = {};
        m.type = INPUT_MOUSE;
        m.mi.dx = static_cast<LONG>((s.x * MOUSE_COORD_MAX) / screen_w);
        m.mi.dy = static_cast<LONG>((s.y * MOUSE_COORD_MAX) / screen_h);
        m.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
        SendInput(1, &m, sizeof(INPUT));
    };

    sendMove(x1, y1);
    Timer::humanDelay(15, 5);

    INPUT down = {};
    down.type = INPUT_MOUSE;
    down.mi.dwFlags = right ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_LEFTDOWN;
    SendInput(1, &down, sizeof(INPUT));

    int duration = hold_ms > 0 ? hold_ms : 200;
    int steps = duration / 16;
    if (steps < 8) steps = 8;
    int step_ms = duration / steps;

    for (int i = 1; i <= steps; ++i) {
        int x = x1 + (x2 - x1) * i / steps;
        int y = y1 + (y2 - y1) * i / steps;
        sendMove(x, y);
        if (step_ms > 0) Sleep(step_ms);
    }

    INPUT up = {};
    up.type = INPUT_MOUSE;
    up.mi.dwFlags = right ? MOUSEEVENTF_RIGHTUP : MOUSEEVENTF_LEFTUP;
    SendInput(1, &up, sizeof(INPUT));
}

void InputController::dragPath(const std::vector<POINT>& points,
                               const std::vector<int>& t_ms, bool right) {
    if (points.size() < 2) return;
    logAction("DRAG_PATH: " + std::to_string(points.size()) + " pts" + (right ? " right" : " left"));

    if (dry_run_) return;
    if (background_mode_) {
        LOG_WARN("dragPath: not supported in background mode, skipping");
        return;
    }

    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);

    auto sendMove = [&](int rel_x, int rel_y) {
        POINT s = toScreen(rel_x, rel_y);
        INPUT m = {};
        m.type = INPUT_MOUSE;
        m.mi.dx = static_cast<LONG>((s.x * MOUSE_COORD_MAX) / screen_w);
        m.mi.dy = static_cast<LONG>((s.y * MOUSE_COORD_MAX) / screen_h);
        m.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
        SendInput(1, &m, sizeof(INPUT));
    };

    sendMove(points[0].x, points[0].y);
    Timer::humanDelay(15, 5);

    INPUT down = {};
    down.type = INPUT_MOUSE;
    down.mi.dwFlags = right ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_LEFTDOWN;
    SendInput(1, &down, sizeof(INPUT));

    const bool have_times = (t_ms.size() == points.size());
    for (size_t i = 1; i < points.size(); ++i) {
        sendMove(points[i].x, points[i].y);
        int wait = 16;
        if (have_times) {
            wait = t_ms[i] - t_ms[i - 1];
            if (wait < 1) wait = 1;
            if (wait > 2000) wait = 2000;  // guard against absurd recorded pauses
        }
        Sleep(static_cast<DWORD>(wait));
    }

    INPUT up = {};
    up.type = INPUT_MOUSE;
    up.mi.dwFlags = right ? MOUSEEVENTF_RIGHTUP : MOUSEEVENTF_LEFTUP;
    SendInput(1, &up, sizeof(INPUT));
}

void InputController::scroll(int rel_x, int rel_y, int delta) {
    logAction("SCROLL: (" + std::to_string(rel_x) + "," + std::to_string(rel_y)
              + ") delta=" + std::to_string(delta));

    if (dry_run_) return;
    if (background_mode_ && target_hwnd_) {
        LPARAM pos = MAKELPARAM(rel_x, rel_y);
        WPARAM wp = MAKEWPARAM(0, static_cast<WORD>(delta));
        PostMessage(target_hwnd_, WM_MOUSEWHEEL, wp, pos);
        return;
    }

    POINT screen_pos = toScreen(rel_x, rel_y);
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);

    INPUT in[2] = {};
    in[0].type = INPUT_MOUSE;
    in[0].mi.dx = static_cast<LONG>((screen_pos.x * MOUSE_COORD_MAX) / screen_w);
    in[0].mi.dy = static_cast<LONG>((screen_pos.y * MOUSE_COORD_MAX) / screen_h);
    in[0].mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
    in[1].type = INPUT_MOUSE;
    in[1].mi.dwFlags = MOUSEEVENTF_WHEEL;
    in[1].mi.mouseData = static_cast<DWORD>(delta);
    SendInput(2, in, sizeof(INPUT));
}

void InputController::moveMouse(int rel_x, int rel_y) {
    logAction("MOVE_MOUSE: (" + std::to_string(rel_x) + ", " + std::to_string(rel_y) + ")");
    if (dry_run_) return;

    if (background_mode_ && target_hwnd_) {
        PostMessage(target_hwnd_, WM_MOUSEMOVE, 0, MAKELPARAM(rel_x, rel_y));
        return;
    }

    POINT screen_pos = toScreen(rel_x, rel_y);
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);

    INPUT move = {};
    move.type = INPUT_MOUSE;
    move.mi.dx = static_cast<LONG>((screen_pos.x * MOUSE_COORD_MAX) / screen_w);
    move.mi.dy = static_cast<LONG>((screen_pos.y * MOUSE_COORD_MAX) / screen_h);
    move.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
    SendInput(1, &move, sizeof(INPUT));
}

void InputController::releaseAllKeys() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (held_keys_.empty()) return;

    LOG_WARN("releasing " + std::to_string(held_keys_.size()) + " held key(s)");

    for (WORD vk : held_keys_) {
        if (dry_run_) continue;
        if (background_mode_ && target_hwnd_) {
            PostMessage(target_hwnd_, WM_KEYUP, vk, buildKeyLParam(vk, true));
        } else {
            INPUT input = {};
            input.type = INPUT_KEYBOARD;
            input.ki.wVk = vk;
            input.ki.wScan = static_cast<WORD>(MapVirtualKey(vk, MAPVK_VK_TO_VSC));
            input.ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(1, &input, sizeof(INPUT));
        }
    }
    held_keys_.clear();
}

std::vector<WORD> InputController::getHeldKeys() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::vector<WORD>(held_keys_.begin(), held_keys_.end());
}

POINT InputController::toScreen(int rel_x, int rel_y) {
    POINT pt = { rel_x, rel_y };
    if (target_hwnd_) {
        ClientToScreen(target_hwnd_, &pt);
    }
    return pt;
}

void InputController::logAction(const std::string& action) {
    if (dry_run_) {
        LOG_INFO("[DRY RUN] " + action);
    } else {
        LOG_DEBUG(action);
    }
}
