#ifndef RENDER_BUFFER_H
#define RENDER_BUFFER_H

#include "vec3.h"
#include "aligned_allocator.h"
#include <algorithm>
#include <mutex>
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
        std::lock_guard<std::mutex> lock(mutex_);
        std::fill(r_.begin(), r_.end(), 0.f);
        std::fill(g_.begin(), g_.end(), 0.f);
        std::fill(b_.begin(), b_.end(), 0.f);
    }

    // 将一块 tile 的 RGB（float）提交到 buffer 中，避免与 UI 读取并发造成数据竞争。
    // tile 数据布局：row-major，索引 = (y - y_start) * tile_w + (x - x_start)
    void commit_tile(int x_start, int y_start, int x_end, int y_end,
                     const std::vector<float>& tile_r,
                     const std::vector<float>& tile_g,
                     const std::vector<float>& tile_b) {
        const int tile_w = x_end - x_start;
        const int tile_h = y_end - y_start;
        if (tile_w <= 0 || tile_h <= 0) {
            return;
        }
        const size_t tile_n = static_cast<size_t>(tile_w) * tile_h;
        if (tile_r.size() < tile_n || tile_g.size() < tile_n || tile_b.size() < tile_n) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        for (int y = y_start; y < y_end; ++y) {
            const int ly = y - y_start;
            for (int x = x_start; x < x_end; ++x) {
                const int lx = x - x_start;
                const size_t src = static_cast<size_t>(ly) * tile_w + lx;
                const size_t dst = static_cast<size_t>(y) * m_width + x;
                r_[dst] = tile_r[src];
                g_[dst] = tile_g[src];
                b_[dst] = tile_b[src];
            }
        }
    }

    // 从当前 buffer 拷贝一份快照到 dst（dst 尺寸需一致）。
    // UI 使用 dst 做后续处理，避免长时间持锁阻塞渲染线程。
    void copy_to(RenderBuffer& dst) const {
        if (dst.m_width != m_width || dst.m_height != m_height) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        std::copy(r_.begin(), r_.end(), dst.r_.begin());
        std::copy(g_.begin(), g_.end(), dst.g_.begin());
        std::copy(b_.begin(), b_.end(), dst.b_.begin());
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

    // 保护 r_/g_/b_：渲染线程以 tile 粒度提交，UI 线程以快照方式读取
    mutable std::mutex mutex_;

    // 32 字节对齐，便于 AVX2 加载
    std::vector<float, AlignedAllocator<float, 32>> r_, g_, b_; // SoA
};

#endif