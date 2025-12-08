#ifndef RENDER_BUFFER_H
#define RENDER_BUFFER_H

#include "vec3.h"
#include <mutex>
#include <vector>

class RenderBuffer {
  public:
    RenderBuffer(int width, int height) : m_width(width), m_height(height) {
        m_pixels.resize(height, std::vector<color>(width));
    }

    void set_pixel(int x, int y, const color &pixel_color) {
        if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
            m_pixels[y][x] = pixel_color;
        }
    }

    const std::vector<std::vector<color>> &get_data() const {
        return m_pixels;
    }

    int width() const {
        return m_width;
    }
    int height() const {
        return m_height;
    }

  private:
    int m_width;
    int m_height;
    std::vector<std::vector<color>> m_pixels;
};

#endif