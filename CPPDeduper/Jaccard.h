#pragma once

// Code used / adapted from
//  https://gudok.xyz/minhash1/
//  https://github.com/andreigudkov/articles

#include <string>
#include <stdexcept>
#include <vector>
#include <chrono>
#include <cstdint>

#include "isalphanum.h"

#ifdef __GNUC__
#include <nmmintrin.h>
#include <emmintrin.h>
#else
#include <nmmintrin.h>
#include <emmintrin.h>
#endif

double JaccardClassical(const uint32_t* fng1, int len1, const uint32_t* fng2, int len2, double )
{
    int pos1 = 0;
    int pos2 = 0;
    int nintersect = 0;
    while (pos1 < len1 && pos2 < len2) {
        if (fng1[pos1] == fng2[pos2]) {
            nintersect++;
            pos1++;
            pos2++;
        }
        else if (fng1[pos1] < fng2[pos2]) {
            pos1++;
        }
        else {
            pos2++;
        }
    }
    int nunion = len1 + len2 - nintersect;
    return nintersect / (double)nunion;
}

double JaccardFast(const uint32_t* fng1, int len1, const uint32_t* fng2, int len2, double alpha)
{
    int smin = (int)std::ceil((1.0 - alpha) / (1.0 + alpha) * (len1 + len2));
    int pos1 = 0;
    int pos2 = 0;
    int nintersect = 0;
    int s = 0;
    while (pos1 < len1 && pos2 < len2) {
        if (fng1[pos1] == fng2[pos2]) {
            nintersect++;
            pos1++;
            pos2++;
        }
        else if (fng1[pos1] < fng2[pos2]) {
            pos1++;
            s++;
        }
        else {
            pos2++;
            s++;
        }
        if (s > smin) {
            return 0.0;
        }
    }
    int nunion = len1 + len2 - nintersect;
    return nintersect / (double)nunion;
}

double JaccardTurbo(const uint32_t* fng1, int len1, const uint32_t* fng2, int len2, double alpha)
{
    int smin = (int)std::ceil((1.0 - alpha) / (1.0 + alpha) * (len1 + len2));
    int pos1 = 0;
    int pos2 = 0;
    int nintersect = 0;
    int s = 0;
    while (pos1 + 4 <= len1 && pos2 + 4 <= len2) {
        __m128i v1 = _mm_loadu_si128((const __m128i*)(fng1 + pos1));
        __m128i v2 = _mm_loadu_si128((const __m128i*)(fng2 + pos2));
        uint64_t m = _mm_cvtsi128_si64(_mm_cmpestrm(v1, 8, v2, 8, _SIDD_UWORD_OPS | _SIDD_CMP_EQUAL_ANY | _SIDD_BIT_MASK));
        if (m) {
            for (int i = 0; i < 4; i++) {
                if (fng1[pos1] == fng2[pos2]) {
                    nintersect++;
                    pos1++;
                    pos2++;
                }
                else if (fng1[pos1] < fng2[pos2]) {
                    pos1++;
                    s++;
                }
                else {
                    pos2++;
                    s++;
                }
            }
        }
        else {
            if (fng1[pos1 + 3] < fng2[pos2 + 3]) {
                pos1 += 4;
            }
            else {
                pos2 += 4;
            }
            s += 4;
        }
        if (s > smin) {
            return 0.0;
        }
    }
    while (pos1 < len1 && pos2 < len2) {
        if (fng1[pos1] == fng2[pos2]) {
            nintersect++;
            pos1++;
            pos2++;
        }
        else if (fng1[pos1] < fng2[pos2]) {
            pos1++;
        }
        else {
            pos2++;
        }
    }

    int nunion = len1 + len2 - nintersect;
    return nintersect / (double)nunion;
}

#pragma message("need to validate jaccard64 bit")
double JaccardTurbo(const uint64_t* fng1, int len1, const uint64_t* fng2, int len2, double alpha)
{
    int smin = (int)std::ceil((1.0 - alpha) / (1.0 + alpha) * (len1 + len2));
    int pos1 = 0;
    int pos2 = 0;
    int nintersect = 0;
    int s = 0;
    while (pos1 + 2 <= len1 && pos2 + 2 <= len2) {
        __m128i v1 = _mm_loadu_si128((const __m128i*)(fng1 + pos1));
        __m128i v2 = _mm_loadu_si128((const __m128i*)(fng2 + pos2));
        uint64_t m = _mm_cvtsi128_si64(_mm_cmpestrm(v1, 8, v2, 8, _SIDD_UWORD_OPS | _SIDD_CMP_EQUAL_ANY | _SIDD_BIT_MASK));
        if (m) {
            for (int i = 0; i < 2; i++) {
                if (fng1[pos1] == fng2[pos2]) {
                    nintersect++;
                    pos1++;
                    pos2++;
                }
                else if (fng1[pos1] < fng2[pos2]) {
                    pos1++;
                    s++;
                }
                else {
                    pos2++;
                    s++;
                }
            }
        }
        else {
            if (fng1[pos1 + 1] < fng2[pos2 + 1]) {
                pos1 += 2;
            }
            else {
                pos2 += 2;
            }
            s += 2;
        }
        if (s > smin) {
            return 0.0;
        }
    }
    while (pos1 < len1 && pos2 < len2) {
        if (fng1[pos1] == fng2[pos2]) {
            nintersect++;
            pos1++;
            pos2++;
        }
        else if (fng1[pos1] < fng2[pos2]) {
            pos1++;
        }
        else {
            pos2++;
        }
    }

    int nunion = len1 + len2 - nintersect;
    return nintersect / (double)nunion;
}