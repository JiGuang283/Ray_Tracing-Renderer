#ifndef IMAGE_OPS_H
#define IMAGE_OPS_H

#pragma once

#include <vector>
#include "vec3.h"

template <typename T>
T clamp_compat(const T& v, const T& lo, const T& hi) {
    if (v < lo) {
        return lo;
    }else if (v > hi) {
        return hi;
    }else {
        return v;
    }

}

namespace ImageOps {
    // 色调映射算法
    vec3 ACESFilm(vec3 x);
    // 应用后处理滤镜
    void apply_post_processing(std::vector<unsigned char>& image_data, int width, int height, int type);
    // 保存图像到磁盘
    void save_image_to_disk(const std::vector<unsigned char>& image_data, int width, int height, int format_idx);

    void build_gamma_lut(float gamma, int lut_size = 4096);

    float gamma_lookup(float x);
}



#endif //IMAGE_OPS_H
