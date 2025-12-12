#ifndef SIMD_H
#define SIMD_H

#include "image_ops.h"

#pragma once

#if defined(__SSE2__)
    #include <emmintrin.h>
#endif

#if defined(__AVX2__)
    #include <immintrin.h>
#endif

// ============================================================
// RGBA 转换相关函数
// ============================================================

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

// ============================================================
// Median 滤镜相关函数
// ============================================================

// 标量版本：9 元素排序网络中值查找
inline unsigned char median9_scalar(unsigned char* arr) {
    #define SWAP(a, b) { \
        if (arr[a] > arr[b]) { \
            unsigned char tmp = arr[a]; \
            arr[a] = arr[b]; \
            arr[b] = tmp; \
        } \
    }

    // 19 次比较的排序网络
    SWAP(0, 1); SWAP(3, 4); SWAP(6, 7);
    SWAP(1, 2); SWAP(4, 5); SWAP(7, 8);
    SWAP(0, 1); SWAP(3, 4); SWAP(6, 7);
    SWAP(0, 3); SWAP(3, 6); SWAP(0, 3);
    SWAP(1, 4); SWAP(4, 7); SWAP(1, 4);
    SWAP(2, 5); SWAP(5, 8); SWAP(2, 5);
    SWAP(1, 3); SWAP(5, 7); SWAP(2, 6);
    SWAP(2, 3); SWAP(4, 6);
    SWAP(2, 4); SWAP(3, 4); SWAP(5, 6);

    #undef SWAP
    return arr[4];
}

#if defined(__SSE2__)
// SSE2 版本：并行处理 16 个字节
inline __m128i median9_sse2(__m128i v0, __m128i v1, __m128i v2,
                             __m128i v3, __m128i v4, __m128i v5,
                             __m128i v6, __m128i v7, __m128i v8) {
    #define MIN(a, b) _mm_min_epu8(a, b)
    #define MAX(a, b) _mm_max_epu8(a, b)
    #define MINMAX(a, b) { __m128i t = MIN(a, b); b = MAX(a, b); a = t; }

    // 排序网络：19 次比较
    MINMAX(v0, v1); MINMAX(v3, v4); MINMAX(v6, v7);
    MINMAX(v1, v2); MINMAX(v4, v5); MINMAX(v7, v8);
    MINMAX(v0, v1); MINMAX(v3, v4); MINMAX(v6, v7);
    MINMAX(v0, v3); MINMAX(v3, v6); MINMAX(v0, v3);
    MINMAX(v1, v4); MINMAX(v4, v7); MINMAX(v1, v4);
    MINMAX(v2, v5); MINMAX(v5, v8); MINMAX(v2, v5);
    MINMAX(v1, v3); MINMAX(v5, v7); MINMAX(v2, v6);
    MINMAX(v2, v3); MINMAX(v4, v6);
    MINMAX(v2, v4); MINMAX(v3, v4); MINMAX(v5, v6);

    #undef MIN
    #undef MAX
    #undef MINMAX
    return v4; // 中值
}

// SSE2 辅助函数：处理 4 个像素的 median
inline void median_filter_sse2(
    const unsigned char* src_ptr,
    unsigned char* dst_ptr,
    int x, int y,
    int width, int stride,
    const int* offsets
) {
    alignas(16) unsigned char r_data[9][16];
    alignas(16) unsigned char g_data[9][16];
    alignas(16) unsigned char b_data[9][16];

    const unsigned char* row_src = src_ptr + y * stride;

    for (int k = 0; k < 9; ++k) {
        const unsigned char* p = row_src + x * 4 + offsets[k];
        for (int i = 0; i < 4; ++i) {
            r_data[k][i] = p[i * 4 + 0];
            g_data[k][i] = p[i * 4 + 1];
            b_data[k][i] = p[i * 4 + 2];
        }
    }

    __m128i vr[9], vg[9], vb[9];
    for (int k = 0; k < 9; ++k) {
        vr[k] = _mm_loadu_si128((__m128i*)r_data[k]);
        vg[k] = _mm_loadu_si128((__m128i*)g_data[k]);
        vb[k] = _mm_loadu_si128((__m128i*)b_data[k]);
    }

    __m128i median_r = median9_sse2(vr[0], vr[1], vr[2], vr[3], vr[4], vr[5], vr[6], vr[7], vr[8]);
    __m128i median_g = median9_sse2(vg[0], vg[1], vg[2], vg[3], vg[4], vg[5], vg[6], vg[7], vg[8]);
    __m128i median_b = median9_sse2(vb[0], vb[1], vb[2], vb[3], vb[4], vb[5], vb[6], vb[7], vb[8]);

    alignas(16) unsigned char result_r[16];
    alignas(16) unsigned char result_g[16];
    alignas(16) unsigned char result_b[16];
    _mm_storeu_si128((__m128i*)result_r, median_r);
    _mm_storeu_si128((__m128i*)result_g, median_g);
    _mm_storeu_si128((__m128i*)result_b, median_b);

    unsigned char* row_dst = dst_ptr + y * stride;
    for (int i = 0; i < 4; ++i) {
        int dst_idx = (x + i) * 4;
        row_dst[dst_idx + 0] = result_r[i];
        row_dst[dst_idx + 1] = result_g[i];
        row_dst[dst_idx + 2] = result_b[i];
        row_dst[dst_idx + 3] = row_src[(x + i) * 4 + 3];
    }
}
#endif // __SSE2__

