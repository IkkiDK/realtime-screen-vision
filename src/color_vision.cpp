#include "svc/color_vision.h"

#include <algorithm>
#include <cmath>

namespace sv {

cv::Rect RoiRatio::toPixels(const cv::Size& frame_size) const {
    return cv::Rect(
        static_cast<int>(std::lround(x * frame_size.width)),
        static_cast<int>(std::lround(y * frame_size.height)),
        static_cast<int>(std::lround(w * frame_size.width)),
        static_cast<int>(std::lround(h * frame_size.height)));
}

cv::Mat safeRoi(const cv::Mat& frame, const cv::Rect& roi) {
    cv::Rect clipped = roi & cv::Rect(0, 0, frame.cols, frame.rows);
    if (clipped.width <= 0 || clipped.height <= 0) return cv::Mat();
    return frame(clipped);
}

cv::Mat buildColorMask(const cv::Mat& hsv, const ColorTarget& target) {
    cv::Mat mask;
    cv::inRange(hsv, target.hsv_low, target.hsv_high, mask);
    if (target.has_wraparound) {
        cv::Mat mask2;
        cv::inRange(hsv, target.hsv_low2, target.hsv_high2, mask2);
        cv::bitwise_or(mask, mask2, mask);
    }
    return mask;
}

float barFillPercent(const cv::Mat& frame, const ColorTarget& target) {
    cv::Mat roi = safeRoi(frame, target.roi.toPixels(frame.size()));
    if (roi.empty()) return -1.0f;

    cv::Mat hsv;
    cv::cvtColor(roi, hsv, cv::COLOR_BGR2HSV);
    cv::Mat mask = buildColorMask(hsv, target);

    // Collapse rows: each column becomes "any matching pixel?" (REDUCE_MAX).
    cv::Mat col_profile;
    cv::reduce(mask, col_profile, 0, cv::REDUCE_MAX);
    if (col_profile.cols == 0) return -1.0f;

    int filled = cv::countNonZero(col_profile);
    float percent = (static_cast<float>(filled) / col_profile.cols) * 100.0f;
    return std::clamp(percent, 0.0f, 100.0f);
}

bool blobCentroid(const cv::Mat& frame, const ColorTarget& target,
                  int min_area, int morph_kernel, cv::Point& out_center) {
    cv::Rect roi_px = target.roi.toPixels(frame.size());
    cv::Mat roi = safeRoi(frame, roi_px);
    if (roi.empty()) return false;

    cv::Mat hsv;
    cv::cvtColor(roi, hsv, cv::COLOR_BGR2HSV);
    cv::Mat mask = buildColorMask(hsv, target);

    if (morph_kernel > 0) {
        cv::Mat kernel = cv::getStructuringElement(
            cv::MORPH_ELLIPSE, cv::Size(morph_kernel, morph_kernel));
        cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);
    }

    if (cv::countNonZero(mask) < min_area) return false;

    cv::Moments m = cv::moments(mask, true);
    if (m.m00 <= 0.0) return false;
    out_center.x = roi_px.x + static_cast<int>(m.m10 / m.m00);
    out_center.y = roi_px.y + static_cast<int>(m.m01 / m.m00);
    return true;
}

int countBlobs(const cv::Mat& frame, const ColorTarget& target, int min_area) {
    cv::Mat roi = safeRoi(frame, target.roi.toPixels(frame.size()));
    if (roi.empty()) return 0;

    cv::Mat hsv;
    cv::cvtColor(roi, hsv, cv::COLOR_BGR2HSV);
    cv::Mat mask = buildColorMask(hsv, target);

    // Small kernel: blobs are tiny; a large kernel would erase them.
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);

    cv::Mat labels, stats, centroids;
    int n = cv::connectedComponentsWithStats(mask, labels, stats, centroids, 8, CV_32S);
    int count = 0;
    for (int i = 1; i < n; ++i) {  // skip label 0 (background)
        if (stats.at<int>(i, cv::CC_STAT_AREA) >= min_area) ++count;
    }
    return count;
}

SlotReading readSlotState(const cv::Mat& frame, const cv::Rect& slot,
                          const SlotThresholds& thr) {
    SlotReading reading;
    reading.roi = slot;

    cv::Mat roi = safeRoi(frame, slot);
    if (roi.empty() || roi.cols < 3 || roi.rows < 3) return reading;

    // Sample the inner area to avoid the slot border/frame.
    const int mx = std::max(1, roi.cols / 8);
    const int my = std::max(1, roi.rows / 8);
    cv::Rect inner(mx, my, roi.cols - mx * 2, roi.rows - my * 2);
    if (inner.width < 3 || inner.height < 3) {
        inner = cv::Rect(0, 0, roi.cols, roi.rows);
    }
    cv::Mat sample = roi(inner);

    cv::Mat hsv;
    cv::cvtColor(sample, hsv, cv::COLOR_BGR2HSV);

    double sum_v = 0.0, sumsq_v = 0.0;
    int highlight = 0;
    const int total = sample.rows * sample.cols;
    for (int y = 0; y < sample.rows; ++y) {
        const cv::Vec3b* row = hsv.ptr<cv::Vec3b>(y);
        for (int x = 0; x < sample.cols; ++x) {
            const int v = row[x][2];
            sum_v += v;
            sumsq_v += static_cast<double>(v) * v;
            if (v >= thr.highlight_value) ++highlight;
        }
    }

    const float mean_v = static_cast<float>(sum_v / total);
    const float var_v = static_cast<float>(sumsq_v / total) - mean_v * mean_v;
    reading.mean_value = mean_v;
    reading.contrast = var_v > 0.0f ? std::sqrt(var_v) : 0.0f;
    reading.highlight_pct = (static_cast<float>(highlight) / total) * 100.0f;

    // Key signal is CONTRAST: a disabled icon is uniformly dim (low contrast);
    // a cooldown has a lit part plus a dark sweep (high contrast); a ready icon
    // is bright (high highlight ratio); an empty slot is flat.
    if (reading.contrast < thr.empty_std) {
        reading.state = SlotState::Empty;
    } else if (reading.highlight_pct >= thr.ready_hl) {
        reading.state = SlotState::Ready;
    } else if (reading.contrast >= thr.cooldown_std) {
        reading.state = SlotState::Cooldown;
    } else {
        reading.state = SlotState::Disabled;
    }
    return reading;
}

void drawRoi(cv::Mat& frame, const cv::Rect& roi, const cv::Scalar& color,
             const std::string& label) {
    cv::rectangle(frame, roi, color, 2);
    if (!label.empty()) {
        cv::putText(frame, label, cv::Point(roi.x, std::max(0, roi.y - 5)),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);
    }
}

} // namespace sv
