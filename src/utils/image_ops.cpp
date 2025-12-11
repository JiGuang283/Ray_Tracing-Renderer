#include "image_ops.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <fstream>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace ImageOps {

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

        if (type == 0 || type == 1) { // Blur or Sharpen
            std::vector<unsigned char> temp_data = image_data;

            #pragma omp parallel for
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    int r = 0, g = 0, b = 0;
                    int weight_sum = 0;

                    int kernel[3][3];
                    if (type == 0) {
                        for(int i=0;i<3;i++) for(int j=0;j<3;j++) kernel[i][j] = 1;
                        weight_sum = 9;
                    } else {
                        int s_k[3][3] = {{0, -1, 0}, {-1, 5, -1}, {0, -1, 0}};
                        for(int i=0;i<3;i++) for(int j=0;j<3;j++) kernel[i][j] = s_k[i][j];
                        weight_sum = 1;
                    }

                    for (int ky = -1; ky <= 1; ++ky) {
                        for (int kx = -1; kx <= 1; ++kx) {
                            int idx = get_idx(x + kx, y + ky);
                            int w = kernel[ky + 1][kx + 1];
                            r += temp_data[idx + 0] * w;
                            g += temp_data[idx + 1] * w;
                            b += temp_data[idx + 2] * w;
                        }
                    }

                    int current_idx = (y * width + x) * 4;
                    image_data[current_idx + 0] = static_cast<unsigned char>(std::max(0, std::min(255, r / weight_sum)));
                    image_data[current_idx + 1] = static_cast<unsigned char>(std::max(0, std::min(255, g / weight_sum)));
                    image_data[current_idx + 2] = static_cast<unsigned char>(std::max(0, std::min(255, b / weight_sum)));
                }
            }
        } else if (type == 2) { // Grayscale
            #pragma omp parallel for
            for (int i = 0; i < width * height; ++i) {
                int idx = i * 4;
                unsigned char gray = static_cast<unsigned char>(
                    0.299 * image_data[idx] + 0.587 * image_data[idx + 1] + 0.114 * image_data[idx + 2]);
                image_data[idx] = gray;
                image_data[idx + 1] = gray;
                image_data[idx + 2] = gray;
            }
        } else if (type == 3) { // Invert
             #pragma omp parallel for
            for (int i = 0; i < width * height; ++i) {
                int idx = i * 4;
                image_data[idx] = 255 - image_data[idx];
                image_data[idx + 1] = 255 - image_data[idx + 1];
                image_data[idx + 2] = 255 - image_data[idx + 2];
            }
        } else if (type == 4) { // Median Filter
            std::vector<unsigned char> temp_data = image_data;
            #pragma omp parallel for
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    std::vector<unsigned char> rs, gs, bs;
                    rs.reserve(9); gs.reserve(9); bs.reserve(9);

                    for (int ky = -1; ky <= 1; ++ky) {
                        for (int kx = -1; kx <= 1; ++kx) {
                            int idx = get_idx(x + kx, y + ky);
                            rs.push_back(temp_data[idx + 0]);
                            gs.push_back(temp_data[idx + 1]);
                            bs.push_back(temp_data[idx + 2]);
                        }
                    }
                    std::sort(rs.begin(), rs.end());
                    std::sort(gs.begin(), gs.end());
                    std::sort(bs.begin(), bs.end());

                    int current_idx = (y * width + x) * 4;
                    image_data[current_idx + 0] = rs[4];
                    image_data[current_idx + 1] = gs[4];
                    image_data[current_idx + 2] = bs[4];
                }
            }
        }
    }
}