#if defined(__AVX2__)
// AVX2 版本：并行处理 32 个字节
inline __m256i median9_avx2(__m256i v0, __m256i v1, __m256i v2,
                             __m256i v3, __m256i v4, __m256i v5,
                             __m256i v6, __m256i v7, __m256i v8) {
    #define MIN(a, b) _mm256_min_epu8(a, b)
    #define MAX(a, b) _mm256_max_epu8(a, b)
    #define MINMAX(a, b) { __m256i t = MIN(a, b); b = MAX(a, b); a = t; }

    // 排序网络：19 次比较
    MINMAX(v0, v1); MINMAX(v3, v4); MINMAX(v6, v7);
    MINMAX(v1, v2); MINMAX(v4, v5); MINMAX(v7, v8);
    MINMAX(v0, v1); MINMAX(v3, v4); MINMAX(v6, v7);
    MINMAX(v0, v3); MINMAX(v3, v6); MINMAX(v0, v3);
    MINMAX(v1, v4); MINMAX(v4, v7); MINMAX(v1, v4);
    MINMAX(v2, v5); MINMAX(v5, v8); MINMAX(v2, v5);
    MINMAX(v1, v3); MINMAX(v5, v7); MINMAX(v2, v6);
    MINMAX(v2, v3); MINMAX(v4, v6);
    MINMAX(v2, v4); MINMAX(v3, v4); MINMAX(v5, v6);

    #undef MIN
    #undef MAX
    #undef MINMAX
    return v4; // 中值
}

// AVX2 辅助函数：处理 8 个像素的 median
inline void median_filter_avx2(
    const unsigned char* src_ptr,
    unsigned char* dst_ptr,
    int x, int y,
    int width, int stride,
    const int* offsets
) {
    alignas(32) unsigned char r_data[9][32];
    alignas(32) unsigned char g_data[9][32];
    alignas(32) unsigned char b_data[9][32];

    const unsigned char* row_src = src_ptr + y * stride;

    for (int k = 0; k < 9; ++k) {
        const unsigned char* p = row_src + x * 4 + offsets[k];
        for (int i = 0; i < 8; ++i) {
            r_data[k][i] = p[i * 4 + 0];
            g_data[k][i] = p[i * 4 + 1];
            b_data[k][i] = p[i * 4 + 2];
        }
    }

    __m256i vr[9], vg[9], vb[9];
    for (int k = 0; k < 9; ++k) {
        vr[k] = _mm256_loadu_si256((__m256i*)r_data[k]);
        vg[k] = _mm256_loadu_si256((__m256i*)g_data[k]);
        vb[k] = _mm256_loadu_si256((__m256i*)b_data[k]);
    }

    __m256i median_r = median9_avx2(vr[0], vr[1], vr[2], vr[3], vr[4], vr[5], vr[6], vr[7], vr[8]);
    __m256i median_g = median9_avx2(vg[0], vg[1], vg[2], vg[3], vg[4], vg[5], vg[6], vg[7], vg[8]);
    __m256i median_b = median9_avx2(vb[0], vb[1], vb[2], vb[3], vb[4], vb[5], vb[6], vb[7], vb[8]);

    alignas(32) unsigned char result_r[32];
    alignas(32) unsigned char result_g[32];
    alignas(32) unsigned char result_b[32];
    _mm256_storeu_si256((__m256i*)result_r, median_r);
    _mm256_storeu_si256((__m256i*)result_g, median_g);
    _mm256_storeu_si256((__m256i*)result_b, median_b);

    unsigned char* row_dst = dst_ptr + y * stride;
    for (int i = 0; i < 8; ++i) {
        int dst_idx = (x + i) * 4;
        row_dst[dst_idx + 0] = result_r[i];
        row_dst[dst_idx + 1] = result_g[i];
        row_dst[dst_idx + 2] = result_b[i];
        row_dst[dst_idx + 3] = row_src[(x + i) * 4 + 3];
    }
}
#endif // __AVX2__

#endif //SIMD_H
