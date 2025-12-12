#include "image_ops.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <fstream>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace ImageOps {

    std::vector<float> gamma_lut_;
    float gamma_lut_inv_size_ = 0.f;

    vec3 ACESFilm(vec3 x) {
        float a = 2.51f;
        float b = 0.03f;
        float c = 2.43f;
        float d = 0.59f;
        float e = 0.14f;
        x = vec3(std::max(0.0, x.x()), std::max(0.0, x.y()), std::max(0.0, x.z()));
        return (x*(x*a+b))/(x*(x*c+d)+e);
    }

    void save_image_to_disk(const std::vector<unsigned char>& image_data, int width, int height, int format_idx) {
        if (image_data.empty() || width == 0 || height == 0) {
            std::cerr << "No image data to save." << std::endl;
            return;
        }

        std::string filename = "output";

        switch (format_idx) {
            case 0: { // PPM
                filename += ".ppm";
                std::ofstream ofs(filename);
                ofs << "P3\n" << width << " " << height << "\n255\n";
                for (int i = 0; i < width * height; ++i) {
                    ofs << static_cast<int>(image_data[i * 4 + 0]) << " "
                        << static_cast<int>(image_data[i * 4 + 1]) << " "
                        << static_cast<int>(image_data[i * 4 + 2]) << "\n";
                }
                ofs.close();
                break;
            }
            case 1: // PNG
                filename += ".png";
                stbi_write_png(filename.c_str(), width, height, 4, image_data.data(), width * 4);
                break;
            case 2: // BMP
                filename += ".bmp";
                stbi_write_bmp(filename.c_str(), width, height, 4, image_data.data());
                break;
            case 3: // JPG
                filename += ".jpg";
                stbi_write_jpg(filename.c_str(), width, height, 4, image_data.data(), 90);
                break;
            default:
                std::cerr << "Unknown format" << std::endl;
                return;
        }
        std::cout << "Image saved to " << filename << std::endl;
    }

    void apply_post_processing(std::vector<unsigned char>& image_data, int width, int height, int type) {
        if (image_data.empty()) return;

        auto get_idx = [&](int x, int y) {
            x = std::max(0, std::min(x, width - 1));
            y = std::max(0, std::min(y, height - 1));
            return (y * width + x) * 4;
        };

        if (type == 0 || type == 1) { // Blur 或 Sharpen
            std::vector<unsigned char> temp_data = image_data;
            const int blur_kernel[3][3]   = { {1,1,1},{1,1,1},{1,1,1} };
            const int sharp_kernel[3][3]  = { {0,-1,0},{-1,5,-1},{0,-1,0} };
            const int (*kernel)[3] = (type == 0) ? blur_kernel : sharp_kernel;
            const int weight_sum = (type == 0) ? 9 : 1;

            #pragma omp parallel for collapse(2)
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    int r = 0, g = 0, b = 0;
                    for (int ky = -1; ky <= 1; ++ky) {
                        for (int kx = -1; kx <= 1; ++kx) {
                            int idx = get_idx(x + kx, y + ky);
                            int w = kernel[ky + 1][kx + 1];
                            r += temp_data[idx + 0] * w;
                            g += temp_data[idx + 1] * w;
                            b += temp_data[idx + 2] * w;
                        }
                    }
                    int cur = (y * width + x) * 4;
                    image_data[cur + 0] = static_cast<unsigned char>(clamp_compat(r / weight_sum, 0, 255));
                    image_data[cur + 1] = static_cast<unsigned char>(clamp_compat(g / weight_sum, 0, 255));
                    image_data[cur + 2] = static_cast<unsigned char>(clamp_compat(b / weight_sum, 0, 255));
                }
            }
        } else if (type == 2) { // Grayscale
            #pragma omp parallel for
            for (int i = 0; i < width * height; ++i) {
                int idx = i * 4;
                unsigned char gray = static_cast<unsigned char>(
                    0.299f * image_data[idx] + 0.587f * image_data[idx + 1] + 0.114f * image_data[idx + 2]);
                image_data[idx + 0] = gray;
                image_data[idx + 1] = gray;
                image_data[idx + 2] = gray;
            }
        } else if (type == 3) { // Invert
            #pragma omp parallel for
            for (int i = 0; i < width * height; ++i) {
                int idx = i * 4;
                image_data[idx + 0] = 255 - image_data[idx + 0];
                image_data[idx + 1] = 255 - image_data[idx + 1];
                image_data[idx + 2] = 255 - image_data[idx + 2];
            }
        } else if (type == 4) { // Median
            std::vector<unsigned char> temp_data = image_data;
            #pragma omp parallel for collapse(2)
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    unsigned char rs[9], gs[9], bs[9];
                    int t = 0;
                    for (int ky = -1; ky <= 1; ++ky) {
                        for (int kx = -1; kx <= 1; ++kx) {
                            int idx = get_idx(x + kx, y + ky);
                            rs[t] = temp_data[idx + 0];
                            gs[t] = temp_data[idx + 1];
                            bs[t] = temp_data[idx + 2];
                            ++t;
                        }
                    }
                    std::sort(rs, rs + 9);
                    std::sort(gs, gs + 9);
                    std::sort(bs, bs + 9);

                    int cur = (y * width + x) * 4;
                    image_data[cur + 0] = rs[4];
                    image_data[cur + 1] = gs[4];
                    image_data[cur + 2] = bs[4];
                }
            }
        }
    }

    void build_gamma_lut(float gamma, int lut_size) {
        const float safe_gamma = std::max(gamma, 1e-6f);
        gamma_lut_.resize(lut_size + 1);
        gamma_lut_inv_size_ = 1.0f / lut_size;
        float inv_gamma = 1.0f / safe_gamma;
        for (int i = 0; i <= lut_size; ++i) {
            float x = static_cast<float>(i) * gamma_lut_inv_size_;
            gamma_lut_[i] = std::pow(x, inv_gamma);
        }
    }

    float gamma_lookup(float x) {
        if (gamma_lut_.empty()) return clamp_compat(x, 0.0f, 1.0f);
        x = clamp_compat(x, 0.0f, 1.0f);
        const int n = static_cast<int>(gamma_lut_.size());
        float fidx = x * (n - 1);
        int idx = static_cast<int>(fidx);
        if (idx >= n - 1) return gamma_lut_.back();
        float t = fidx - idx;
        return gamma_lut_[idx] * (1.0f - t) + gamma_lut_[idx + 1] * t;
    }
}