#include "ImageProcessor.h"
#include "image_ops.h"
#include "simd.h"
#include <future>
#include <algorithm>
#include <thread>

#if defined(__AVX2__)
    #include <immintrin.h>
#endif

ImageProcessor::ImageProcessor() {
    ImageOps::build_gamma_lut(config_.gamma, 4096);
}

void ImageProcessor::update_config(const ImageProcessConfig& config) {
    bool gamma_changed = (config.gamma != config_.gamma);
    config_ = config;

    if (gamma_changed) {
        ImageOps::build_gamma_lut(config_.gamma, 4096);
    }
}

void ImageProcessor::process(
    const RenderBuffer& buffer,
    std::vector<unsigned char>& output,
    int width,
    int height
) {
    if (output.size() != static_cast<size_t>(width * height * 4)) {
        output.resize(width * height * 4);
    }

    // 多线程处理
    int nthreads = std::max(1u, std::thread::hardware_concurrency() - 1);
    int rows_per_task = (height + nthreads - 1) / nthreads;

    std::vector<std::future<void>> tasks;
    tasks.reserve(nthreads);

    for (int t = 0; t < nthreads; ++t) {
        int y0 = t * rows_per_task;
        int y1 = std::min(height, y0 + rows_per_task);
        if (y0 >= y1) break;

        tasks.emplace_back(std::async(std::launch::async,
            [this, &buffer, &output, width, height, y0, y1]() {
                process_rows_simd(buffer, output, width, height, y0, y1);
            }
        ));
    }

    for (auto& f : tasks) {
        f.get();
    }

    // 应用后处理
    if (config_.enable_post_process) {
        apply_post_processing(output, width, height);
    }
}

