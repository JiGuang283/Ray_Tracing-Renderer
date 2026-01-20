#ifndef IMAGEPROCESSOR_H
#define IMAGEPROCESSOR_H

#include <vector>
#include <memory>
#include "vec3.h"
#include "render_buffer.h"

struct ImageProcessConfig {
    int tone_mapping_type = 0;  // 0: None, 1: Reinhard, 2: ACES
    float gamma = 2.0f;
    bool enable_post_process = false;
    int post_process_type = 0;  // 0: Blur, 1: Sharpen, etc.

    // 滤镜强度 [0,1]，0=关闭效果，1=全强度
    float post_process_strength = 1.0f;
};

class ImageProcessor {
public:
    ImageProcessor();

    void update_config(const ImageProcessConfig& config);
    const ImageProcessConfig& get_config() const { return config_; }

    // 从 RenderBuffer 生成最终的 RGBA8 图像
    void process(
        const RenderBuffer& buffer,
        std::vector<unsigned char>& output,
        int width,
        int height
    );

private:
    vec3 apply_tone_mapping(const vec3& color) const;
    void apply_gamma_correction(std::vector<unsigned char>& data, int w, int h);
    void apply_post_processing(std::vector<unsigned char>& data, int w, int h)const;

    void process_rows_simd(
        const RenderBuffer& buffer,
        std::vector<unsigned char>& output,
        int width, int height,
        int y_start, int y_end
    );

    ImageProcessConfig config_;
};

#endif //IMAGEPROCESSOR_H
