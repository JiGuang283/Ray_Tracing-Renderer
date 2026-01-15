#include "image_ops.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "simd.h"
#include "stb_image_write.h"

// 处理跨平台的 mkdir
#ifdef _WIN32
    #include <direct.h>
    #define MKDIR(dir) _mkdir(dir)
#else
    #include <sys/stat.h>
    #define MKDIR(dir) mkdir(dir, 0755)
#endif

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

    void save_image_to_disk(const std::vector<unsigned char>& image_data,
                            int width, int height,
                            int format_idx,
                            int scene_id,
                            int integrator_id) {

        if (image_data.empty() || width == 0 || height == 0) {
            std::cerr << "No image data to save." << std::endl;
            return;
        }

        // 1. 创建 output 文件夹（如果不存在）
        // 如果文件夹已存在，MKDIR 通常会返回 -1，这里我们忽略错误，假设是文件夹已存在
        MKDIR("output");

        // 2. 生成带编号和时间戳的文件名基础部分 (不含扩展名)
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);

        std::stringstream ss;
        ss << "output/scene" << std::setfill('0') << std::setw(2) << scene_id
           << "_integrator" << integrator_id << "_" << timestamp;

        std::string base_filename = ss.str();
        std::string full_filename;

        // 3. 根据格式添加扩展名并保存
        switch (format_idx) {
            case 0: { // PPM
                full_filename = base_filename + ".ppm";
                std::ofstream ofs(full_filename);
                if (!ofs) {
                    std::cerr << "Failed to open file for writing: " << full_filename << std::endl;
                    return;
                }
                ofs << "P3\n" << width << " " << height << "\n255\n";

                // 缓存 buffer 输出以减少 I/O 次数
                std::string buffer;
                buffer.reserve(width * height * 12);
                char temp[32]; // 稍微加大一点 buffer 防止溢出
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
                full_filename = base_filename + ".png";
                stbi_write_png(full_filename.c_str(), width, height, 4, image_data.data(), width * 4);
                break;
            case 2: // BMP
                full_filename = base_filename + ".bmp";
                stbi_write_bmp(full_filename.c_str(), width, height, 4, image_data.data());
                break;
            case 3: // JPG
                full_filename = base_filename + ".jpg";
                stbi_write_jpg(full_filename.c_str(), width, height, 4, image_data.data(), 90);
                break;
            default:
                std::cerr << "Unknown format index: " << format_idx << std::endl;
                return;
        }

        std::cout << "Image saved successfully to " << full_filename << std::endl;
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
        } else if (type == 5) {
            // 双边滤波
            apply_bilateral_filter(image_data, width, height);
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
        if (n < 2) {
            return x;
        }

        float fidx = x * (n - 1);
        int idx = static_cast<int>(fidx);

        // x==1 时 idx==n-1，避免 idx+1 越界
        if (idx >= n - 1) {
            return gamma_lut_[n - 1];
        }

        float t = fidx - idx;
        return gamma_lut_[idx] * (1.0f - t) + gamma_lut_[idx + 1] * t;
    }

    const float* gamma_lut_data() {
        return gamma_lut_.empty() ? nullptr : gamma_lut_.data();
    }

    int gamma_lut_size() {
        return static_cast<int>(gamma_lut_.size());
    }

    // 预计算高斯权重表，提升性能
    void build_gaussian_weights(std::vector<float>& spatial_weights, std::vector<float>& range_weights,
                                int radius, float sigma_s, float sigma_r) {
        // 1. 空间权重 (Spatial)
        int kernel_size = 2 * radius + 1;
        spatial_weights.resize(kernel_size * kernel_size);
        float coeff_s = -1.0f / (2.0f * sigma_s * sigma_s);

        for (int y = -radius; y <= radius; ++y) {
            for (int x = -radius; x <= radius; ++x) {
                float r2 = static_cast<float>(x*x + y*y);
                spatial_weights[(y + radius) * kernel_size + (x + radius)] = std::exp(r2 * coeff_s);
            }
        }

        // 2. 颜色差异权重 (Range) - 针对 0-255 的差值
        range_weights.resize(256);
        float coeff_r = -1.0f / (2.0f * sigma_r * sigma_r);
        for (int i = 0; i < 256; ++i) {
            range_weights[i] = std::exp(static_cast<float>(i*i) * coeff_r);
        }
    }

    void apply_bilateral_filter(std::vector<unsigned char>& image_data, int width, int height) {
        // === 参数调整区 ===
        const int radius = 2;         // 核心半径 (2 = 5x5 窗口, 3 = 7x7 窗口)
        const float sigma_s = 2.0f;   // 空间模糊强度 (越大越糊)
        const float sigma_r = 30.0f;  // 颜色容差 (越大越容易把边缘抹掉，越小保留边缘越好但降噪弱)

        std::vector<unsigned char> source = image_data; // 拷贝副本用于读取
        const int stride = width * 4;
        const int kernel_width = 2 * radius + 1;

        // 预计算权重表
        std::vector<float> w_spatial;
        std::vector<float> w_range;
        build_gaussian_weights(w_spatial, w_range, radius, sigma_s, sigma_r);

        #pragma omp parallel for schedule(dynamic)
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float sum_r = 0, sum_g = 0, sum_b = 0;
                float total_weight = 0.0f;

                // 中心像素颜色
                int center_idx = (y * width + x) * 4;
                unsigned char c_r = source[center_idx + 0];
                unsigned char c_g = source[center_idx + 1];
                unsigned char c_b = source[center_idx + 2];

                // 遍历邻域
                for (int ky = -radius; ky <= radius; ++ky) {
                    int ny = y + ky;
                    // 边界检查 (Clamp edge)
                    if (ny < 0) ny = 0;
                    else if (ny >= height) ny = height - 1;

                    for (int kx = -radius; kx <= radius; ++kx) {
                        int nx = x + kx;
                        if (nx < 0) nx = 0;
                        else if (nx >= width) nx = width - 1;

                        // 获取邻居像素
                        int neighbor_idx = (ny * width + nx) * 4;
                        unsigned char n_r = source[neighbor_idx + 0];
                        unsigned char n_g = source[neighbor_idx + 1];
                        unsigned char n_b = source[neighbor_idx + 2];

                        // 1. 空间权重 (查表)
                        float ws = w_spatial[(ky + radius) * kernel_width + (kx + radius)];

                        // 2. 颜色权重 (计算曼哈顿距离或欧氏距离，这里简化用最大通道差查表)
                        // 使用亮度差或者 RGB 距离都可以，这里为了简单使用 L1 距离的平均
                        int diff = (std::abs(c_r - n_r) + std::abs(c_g - n_g) + std::abs(c_b - n_b)) / 3;
                        float wr = w_range[diff];

                        float weight = ws * wr;

                        sum_r += n_r * weight;
                        sum_g += n_g * weight;
                        sum_b += n_b * weight;
                        total_weight += weight;
                    }
                }

                // 归一化并写入
                int dst_idx = (y * width + x) * 4;
                image_data[dst_idx + 0] = static_cast<unsigned char>(sum_r / total_weight);
                image_data[dst_idx + 1] = static_cast<unsigned char>(sum_g / total_weight);
                image_data[dst_idx + 2] = static_cast<unsigned char>(sum_b / total_weight);
                image_data[dst_idx + 3] = source[center_idx + 3]; // Alpha
            }
        }
    }
}