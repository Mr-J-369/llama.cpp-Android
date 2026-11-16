#pragma once

// Computes C[M x N] += A[M x K] * B[K x N]

#include "simd-mappings.h"

// Use fixed-width 128-bit NEON registers when __ARM_FEATURE_SVE is also enabled
#if defined(GGML_SIMD) && (!defined(__ARM_FEATURE_SVE) || (defined(__ARM_NEON) && defined(__ARM_FEATURE_FMA) && defined(__ARM_FP16_FORMAT_IEEE)))

#if defined(__ARM_NEON) && defined(__ARM_FEATURE_FMA) && defined(__ARM_FP16_FORMAT_IEEE)
    #define GGML_GEMM_F32_EPR       4
    #define GGML_GEMM_F32_VEC       GGML_F32x4
    #define GGML_GEMM_F32_VEC_LOAD  GGML_F32x4_LOAD
    #define GGML_GEMM_F32_VEC_STORE GGML_F32x4_STORE
    #define GGML_GEMM_F32_VEC_SET1  GGML_F32x4_SET1
    #define GGML_GEMM_F32_VEC_FMA   GGML_F32x4_FMA
    static constexpr int GEMM_RM = 4;
    static constexpr int GEMM_RN = 4; // 16+4+1 = 21/32
#else
    #define GGML_GEMM_F32_EPR       GGML_F32_EPR
    #define GGML_GEMM_F32_VEC       GGML_F32_VEC
    #define GGML_GEMM_F32_VEC_LOAD  GGML_F32_VEC_LOAD
    #define GGML_GEMM_F32_VEC_STORE GGML_F32_VEC_STORE
    #define GGML_GEMM_F32_VEC_SET1  GGML_F32_VEC_SET1
    #define GGML_GEMM_F32_VEC_FMA   GGML_F32_VEC_FMA
    static constexpr int GEMM_RM = 2;
    static constexpr int GEMM_RN = 2;
#endif

template <int RM, int RN>
static inline void simd_gemm_ukernel(
    float       * GGML_RESTRICT C,
    const float * GGML_RESTRICT A,
    const float * GGML_RESTRICT B,
    int K, int N)
{
    static constexpr int KN = GGML_GEMM_F32_EPR;

    GGML_GEMM_F32_VEC acc[RM][RN];
    for (int64_t i = 0; i < RM; i++) {
        for (int r = 0; r < RN; r++) {
            acc[i][r] = GGML_GEMM_F32_VEC_LOAD(C + i * N + r * KN);
        }
    }

    for (int64_t kk = 0; kk < K; kk++) {
        GGML_GEMM_F32_VEC Bv[RN];
        for (int r = 0; r < RN; r++) {
            Bv[r] = GGML_GEMM_F32_VEC_LOAD(B + kk * N + r * KN);
        }
        for (int64_t i = 0; i < RM; i++) {
            GGML_GEMM_F32_VEC p = GGML_GEMM_F32_VEC_SET1(A[i * K + kk]);
            for (int r = 0; r < RN; r++) {
                acc[i][r] = GGML_GEMM_F32_VEC_FMA(acc[i][r], Bv[r], p);
            }
        }
    }

    for (int64_t i = 0; i < RM; i++) {
        for (int r = 0; r < RN; r++) {
            GGML_GEMM_F32_VEC_STORE(C + i * N + r * KN, acc[i][r]);
        }
    }
}

template <int RM>
static inline void simd_gemm_ukernel_tail(
    float       * GGML_RESTRICT C,
    const float * GGML_RESTRICT A,
    const float * GGML_RESTRICT B,
    int K, int N, int cols)
{
    for (int64_t j = 0; j < cols; j++) {
        for (int64_t i = 0; i < RM; i++) {
            float a = C[i * N + j];
            for (int64_t kk = 0; kk < K; kk++) {
                a += A[i * K + kk] * B[kk * N + j];
            }
            C[i * N + j] = a;
        }
    }
}

// C[M x N] += A[M x K] * B[K x N]
static void simd_gemm(
    float       * GGML_RESTRICT C,
    const float * GGML_RESTRICT A,
    const float * GGML_RESTRICT B,
    int M, int K, int N)
{
    static constexpr int KN = GGML_GEMM_F32_EPR;

    int64_t ii = 0;
    for (; ii + GEMM_RM <= M; ii += GEMM_RM) {
        int64_t jj = 0;
        for (; jj + GEMM_RN * KN <= N; jj += GEMM_RN * KN) {
            simd_gemm_ukernel<GEMM_RM, GEMM_RN>(C + jj, A, B + jj, K, N);
        }
        for (; jj + KN <= N; jj += KN) {
            simd_gemm_ukernel<GEMM_RM, 1>(C + jj, A, B + jj, K, N);
        }
        if (jj < N) {
            simd_gemm_ukernel_tail<GEMM_RM>(C + jj, A, B + jj, K, N, N - jj);
        }

        A += GEMM_RM * K;
        C += GEMM_RM * N;
    }

    // Tail rows: one at a time
    for (; ii < M; ii++) {
        int64_t jj = 0;
        for (; jj + GEMM_RN * KN <= N; jj += GEMM_RN * KN) {
            simd_gemm_ukernel<1, GEMM_RN>(C + jj, A, B + jj, K, N);
        }
        for (; jj + KN <= N; jj += KN) {
            simd_gemm_ukernel<1, 1>(C + jj, A, B + jj, K, N);
        }
        if (jj < N) {
            simd_gemm_ukernel_tail<1>(C + jj, A, B + jj, K, N, N - jj);
        }

        A += K;
        C += N;
    }
}

#undef GGML_GEMM_F32_EPR
#undef GGML_GEMM_F32_VEC
#undef GGML_GEMM_F32_VEC_LOAD
#undef GGML_GEMM_F32_VEC_STORE
#undef GGML_GEMM_F32_VEC_SET1
#undef GGML_GEMM_F32_VEC_FMA
#else // scalar path

static void simd_gemm(
    float       * GGML_RESTRICT C,
    const float * GGML_RESTRICT A,
    const float * GGML_RESTRICT B,
    int M, int K, int N)
{
    for (int64_t i = 0; i < M; i++) {
        for (int64_t j = 0; j < N; j++) {
            float sum = C[i * N + j];
            for (int64_t kk = 0; kk < K; kk++) {
                sum += A[i * K + kk] * B[kk * N + j];
            }
            C[i * N + j] = sum;
        }
    }
}

#endif // GGML_SIMD