void ImageProcessor::process_rows_simd(
    const RenderBuffer& buffer,
    std::vector<unsigned char>& output,
    int width, int height,
    int y_start, int y_end
) {
    const auto& rbuf = buffer.r();
    const auto& gbuf = buffer.g();
    const auto& bbuf = buffer.b();

    for (int j = y_start; j < y_end; ++j) {
        int i = 0;

        // SIMD 处理 8 像素一批
        for (; i + 7 < width; i += 8) {
            size_t idx0 = (height - 1 - j) * width + i; // 翻转 Y 轴

            #if defined(__AVX2__)
                // 1. 加载 (使用 loadu 防止崩溃)
                __m256 vr = _mm256_loadu_ps(&rbuf[idx0]);
                __m256 vg = _mm256_loadu_ps(&gbuf[idx0]);
                __m256 vb = _mm256_loadu_ps(&bbuf[idx0]);

                __m256 v0 = _mm256_set1_ps(0.0f);
                __m256 v1 = _mm256_set1_ps(1.0f);

                if (config_.tone_mapping_type == 1) { // Reinhard
                    // x / (x + 1)
                    vr = tone_map_reinhard_avx2(vr);
                    vg = tone_map_reinhard_avx2(vg);
                    vb = tone_map_reinhard_avx2(vb);
                } else if (config_.tone_mapping_type == 2) { // ACES
                    vr = tone_map_aces_avx2(vr);
                    vg = tone_map_aces_avx2(vg);
                    vb = tone_map_aces_avx2(vb);
                }

                // Clamp [0, 1]
                vr = _mm256_min_ps(v1, _mm256_max_ps(v0, vr));
                vg = _mm256_min_ps(v1, _mm256_max_ps(v0, vg));
                vb = _mm256_min_ps(v1, _mm256_max_ps(v0, vb));

                // Gamma LUT lookup
                const int lut_size = ImageOps::gamma_lut_size();
                const __m256 scale = _mm256_set1_ps(static_cast<float>(lut_size - 1));
                __m256 fr = _mm256_mul_ps(vr, scale);
                __m256 fg = _mm256_mul_ps(vg, scale);
                __m256 fb = _mm256_mul_ps(vb, scale);

                __m256i ir = _mm256_cvtps_epi32(fr);
                __m256i ig = _mm256_cvtps_epi32(fg);
                __m256i ib = _mm256_cvtps_epi32(fb);

                const float* lutp = ImageOps::gamma_lut_data();
                vr = _mm256_i32gather_ps(lutp, ir, 4);
                vg = _mm256_i32gather_ps(lutp, ig, 4);
                vb = _mm256_i32gather_ps(lutp, ib, 4);

                // 5. 直接存储 (无内存往返)
                uint8_t* dst = &output[(j * width + i) * 4];
                store_rgba8_avx2(vr, vg, vb, dst);
            #elif defined(__SSE2__)
                // --- SSE2 路径
                // 每次处理 8 个像素，SSE2 寄存器一次只能存 4 个 float
                // 所以我们需要循环 2 次 (k=0, k=4)
                for (int k = 0; k < 8; k += 4) {
                    __m128 vr = _mm_loadu_ps(&rbuf[idx0 + k]);
                    __m128 vg = _mm_loadu_ps(&gbuf[idx0 + k]);
                    __m128 vb = _mm_loadu_ps(&bbuf[idx0 + k]);

                    if (config_.tone_mapping_type == 1) {
                        vr = tone_map_reinhard_sse2(vr);
                        vg = tone_map_reinhard_sse2(vg);
                        vb = tone_map_reinhard_sse2(vb);
                    }else if (config_.tone_mapping_type == 2) {
                        vr = tone_map_aces_sse2(vr);
                        vg = tone_map_aces_sse2(vg);
                        vb = tone_map_aces_sse2(vb);
                    }
                    // Clamp
                    __m128 v0 = _mm_set1_ps(0.0f);
                    __m128 v1 = _mm_set1_ps(1.0f);
                    vr = _mm_min_ps(v1, _mm_max_ps(v0, vr));
                    vg = _mm_min_ps(v1, _mm_max_ps(v0, vg));
                    vb = _mm_min_ps(v1, _mm_max_ps(v0, vb));

                    // Gamma 2.0 (Sqrt)
                    vr = _mm_sqrt_ps(vr);
                    vg = _mm_sqrt_ps(vg);
                    vb = _mm_sqrt_ps(vb);

                    // 注意 dst 需要带上 k 的偏移，否则会覆盖前 4 像素
                    uint8_t* dst = &output[(j * width + i + k) * 4];
                    store_rgba8_sse2(vr, vg, vb, dst);
                }
            #else
                float r[8], g[8], b[8];
                for (int k = 0; k < 8; ++k) {
                    size_t idx = idx0 + k;
                    vec3 c(rbuf[idx], gbuf[idx], bbuf[idx]);
                    c = apply_tone_mapping(c);

                    r[k] = ImageOps::gamma_lookup(clamp_compat(static_cast<float>(c.x()), 0.0f, 1.0f));
                    g[k] = ImageOps::gamma_lookup(clamp_compat(static_cast<float>(c.y()), 0.0f, 1.0f));
                    b[k] = ImageOps::gamma_lookup(clamp_compat(static_cast<float>(c.z()), 0.0f, 1.0f));
                }
                uint8_t* dst = &output[(j * width + i) * 4];
                store_rgba8_scalar(r, g, b, dst);
            #endif
        }

        // 处理剩余像素
        for (; i < width; ++i) {
            size_t idx = (height - 1 - j) * width + i;
            vec3 c(rbuf[idx], gbuf[idx], bbuf[idx]);
            c = apply_tone_mapping(c);

            float r = ImageOps::gamma_lookup(clamp_compat(static_cast<float>(c.x()), 0.0f, 1.0f));
            float g = ImageOps::gamma_lookup(clamp_compat(static_cast<float>(c.y()), 0.0f, 1.0f));
            float b = ImageOps::gamma_lookup(clamp_compat(static_cast<float>(c.z()), 0.0f, 1.0f));

            size_t out_idx = (j * width + i) * 4;
            output[out_idx + 0] = static_cast<uint8_t>(r * 255.99f);
            output[out_idx + 1] = static_cast<uint8_t>(g * 255.99f);
            output[out_idx + 2] = static_cast<uint8_t>(b * 255.99f);
            output[out_idx + 3] = 255;
        }
    }
}

vec3 ImageProcessor::apply_tone_mapping(const vec3& color) const {
    switch (config_.tone_mapping_type) {
        case 1: // Reinhard
            return color / (color + vec3(1.0, 1.0, 1.0));
        case 2: // ACES
            return ImageOps::ACESFilm(color);
        default:
            return color;
    }
}

void ImageProcessor::apply_post_processing(
    std::vector<unsigned char>& data,
    int w,
    int h
)const {
    ImageOps::apply_post_processing(data, w, h, config_.post_process_type);
}
