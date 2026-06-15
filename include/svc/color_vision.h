#pragma once
// ============================================================================
// color_vision - Generic real-time color/ROI vision primitives over OpenCV.
//
// These are the reusable building blocks for reading a game/app UI from a
// captured frame, with no application-specific logic:
//
//   - Resolution-independent ROIs (ratios 0..1 -> pixels per frame).
//   - HSV masking with hue wraparound (red lives at H=0-10 AND H=170-179).
//   - Bar-fill measurement by column projection, robust to text drawn over
//     the bar.
//   - Blob detection (centroid / count) via connected components.
//   - Template-free icon-state classification by brightness/contrast.
// ============================================================================

#include <opencv2/opencv.hpp>
#include <string>

namespace sv {

// Rectangle stored as ratios (0.0-1.0) of the frame size, so the same
// configuration works at any resolution.
struct RoiRatio {
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;

    cv::Rect toPixels(const cv::Size& frame_size) const;
    bool valid() const { return w > 0.0 && h > 0.0; }
};

// A region of interest plus one or two HSV ranges. The optional second range
// handles hue wraparound (e.g. red appears at both ends of the hue circle).
struct ColorTarget {
    RoiRatio roi;
    cv::Scalar hsv_low  = cv::Scalar(0, 0, 0);
    cv::Scalar hsv_high = cv::Scalar(179, 255, 255);

    bool has_wraparound = false;
    cv::Scalar hsv_low2 = cv::Scalar(0, 0, 0);
    cv::Scalar hsv_high2 = cv::Scalar(0, 0, 0);
};

enum class SlotState {
    Unknown,
    Empty,     // no icon: flat, very low contrast
    Ready,     // lit icon: high ratio of bright pixels
    Cooldown,  // partly lit + dark sweep: high contrast, not ready
    Disabled,  // uniformly dimmed: low contrast but not flat
};

struct SlotReading {
    SlotState state = SlotState::Unknown;
    cv::Rect roi;
    float mean_value = 0.0f;    // mean HSV value (0-255)
    float contrast = 0.0f;      // std-dev of value
    float highlight_pct = 0.0f; // percentage of bright pixels
};

// Thresholds for readSlotState, grouped for clarity. Defaults are sane starting
// points; tune against your own labelled samples.
struct SlotThresholds {
    int highlight_value = 150;  // V for a pixel to count as "bright"
    float empty_std = 16.0f;    // contrast below this => Empty
    float ready_hl = 27.0f;     // highlight % at/above this => Ready
    float cooldown_std = 43.0f; // contrast at/above this (and not ready) => Cooldown
};

// Clip a ROI to the frame; returns an empty Mat when the result has no area.
cv::Mat safeRoi(const cv::Mat& frame, const cv::Rect& roi);

// inRange with the primary band, OR'd with the secondary band when wraparound
// is enabled. The input must be an HSV image.
cv::Mat buildColorMask(const cv::Mat& hsv, const ColorTarget& target);

// Fill ratio (0-100) of a horizontal bar, robust to text drawn over it. Uses a
// per-column max projection: a column counts as filled if ANY pixel in it
// matches, so a "1234/5678" overlay does not punch holes in the reading.
// Returns -1 when the ROI is empty.
float barFillPercent(const cv::Mat& frame, const ColorTarget& target);

// Centroid (in frame coordinates) of the colored mass inside the ROI, after a
// morphological open. Returns false when the matched area is below min_area.
bool blobCentroid(const cv::Mat& frame, const ColorTarget& target,
                  int min_area, int morph_kernel, cv::Point& out_center);

// Count distinct colored blobs (connected components) at/above min_area.
int countBlobs(const cv::Mat& frame, const ColorTarget& target, int min_area);

// Classify an icon slot by brightness/contrast without any template:
// flat => Empty, bright => Ready, high-contrast => Cooldown, else Disabled.
SlotReading readSlotState(const cv::Mat& frame, const cv::Rect& slot,
                          const SlotThresholds& thr = SlotThresholds{});

// Draw a labelled ROI rectangle (debug helper).
void drawRoi(cv::Mat& frame, const cv::Rect& roi, const cv::Scalar& color,
             const std::string& label = "");

} // namespace sv
