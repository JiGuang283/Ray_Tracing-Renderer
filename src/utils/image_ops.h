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
    // strength: [0,1]，0=关闭效果，1=全强度
    void apply_post_processing(std::vector<unsigned char>& image_data, int width, int height, int type, float strength = 1.0f);

    // 保存图像到磁盘
    void save_image_to_disk(const std::vector<unsigned char>& image_data, int width, int height, int format_idx, int scene_id, int integrator_id);

    void build_gamma_lut(float gamma, int lut_size = 4096);

    float gamma_lookup(float x);

    const float* gamma_lut_data();
    int gamma_lut_size();

    void build_gaussian_weights(std::vector<float>& spatial_weights, std::vector<float>& range_weights, int radius, float sigma_s, float sigma_r);
    void apply_bilateral_filter(std::vector<unsigned char>& image_data, int width, int height, float strength = 1.0f);
}



#endif //IMAGE_OPS_H
