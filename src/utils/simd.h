#ifndef SIMD_H
#define SIMD_H

#include "aligned_allocator.h"
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

#if defined(__AVX2__)

inline void store_rgba8_avx2(__m256 r, __m256 g, __m256 b, uint8_t*dst_rgba){
    const __m256 v255 = _mm256_set1_ps(255.0f);

    // 1. Float [0,1] -> Int [0,255]
    // 使用 cvtps_epi32 进行转换
    __m256i ir = _mm256_cvtps_epi32(_mm256_mul_ps(r, v255));
    __m256i ig = _mm256_cvtps_epi32(_mm256_mul_ps(g, v255));
    __m256i ib = _mm256_cvtps_epi32(_mm256_mul_ps(b, v255));
    __m256i ia = _mm256_set1_epi32(255); // Alpha = 255

    // 2. Pack (32-bit -> 16-bit -> 8-bit)
    // R G R G ...
    __m256i rg = _mm256_packus_epi32(ir, ig);
    // B A B A ...
    __m256i ba = _mm256_packus_epi32(ib, ia);

    // R G B A R G B A ... (但顺序是乱序的 Lane)
    __m256i rgba = _mm256_packus_epi16(rg, ba);

    // 3. Shuffle (修复 AVX2 Lane 乱序问题并排列为 RGBA)
    // 这里的 mask 用于将数据排列成正确的线性顺序
    __m256i mask = _mm256_setr_epi8(
        0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15,
        0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15
    );
    rgba = _mm256_shuffle_epi8(rgba, mask);

    // 4. Store
    _mm256_storeu_si256((__m256i*)dst_rgba, rgba);
}


// ============================================================
//  tone_map
// ============================================================

inline __m256 tone_map_reinhard_avx2(__m256 c) {
    __m256 v1 = _mm256_set1_ps(1.0f);
    // c / (c + 1.0)
    return _mm256_div_ps(c, _mm256_add_ps(c, v1));
}

inline __m256 tone_map_aces_avx2(__m256 x) {
    const __m256 a = _mm256_set1_ps(2.51f);
    const __m256 b = _mm256_set1_ps(0.03f);
    const __m256 c = _mm256_set1_ps(2.43f);
    const __m256 d = _mm256_set1_ps(0.59f);
    const __m256 e = _mm256_set1_ps(0.14f);

    // (x * (a * x + b)) / (x * (c * x + d) + e)
    __m256 ax_b = _mm256_fmadd_ps(a, x, b); // a*x + b
    __m256 num = _mm256_mul_ps(x, ax_b);    // x*(a*x+b)

    __m256 cx_d = _mm256_fmadd_ps(c, x, d); // c*x + d
    __m256 den = _mm256_fmadd_ps(x, cx_d, e); // x*(c*x+d) + e

    return _mm256_div_ps(num, den);
}

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

#if defined(__SSE2__)

// SSE2 Reinhard
inline __m128 tone_map_reinhard_sse2(__m128 c) {
    __m128 v1 = _mm_set1_ps(1.0f);
    return _mm_div_ps(c, _mm_add_ps(c, v1));
}

inline __m128 tone_map_aces_sse2(__m128 x) {
    const __m128 a = _mm_set1_ps(2.51f);
    const __m128 b = _mm_set1_ps(0.03f);
    const __m128 c = _mm_set1_ps(2.43f);
    const __m128 d = _mm_set1_ps(0.59f);
    const __m128 e = _mm_set1_ps(0.14f);

    // 分子: x * (a * x + b)
    // SSE2 没有 fmadd，所以是 mul 然后 add
    __m128 ax_b = _mm_add_ps(_mm_mul_ps(a, x), b);
    __m128 num  = _mm_mul_ps(x, ax_b);

    // 分母: x * (c * x + d) + e
    __m128 cx_d = _mm_add_ps(_mm_mul_ps(c, x), d);
    __m128 den  = _mm_add_ps(_mm_mul_ps(x, cx_d), e);

    // 结果: num / den
    // 注意：den 最小值为 0.14 (当x>=0时)，所以不会除以零
    return _mm_div_ps(num, den);
}

inline void store_rgba8_sse2(__m128 r, __m128 g, __m128 b, uint8_t* dst) {
    __m128 v255 = _mm_set1_ps(255.0f);

    // 1. Float -> Int32
    __m128i ir = _mm_cvtps_epi32(_mm_mul_ps(r, v255));
    __m128i ig = _mm_cvtps_epi32(_mm_mul_ps(g, v255));
    __m128i ib = _mm_cvtps_epi32(_mm_mul_ps(b, v255));
    __m128i ia = _mm_set1_epi32(255);

    // 2. Pack 32 -> 16 (Signed Saturate)
    // rg: R0 R1 R2 R3 G0 G1 G2 G3 (16-bit)
    __m128i rg = _mm_packs_epi32(ir, ig);
    // ba: B0 B1 B2 B3 A0 A1 A2 A3 (16-bit)
    __m128i ba = _mm_packs_epi32(ib, ia);

    // 3. Pack 16 -> 8 (Unsigned Saturate)
    // planar: R0..3 G0..3 B0..3 A0..3 (8-bit)
    __m128i planar = _mm_packus_epi16(rg, ba);

    // 4. Interleave (Planar -> RGBA) 使用 Unpack 指令
    // 这一步在 SSE2 中替代 pshufb
    // planar: RRRR GGGG BBBB AAAA

    // ir, ig, ib, ia (32-bit)
    __m128i t1 = _mm_unpacklo_epi32(ir, ig); // R0 G0 R1 G1 (32-bit)
    __m128i t2 = _mm_unpackhi_epi32(ir, ig); // R2 G2 R3 G3
    __m128i t3 = _mm_unpacklo_epi32(ib, ia); // B0 A0 B1 A1
    __m128i t4 = _mm_unpackhi_epi32(ib, ia); // B2 A2 B3 A3

    // 现在合并成 16-bit 像素 (但数据是 32-bit 宽)
    // 我们用 packus 生成 planar，然后存到栈上，再标量写。
    alignas(16) uint8_t temp[16];
    _mm_store_si128((__m128i*)temp, planar);
    // temp: R0 R1 R2 R3 G0 G1 G2 G3 B0 B1 B2 B3 A0 A1 A2 A3

    for(int k=0; k<4; k++) {
        dst[k*4+0] = temp[k];      // R
        dst[k*4+1] = temp[k+4];    // G
        dst[k*4+2] = temp[k+8];    // B
        dst[k*4+3] = temp[k+12];   // A
    }
}

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

#endif //SIMD_H
