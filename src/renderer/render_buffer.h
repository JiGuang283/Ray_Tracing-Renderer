#ifndef RENDER_BUFFER_H
#define RENDER_BUFFER_H

#include "vec3.h"
#include "aligned_allocator.h"
#include <vector>

class RenderBuffer {
  public:
    RenderBuffer(int width, int height)
        : m_width(width), m_height(height) {
        const size_t n = static_cast<size_t>(width) * height;
        r_.resize(n);
        g_.resize(n);
        b_.resize(n);
    }

    inline void set_pixel(int x, int y, const color &c) {
        if (x < 0 || x >= m_width || y < 0 || y >= m_height) return;
        size_t idx = static_cast<size_t>(y) * m_width + x;
        r_[idx] = c.x();
        g_[idx] = c.y();
        b_[idx] = c.z();
    }

    inline color get_pixel(int x, int y) const {
        size_t idx = static_cast<size_t>(y) * m_width + x;
        return color(r_[idx], g_[idx], b_[idx]);
    }

    const std::vector<float, AlignedAllocator<float, 32>>& r() const {
        return r_;
    }

    const std::vector<float, AlignedAllocator<float, 32>>& g() const {
        return g_;
    }

    const std::vector<float, AlignedAllocator<float, 32>>& b() const {
        return b_;
    }

    int width() const {
        return m_width;
    }
    int height() const {
        return m_height;
    }

    void clear() {
        std::fill(r_.begin(), r_.end(), 0.f);
        std::fill(g_.begin(), g_.end(), 0.f);
        std::fill(b_.begin(), b_.end(), 0.f);
    }

    int get_width() const {
        return m_width;
    }

    int get_height() const {
        return m_height;
    }

private:
    int m_width;
    int m_height;

    // 32 字节对齐，便于 AVX2 加载
    std::vector<float, AlignedAllocator<float, 32>> r_, g_, b_; // SoA
};

#endif