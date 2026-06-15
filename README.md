# screen-vision-cpp

Real-time **screen vision + input automation** primitives in modern C++ (C++17)
on top of OpenCV and the Win32 API.

The library reads a live application window — captures its client area, finds
colored UI elements, and measures their state — and can drive keyboard/mouse
input back into it. It is the reusable, application-agnostic core extracted from
a larger computer-vision automation project.

> ⚠️ **License:** source published for portfolio/evaluation only — all rights
> reserved. See [LICENSE](LICENSE). Not open source; no reuse permitted.

---

## Highlights

A few techniques worth a look:

- **Resolution-independent ROIs.** Regions of interest are stored as ratios
  (`0..1`) of the frame and converted to pixels per frame, so the same
  configuration works at 800×600 and 1920×1080 without retuning.

- **HSV masking with hue wraparound.** Red occupies *both* ends of the hue
  circle (`H≈0–10` and `H≈170–179`). `buildColorMask` OR-combines two HSV bands
  so wrapping colors are detected as one.

- **Bar fill by column projection.** Reading a health/progress bar by counting
  matching pixels breaks when text (`1234/5678`) is drawn over it. Instead, a
  per-column **max** projection (`cv::reduce` + `REDUCE_MAX`) marks a column as
  filled if *any* pixel in it matches — the overlay text no longer punches holes
  in the measurement.

- **Template-free icon state by brightness/contrast.** `readSlotState`
  classifies an icon as ready / on-cooldown / disabled / empty from the mean and
  standard deviation of its brightness, plus a highlight ratio — no template
  images, no per-icon training. Cheap enough to run every frame on weak
  hardware.

- **GDI window capture.** Client-area capture via
  `GetDC → CreateCompatibleDC → BitBlt → GetDIBits → cv::Mat`, with careful GDI
  object cleanup.

- **Humanized input.** `InputController` sends hardware **scan-code** keystrokes
  and absolute-coordinate mouse input via `SendInput`, with jittered timings and
  recorded-gesture (`dragPath`) replay. A `PostMessage` background mode and a
  dry-run mode are included.

## Layout

```
include/svc/      public headers
  color_vision.h    ROIs, HSV masks, bar fill, blobs, slot state
  window_finder.h   locate/track a window by title (Win32)
  window_capture.h  GDI client-area capture -> cv::Mat
  input_controller.h keyboard/mouse via SendInput / PostMessage
  timer.h / logger.h small utilities
src/              implementations
examples/
  bar_reader.cpp    live demo: read a colored bar from any window
```

## Build

Requirements: a C++17 compiler (MSVC), CMake ≥ 3.15, and OpenCV (Windows).

```powershell
# point CMake at your OpenCV build
cmake -S . -B build -DOpenCV_DIR=C:/opencv/build
cmake --build build --config Release
```

`scripts/fetch-deps.ps1` checks that OpenCV is reachable and prints guidance if
it is not.

## Demo

`bar_reader` attaches to any window by title and prints, live, how full a
colored bar is inside a region of interest:

```powershell
# measure any bright/saturated bar in the top-left strip of a "Game" window
build\Release\bar_reader.exe "Game"

# explicit ROI (ratios) + HSV bounds for a red bar
build\Release\bar_reader.exe "Game" 0.04 0.015 0.11 0.02 0 120 100 10 255 255
```

## Notes

- Windows-only (Win32 + GDI). The vision primitives themselves are pure OpenCV
  and portable; capture and input are platform-specific.
- Comments and API are in English; the code favors clarity and small, direct
  functions over abstraction.
