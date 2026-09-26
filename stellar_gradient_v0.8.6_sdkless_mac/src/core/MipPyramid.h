#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>
#include "Parallel.h"

namespace stellar {

struct MipLevelRGBA {
    int width = 0;
    int height = 0;
    std::vector<float> pixels; // tightly packed RGBA float
};

class MipPyramidRGBA {
public:
    // Logical clear keeps allocated storage for the next frame. This is
    // intentional: AE renders many frames with identical dimensions.
    void clear() { active_levels_ = 0; }

    float* prepare_level0(int width, int height) {
        active_levels_ = 0;
        std::size_t floats = 0;
        if (!checked_rgba_float_count(width, height, &floats)) return nullptr;
        ensure_level_slot(0);
        auto& level = levels_[0];
        level.width = width;
        level.height = height;
        level.pixels.resize(floats);
        active_levels_ = 1;
        return level.pixels.data();
    }

    std::size_t retained_bytes() const {
        std::size_t total = 0;
        for (const auto& level : levels_) {
            const std::size_t bytes = level.pixels.capacity() * sizeof(float);
            if (bytes > std::numeric_limits<std::size_t>::max() - total) return std::numeric_limits<std::size_t>::max();
            total += bytes;
        }
        return total;
    }

    void release_storage() {
        levels_.clear();
        levels_.shrink_to_fit();
        active_levels_ = 0;
    }

    void trim_retained_bytes(std::size_t max_bytes) {
        if (retained_bytes() > max_bytes) release_storage();
    }

    static bool estimate_storage_bytes(int width, int height, int max_levels, std::size_t* out_bytes) {
        if (!out_bytes || width <= 0 || height <= 0) return false;
        std::size_t total = 0;
        int w = width, h = height, levels = 0;
        while (w > 0 && h > 0 && (max_levels <= 0 || levels < max_levels)) {
            std::size_t floats = 0;
            if (!checked_rgba_float_count(w, h, &floats)) return false;
            if (floats > std::numeric_limits<std::size_t>::max() / sizeof(float)) return false;
            const std::size_t bytes = floats * sizeof(float);
            if (bytes > std::numeric_limits<std::size_t>::max() - total) return false;
            total += bytes;
            ++levels;
            if (w == 1 && h == 1) break;
            w = std::max(1, (w + 1) / 2);
            h = std::max(1, (h + 1) / 2);
        }
        *out_bytes = total;
        return true;
    }

    void build_copy(const float* rgba, int width, int height, int stride_floats, int max_levels = 0) {
        if (!rgba || width <= 0 || height <= 0 || stride_floats < width * 4) { clear(); return; }
        float* level0 = prepare_level0(width, height);
        if (!level0) return;
        for (int y = 0; y < height; ++y) {
            std::copy_n(rgba + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride_floats),
                        static_cast<std::size_t>(width) * 4u,
                        level0 + static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 4u);
        }
        append_downsampled_levels(max_levels);
    }

    void build_owned(std::vector<float>&& level0, int width, int height, int max_levels = 0) {
        active_levels_ = 0;
        if (width <= 0 || height <= 0) return;
        std::size_t expected = 0;
        if (!checked_rgba_float_count(width, height, &expected) || level0.size() != expected) return;
        ensure_level_slot(0);
        auto& dst = levels_[0];
        dst.width = width;
        dst.height = height;
        dst.pixels = std::move(level0);
        active_levels_ = 1;
        append_downsampled_levels(max_levels);
    }

    void generate_mips(int max_levels = 0) {
        if (active_levels_ == 1) append_downsampled_levels(max_levels);
    }

    int levels() const { return active_levels_; }

    void sample_lod(float x, float y, float lod, float out[4]) const {
        if (active_levels_ <= 0) {
            for (int c = 0; c < 4; ++c) out[c] = 0.0f;
            return;
        }
        const float cl = std::clamp(lod, 0.0f, static_cast<float>(active_levels_ - 1));
        const int l0 = static_cast<int>(std::floor(cl));
        const int l1 = std::min(l0 + 1, active_levels_ - 1);
        const float t = cl - static_cast<float>(l0);
        float a[4], b[4];
        sample_level(x, y, l0, a);
        sample_level(x, y, l1, b);
        for (int c = 0; c < 4; ++c) out[c] = a[c] + (b[c] - a[c]) * t;
    }

private:
    static bool checked_rgba_float_count(int width, int height, std::size_t* out) {
        if (!out || width <= 0 || height <= 0) return false;
        const std::size_t w = static_cast<std::size_t>(width);
        const std::size_t h = static_cast<std::size_t>(height);
        constexpr std::size_t channels = 4u;
        if (w > std::numeric_limits<std::size_t>::max() / h) return false;
        const std::size_t pixels = w * h;
        if (pixels > std::numeric_limits<std::size_t>::max() / channels) return false;
        *out = pixels * channels;
        return true;
    }

