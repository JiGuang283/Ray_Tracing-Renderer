#ifndef SIMD_RGBA_H
#define SIMD_RGBA_H

#pragma once
#include "image_ops.h"

#if defined(__SSE2__)
    #include <emmintrin.h>
#endif

#if defined(__AVX2__)
    #include <immintrin.h>
#endif

// 标量 fallback：处理 8 个像素
inline void store_rgba8_scalar(const float* r, const float* g, const float* b, uint8_t* dst_rgba) {
    for (int k = 0; k < 8; ++k) {
        float rf = clamp_compat(r[k], 0.0f, 1.0f) * 255.0f;
        float gf = clamp_compat(g[k], 0.0f, 1.0f) * 255.0f;
        float bf = clamp_compat(b[k], 0.0f, 1.0f) * 255.0f;
        dst_rgba[4 * k + 0] = static_cast<uint8_t>(rf);
        dst_rgba[4 * k + 1] = static_cast<uint8_t>(gf);
        dst_rgba[4 * k + 2] = static_cast<uint8_t>(bf);
        dst_rgba[4 * k + 3] = 255;
    }
}

#if defined(__SSE2__)
// SSE2 处理 8 像素：拆成两批 4 个
inline void store_rgba8_sse2(const float* r, const float* g, const float* b, uint8_t* dst_rgba) {
    __m128 v255 = _mm_set1_ps(255.0f);
    __m128 v0   = _mm_set1_ps(0.0f);
    __m128 v1   = _mm_set1_ps(1.0f);

    // 处理 8 个像素
    auto do4 = [&](int off, uint8_t* dst) {
        __m128 vr = _mm_loadu_ps(r + off);
        __m128 vg = _mm_loadu_ps(g + off);
        __m128 vb = _mm_loadu_ps(b + off);

        // 限制在0-1之间
        vr = _mm_min_ps(v1, _mm_max_ps(v0, vr));
        vg = _mm_min_ps(v1, _mm_max_ps(v0, vg));
        vb = _mm_min_ps(v1, _mm_max_ps(v0, vb));

        // 乘以255
        vr = _mm_mul_ps(vr, v255);
        vg = _mm_mul_ps(vg, v255);
        vb = _mm_mul_ps(vb, v255);

        // 将浮点数转换为整数
        __m128i ir = _mm_cvtps_epi32(vr);
        __m128i ig = _mm_cvtps_epi32(vg);
        __m128i ib = _mm_cvtps_epi32(vb);

        // 保存
        alignas(16) int ri[4], gi[4], bi[4];
        _mm_store_si128(reinterpret_cast<__m128i*>(ri), ir);
        _mm_store_si128(reinterpret_cast<__m128i*>(gi), ig);
        _mm_store_si128(reinterpret_cast<__m128i*>(bi), ib);

        for (int k = 0; k < 4; ++k) {
            dst[4 * k + 0] = static_cast<uint8_t>(ri[k]);
            dst[4 * k + 1] = static_cast<uint8_t>(gi[k]);
            dst[4 * k + 2] = static_cast<uint8_t>(bi[k]);
            dst[4 * k + 3] = 255;
        }
    };

    do4(0, dst_rgba);
    do4(4, dst_rgba + 16);
}
#endif // __SSE2__

#if defined(__AVX2__)
// AVX2 处理 8 像素：一批完成
inline void store_rgba8_avx2(const float* r, const float* g, const float* b, uint8_t* dst_rgba) {
    // 初始化向量
    __m256 v255 = _mm256_set1_ps(255.0f);
    __m256 v0   = _mm256_set1_ps(0.0f);
    __m256 v1   = _mm256_set1_ps(1.0f);

    __m256 vr = _mm256_loadu_ps(r);
    __m256 vg = _mm256_loadu_ps(g);
    __m256 vb = _mm256_loadu_ps(b);

    // 限制在0-1之间
    vr = _mm256_min_ps(v1, _mm256_max_ps(v0, vr));
    vg = _mm256_min_ps(v1, _mm256_max_ps(v0, vg));
    vb = _mm256_min_ps(v1, _mm256_max_ps(v0, vb));

    // 乘以255
    vr = _mm256_mul_ps(vr, v255);
    vg = _mm256_mul_ps(vg, v255);
    vb = _mm256_mul_ps(vb, v255);

    // 将浮点数转换为整数
    __m256i ir = _mm256_cvtps_epi32(vr);
    __m256i ig = _mm256_cvtps_epi32(vg);
    __m256i ib = _mm256_cvtps_epi32(vb);

    // 将整数保存到数组中
    alignas(32) int ri[8], gi[8], bi[8];
    _mm256_store_si256(reinterpret_cast<__m256i*>(ri), ir);
    _mm256_store_si256(reinterpret_cast<__m256i*>(gi), ig);
    _mm256_store_si256(reinterpret_cast<__m256i*>(bi), ib);

    //TODO：未来可使用unpack/shuffle更进一步优化
    for (int k = 0; k < 8; ++k) {
        dst_rgba[4 * k + 0] = static_cast<uint8_t>(ri[k]);
        dst_rgba[4 * k + 1] = static_cast<uint8_t>(gi[k]);
        dst_rgba[4 * k + 2] = static_cast<uint8_t>(bi[k]);
        dst_rgba[4 * k + 3] = 255;
    }

}
#endif // __AVX2__

#endif //SIMD_RGBA_H
