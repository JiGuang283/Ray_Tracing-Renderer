#include "image_ops.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <fstream>

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "simd.h"
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
                // 缓存 buffer 输出以减少 I/O 次数
                std::string buffer;
                buffer.reserve(width * height * 12);
                char temp[16];
                for (int i = 0; i < width * height; ++i) {
                    int len = sprintf(temp, "%d %d %d\n",
                                            image_data[i * 4 + 0],
                                            image_data[i * 4 + 1],
                                            image_data[i * 4 + 2]);
                    buffer.append(temp, len);
                    if (buffer.size() > 65536) { // 分块写入
                        ofs << buffer;
                        buffer.clear();
                    }
                }
                ofs << buffer;
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
        if (image_data.empty()) {
            return;
        }

        const int stride = width * 4;
        const int total_pixels = width * height;

        //仅用于处理边界情况 (Slow Path)
        auto get_idx_safe = [&](int x, int y) {
            x = std::max(0, std::min(x, width - 1));
            y = std::max(0, std::min(y, height - 1));
            return (y * width + x) * 4;
        };

        if (type == 0 || type == 1) { // Blur 或 Sharpen
            // 只有卷积操作需要拷贝副本
            std::vector<unsigned char> source = image_data;
            const unsigned char* src_ptr = source.data();
            unsigned char* dst_ptr = image_data.data();

            const int blur_kernel[9] = {1,1,1, 1,1,1, 1,1,1};
            const int sharp_kernel[9] = {0,-1,0, -1,5,-1, 0,-1,0};
            const int* kernel = (type == 0) ? blur_kernel : sharp_kernel;

            // 预计算除法的倒数，转为乘法
            const float weight_inv = (type == 0) ? (1.0f / 9.0f) : 1.0f;

            // 卷积偏移量表 (3x3)
            const int offsets[9] = {
                -stride - 4, -stride, -stride + 4,
                -4,           0,       4,
                stride - 4,  stride,  stride + 4
            };

            #pragma omp parallel for schedule(static)
            for (int y = 0; y < height; ++y) {
                // 指向当前行起始
                unsigned char* row_dst = dst_ptr + y * stride;
                const unsigned char* row_src = src_ptr + y * stride;

                for (int x = 0; x < width; ++x) {
                    int r = 0, g = 0, b = 0;

                    // 快速路径：图像中心区域（不涉及边界检查）
                    if (x > 0 && x < width - 1 && y > 0 && y < height - 1) {
                        const unsigned char* p = row_src + x * 4;
                        // 手动展开循环或利用编译器向量化
                        for (int k = 0; k < 9; ++k) {
                            const int w = kernel[k];
                            // 只有当权重不为0时才计算 (Sharpen中有0)
                            if (w != 0) {
                                const unsigned char* neighbor = p + offsets[k];
                                r += neighbor[0] * w;
                                g += neighbor[1] * w;
                                b += neighbor[2] * w;
                            }
                        }
                    }else {
                        // 慢速路径：图像边缘
                        int k = 0;
                        for (int ky = -1; ky <= 1; ++ky) {
                            for (int kx = -1; kx <= 1; ++kx) {
                                int idx = get_idx_safe(x + kx, y + ky);
                                int w = kernel[k++];
                                if (w != 0) {
                                    r += source[idx + 0] * w;
                                    g += source[idx + 1] * w;
                                    b += source[idx + 2] * w;
                                }
                            }
                        }
                    }
                    // 写入结果
                    int dst_idx = x * 4;
                    // 使用浮点乘法代替整数除法
                    row_dst[dst_idx + 0] = static_cast<unsigned char>(clamp_compat(r * weight_inv, 0.0f, 255.0f));
                    row_dst[dst_idx + 1] = static_cast<unsigned char>(clamp_compat(g * weight_inv, 0.0f, 255.0f));
                    row_dst[dst_idx + 2] = static_cast<unsigned char>(clamp_compat(b * weight_inv, 0.0f, 255.0f));
                    // Alpha 通道保持不变 (假设是 source 的 alpha)
                    row_dst[dst_idx + 3] = row_src[x * 4 + 3];
                }
            }
        } else if (type == 2) { // Grayscale
            #pragma omp parallel for
            for (int i = 0; i < total_pixels; ++i) {
                int idx = i * 4;
                float gray_f = 0.299f * image_data[idx] + 0.587f * image_data[idx + 1] + 0.114f * image_data[idx + 2];
                unsigned char gray = static_cast<unsigned char>(gray_f);
                image_data[idx + 0] = gray;
                image_data[idx + 1] = gray;
                image_data[idx + 2] = gray;
            }
        } else if (type == 3) { // Invert
            #pragma omp parallel for schedule(static)
            for (int i = 0; i < total_pixels; ++i) {
                int idx = i * 4;
                image_data[idx + 0] = 255 - image_data[idx + 0];
                image_data[idx + 1] = 255 - image_data[idx + 1];
                image_data[idx + 2] = 255 - image_data[idx + 2];
            }
        }else if (type == 4) { // Median - SIMD 优化版本
            std::vector<unsigned char> source = image_data;
            const unsigned char* src_ptr = source.data();
            unsigned char* dst_ptr = image_data.data();

            const int offsets[9] = {
                -stride - 4, -stride, -stride + 4,
                -4,           0,       4,
                stride - 4,  stride,  stride + 4
            };

            #pragma omp parallel for schedule(static)
            for (int y = 0; y < height; ++y) {
                unsigned char* row_dst = dst_ptr + y * stride;
                const unsigned char* row_src = src_ptr + y * stride;

                int x = 0;

                // SIMD 处理：只在安全区域使用（非边界行和列）
                const bool is_safe_row = (y > 0 && y < height - 1);

                if (is_safe_row) {
                    #if defined(__AVX2__)
                        // AVX2：处理 8 像素批次
                        const int simd_width = 8;
                        const int safe_end = width - simd_width;

                        for (x = 1; x < safe_end; x += simd_width) {
                            median_filter_avx2(src_ptr, dst_ptr, x, y, width, stride, offsets);
                        }
                    #elif defined(__SSE2__)
                        // SSE2：处理 4 像素批次
                        const int simd_width = 4;
                        const int safe_end = width - simd_width;

                        for (x = 1; x < safe_end; x += simd_width) {
                            median_filter_sse2(src_ptr, dst_ptr, x, y, width, stride, offsets);
                        }
                    #endif
                }

                // 标量路径：处理剩余像素（包括边界）
                for (; x < width; ++x) {
                    unsigned char rs[9], gs[9], bs[9];

                    // 判断是否为内部像素
                    const bool is_inner = (x > 0 && x < width - 1 && is_safe_row);

                    if (is_inner) {
                        // 快速路径：直接访问邻域
                        const unsigned char* p = row_src + x * 4;
                        for(int k = 0; k < 9; ++k) {
                            const unsigned char* neighbor = p + offsets[k];
                            rs[k] = neighbor[0];
                            gs[k] = neighbor[1];
                            bs[k] = neighbor[2];
                        }
                    } else {
                        // 慢速路径：边界安全访问
                        int t = 0;
                        for (int ky = -1; ky <= 1; ++ky) {
                            for (int kx = -1; kx <= 1; ++kx) {
                                int idx = get_idx_safe(x + kx, y + ky);
                                rs[t] = source[idx + 0];
                                gs[t] = source[idx + 1];
                                bs[t] = source[idx + 2];
                                ++t;
                            }
                        }
                    }

                    // 计算中值并写入
                    int dst_idx = x * 4;
                    row_dst[dst_idx + 0] = median9_scalar(rs);
                    row_dst[dst_idx + 1] = median9_scalar(gs);
                    row_dst[dst_idx + 2] = median9_scalar(bs);
                    row_dst[dst_idx + 3] = row_src[x * 4 + 3];
                }
            }
        }
    }

    void build_gamma_lut(float gamma, int lut_size) {
        const float safe_gamma = std::max(gamma, 1e-6f);
        if (gamma_lut_.size() != (size_t)(lut_size + 1)) {
            gamma_lut_.resize(lut_size + 1);
        }
        gamma_lut_inv_size_ = 1.0f / lut_size;
        float inv_gamma = 1.0f / safe_gamma;

        #pragma omp parallel for schedule(static)
        for (int i = 0; i <= lut_size; ++i) {
            float x = static_cast<float>(i) * gamma_lut_inv_size_;
            gamma_lut_[i] = std::pow(x, inv_gamma);
        }
    }

    float gamma_lookup(float x) {

        if (gamma_lut_.empty()) {
            return clamp_compat(x, 0.0f, 1.0f);
        }
        x = (x < 0.0f) ? 0.0f : ((x > 1.0f) ? 1.0f : x);
        const int n = static_cast<int>(gamma_lut_.size());
        float fidx = x * (n - 1);
        int idx = static_cast<int>(fidx);
        // 只有极少数浮点误差可能导致越界，保留一个简单的 fast check
        float t = fidx - idx;
        // 直接访问
        return gamma_lut_[idx] * (1.0f - t) + gamma_lut_[idx + 1] * t;
    }

    const float* gamma_lut_data() {
        return gamma_lut_.empty() ? nullptr : gamma_lut_.data();
    }

    int gamma_lut_size() {
        return static_cast<int>(gamma_lut_.size());
    }
}