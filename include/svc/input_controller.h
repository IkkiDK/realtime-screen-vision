#pragma once
// ============================================================================
// InputController - Send keyboard and mouse input to a target window.
//
// Two delivery modes:
//
//   1. Foreground (SendInput): simulates real hardware input. Works for any
//      app but requires the target window to be focused. Most compatible.
//
//   2. Background (PostMessage): posts WM_KEY*/WM_MOUSE* directly to the HWND,
//      no focus needed. Works for plain Win32 apps, but many games that read
//      raw hardware input ignore posted messages.
//
// Keyboard events use hardware scan codes (KEYEVENTF_SCANCODE), which is the
// most broadly accepted form of synthetic input.
// ============================================================================

#include <windows.h>
#include <mutex>
#include <set>
#include <string>
#include <vector>

// Absolute mouse coordinates for SendInput range from 0 to MOUSE_COORD_MAX.
static constexpr LONG MOUSE_COORD_MAX = 65535;

class InputController {
public:
    InputController();
    ~InputController();

    void setTargetWindow(HWND hwnd);

    // Dry run: log actions without sending any real input.
    void setDryRun(bool enabled);
    bool isDryRun() const;

    // true = PostMessage (no focus needed), false = SendInput (needs focus).
    void setBackgroundMode(bool enabled);
    bool isBackgroundMode() const;

    // ---- Keyboard ----------------------------------------------------------
    void pressKey(WORD vk_code);
    void pressChord(WORD vk_code, bool ctrl, bool alt, bool shift);
    // Short timings, for latency-sensitive sequences.
    void pressChordQuick(WORD vk_code, bool ctrl, bool alt, bool shift);
    void keyDown(WORD vk_code);
    void keyUp(WORD vk_code);

    // ---- Mouse -------------------------------------------------------------
    void click(int rel_x, int rel_y);
    void rightClick(int rel_x, int rel_y);
    void moveMouse(int rel_x, int rel_y);

    // Press at (x1,y1), interpolate to (x2,y2) over hold_ms, release.
    void drag(int x1, int y1, int x2, int y2, int hold_ms, bool right);

    // Replay a recorded gesture: press at the first point, walk the polyline
    // honouring the relative timings (t_ms since gesture start), release at the
    // last point. t_ms must match points in size; if empty, uses ~16ms steps.
    void dragPath(const std::vector<POINT>& points, const std::vector<int>& t_ms, bool right);

    // Position the cursor and turn the wheel. delta is in WHEEL_DELTA (120)
    // multiples; positive scrolls forward.
    void scroll(int rel_x, int rel_y, int delta);

    // ---- Safety ------------------------------------------------------------
    // Release every key currently held. Call on shutdown and on error.
    void releaseAllKeys();
    std::vector<WORD> getHeldKeys() const;

private:
    HWND target_hwnd_ = nullptr;
    bool dry_run_ = false;
    bool background_mode_ = false;
    mutable std::mutex mutex_;
    std::set<WORD> held_keys_;

    POINT toScreen(int rel_x, int rel_y);
    void logAction(const std::string& action);

    void pressKeyWithDelay(WORD vk_code, int base_hold_ms, int hold_variation_ms);
    void pressChordWithTimings(WORD vk_code, bool ctrl, bool alt, bool shift,
                               int base_hold_ms, int hold_variation_ms,
                               int mod_down_delay_ms, int mod_down_variation_ms,
                               int mod_up_delay_ms, int mod_up_variation_ms);
};