    void ensure_level_slot(int index) {
        while (static_cast<int>(levels_.size()) <= index) levels_.emplace_back();
    }

    void append_downsampled_levels(int max_levels) {
        while (active_levels_ > 0) {
            const int src_index = active_levels_ - 1;
            const int sw = levels_[static_cast<std::size_t>(src_index)].width;
            const int sh = levels_[static_cast<std::size_t>(src_index)].height;
            if ((sw <= 1 && sh <= 1) || (max_levels > 0 && active_levels_ >= max_levels)) break;

            const int dst_index = active_levels_;
            ensure_level_slot(dst_index);
            const auto& src = levels_[static_cast<std::size_t>(src_index)];
            auto& dst = levels_[static_cast<std::size_t>(dst_index)];
            dst.width = std::max(1, (src.width + 1) / 2);
            dst.height = std::max(1, (src.height + 1) / 2);
            dst.pixels.resize(static_cast<std::size_t>(dst.width) * static_cast<std::size_t>(dst.height) * 4u);

            parallel_rows(0, dst.height, [&](int y0, int y1) {
                for (int y = y0; y < y1; ++y) {
                    for (int x = 0; x < dst.width; ++x) {
                        float acc[4] = {0, 0, 0, 0};
                        int n = 0;
                        for (int oy = 0; oy < 2; ++oy) {
                            for (int ox = 0; ox < 2; ++ox) {
                                const int sx = x * 2 + ox;
                                const int sy = y * 2 + oy;
                                if (sx >= src.width || sy >= src.height) continue;
                                const float* px = &src.pixels[(static_cast<std::size_t>(sy) * static_cast<std::size_t>(src.width) + static_cast<std::size_t>(sx)) * 4u];
                                for (int c = 0; c < 4; ++c) acc[c] += px[c];
                                ++n;
                            }
                        }
                        float* q = &dst.pixels[(static_cast<std::size_t>(y) * static_cast<std::size_t>(dst.width) + static_cast<std::size_t>(x)) * 4u];
                        const float inv = 1.0f / static_cast<float>(std::max(1, n));
                        for (int c = 0; c < 4; ++c) q[c] = acc[c] * inv;
                    }
                }
            }, 192);
            ++active_levels_;
        }
    }

    void sample_level(float x, float y, int level, float out[4]) const {
        const auto& m = levels_[static_cast<std::size_t>(level)];
        const float scale = std::ldexp(1.0f, -level);
        float fx = (x + 0.5f) * scale - 0.5f;
        float fy = (y + 0.5f) * scale - 0.5f;
        fx = std::clamp(fx, 0.0f, static_cast<float>(m.width - 1));
        fy = std::clamp(fy, 0.0f, static_cast<float>(m.height - 1));
        const int x0 = static_cast<int>(std::floor(fx));
        const int y0 = static_cast<int>(std::floor(fy));
        const int x1 = std::min(x0 + 1, m.width - 1);
        const int y1 = std::min(y0 + 1, m.height - 1);
        const float tx = fx - static_cast<float>(x0);
        const float ty = fy - static_cast<float>(y0);
        const float* p00 = &m.pixels[(static_cast<std::size_t>(y0) * static_cast<std::size_t>(m.width) + static_cast<std::size_t>(x0)) * 4u];
        const float* p10 = &m.pixels[(static_cast<std::size_t>(y0) * static_cast<std::size_t>(m.width) + static_cast<std::size_t>(x1)) * 4u];
        const float* p01 = &m.pixels[(static_cast<std::size_t>(y1) * static_cast<std::size_t>(m.width) + static_cast<std::size_t>(x0)) * 4u];
        const float* p11 = &m.pixels[(static_cast<std::size_t>(y1) * static_cast<std::size_t>(m.width) + static_cast<std::size_t>(x1)) * 4u];
        for (int c = 0; c < 4; ++c) {
            const float top = p00[c] + (p10[c] - p00[c]) * tx;
            const float bot = p01[c] + (p11[c] - p01[c]) * tx;
            out[c] = top + (bot - top) * ty;
        }
    }

    std::vector<MipLevelRGBA> levels_;
    int active_levels_ = 0;
};

inline float radius_to_lod(float radius_px) {
    return radius_px <= 1.0f ? 0.0f : std::log2(radius_px);
}

} // namespace stellar
