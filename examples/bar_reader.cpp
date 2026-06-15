// ============================================================================
// bar_reader - Live demo of the vision + capture primitives.
//
// Attaches to any window by title, captures its client area each frame, and
// prints how full a colored bar is inside a region of interest. No game- or
// app-specific logic: it just measures a horizontal colored bar.
//
// Usage:
//   bar_reader "<window title>" [x y w h] [hLo sLo vLo hHi sHi vHi]
//
//   x y w h        ROI as ratios of the frame (0..1). Default: top-left strip.
//   hLo..vHi       HSV lower/upper bounds. Default: any bright, saturated color.
//
// Example (read the HP bar of a window titled "Game"):
//   bar_reader "Game" 0.04 0.015 0.11 0.02 0 120 100 10 255 255
// ============================================================================

#include "svc/color_vision.h"
#include "svc/timer.h"
#include "svc/window_capture.h"
#include "svc/window_finder.h"

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
volatile std::sig_atomic_t g_stop = 0;
void onSignal(int) { g_stop = 1; }
}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0]
                  << " \"<window title>\" [x y w h] [hLo sLo vLo hHi sHi vHi]\n";
        return 1;
    }
    std::signal(SIGINT, onSignal);

    const std::string title = argv[1];

    sv::ColorTarget target;
    target.roi = {0.04, 0.015, 0.11, 0.02};       // top-left strip by default
    target.hsv_low = cv::Scalar(0, 80, 90);       // any bright, saturated color
    target.hsv_high = cv::Scalar(179, 255, 255);

    if (argc >= 6) {
        target.roi = {std::atof(argv[2]), std::atof(argv[3]),
                      std::atof(argv[4]), std::atof(argv[5])};
    }
    if (argc >= 12) {
        target.hsv_low = cv::Scalar(std::atof(argv[6]), std::atof(argv[7]), std::atof(argv[8]));
        target.hsv_high = cv::Scalar(std::atof(argv[9]), std::atof(argv[10]), std::atof(argv[11]));
    }

    WindowFinder finder;
    finder.setTargetTitle(title);
    if (!finder.find()) {
        std::cerr << "window not found: " << title << "\n";
        return 2;
    }

    WindowCapture capture;
    std::cout << "Reading bar fill in \"" << title << "\". Press Ctrl+C to stop.\n";

    while (!g_stop) {
        if (!finder.update() || !finder.isReady()) {
            std::cout << "\rwindow not ready...                 " << std::flush;
            Timer::delay(200);
            continue;
        }
        cv::Mat frame = capture.capture(finder.info());
        if (frame.empty()) {
            Timer::delay(50);
            continue;
        }

        float pct = sv::barFillPercent(frame, target);
        std::cout << "\rbar fill: " << (pct < 0 ? -1 : static_cast<int>(pct))
                  << "%   frame " << frame.cols << "x" << frame.rows << "      "
                  << std::flush;
        Timer::delay(100);
    }

    std::cout << "\nstopped.\n";
    return 0;
}
