#ifndef RENDER_BUFFER_H
#define RENDER_BUFFER_H

#include "vec3.h"
#include <string>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

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

    // 保存为PNG图片
    bool save_to_png(const std::string &filename) const {
        std::vector<unsigned char> image_data(m_width * m_height * 3);

        for (int j = 0; j < m_height; ++j) {
            for (int i = 0; i < m_width; ++i) {
                // 翻转Y坐标，使图片正确显示
                int flipped_j = m_height - 1 - j;
                int index = (j * m_width + i) * 3;
                const auto &pixel = m_pixels[flipped_j][i];
                image_data[index + 0] =
                    static_cast<unsigned char>(pixel[0] * 255);
                image_data[index + 1] =
                    static_cast<unsigned char>(pixel[1] * 255);
                image_data[index + 2] =
                    static_cast<unsigned char>(pixel[2] * 255);
            }
        }

        return stbi_write_png(filename.c_str(), m_width, m_height, 3,
                              image_data.data(), m_width * 3);
    }

    // 保存为JPG图片
    bool save_to_jpg(const std::string &filename, int quality = 90) const {
        std::vector<unsigned char> image_data(m_width * m_height * 3);

        for (int j = 0; j < m_height; ++j) {
            for (int i = 0; i < m_width; ++i) {
                // 翻转Y坐标，使图片正确显示
                int flipped_j = m_height - 1 - j;
                int index = (j * m_width + i) * 3;
                const auto &pixel = m_pixels[flipped_j][i];
                image_data[index + 0] =
                    static_cast<unsigned char>(pixel[0] * 255);
                image_data[index + 1] =
                    static_cast<unsigned char>(pixel[1] * 255);
                image_data[index + 2] =
                    static_cast<unsigned char>(pixel[2] * 255);
            }
        }

        return stbi_write_jpg(filename.c_str(), m_width, m_height, 3,
                              image_data.data(), quality);
    }

  private:
    int m_width;
    int m_height;
    std::vector<std::vector<color>> m_pixels;
};

#endif