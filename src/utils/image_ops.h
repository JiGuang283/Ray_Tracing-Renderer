#ifndef IMAGE_OPS_H
#define IMAGE_OPS_H

#pragma once

#include <vector>
#include "vec3.h" // 假设 vec3 定义在此处或其包含的头文件中

namespace ImageOps {
    // 色调映射算法
    vec3 ACESFilm(vec3 x);
    // 应用后处理滤镜
    void apply_post_processing(std::vector<unsigned char>& image_data, int width, int height, int type);
    // 保存图像到磁盘
    void save_image_to_disk(const std::vector<unsigned char>& image_data, int width, int height, int format_idx);
}



#endif //IMAGE_OPS_H
