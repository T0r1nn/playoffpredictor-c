//
// Created by Tyler on 9/9/2026.
//

#include "cuda.cuh"

#include <chrono>
#include "cuda_profiler_api.h"

#include "ts.h"

#define CUDA_CHECK(expr_to_check) do {            \
    cudaError_t result  = expr_to_check;          \
    if(result != cudaSuccess)                     \
    {                                             \
        fprintf(stderr,                           \
                "CUDA Runtime Error: %s:%i:%d = %s\n", \
                __FILE__,                         \
                __LINE__,                         \
                result,\
                cudaGetErrorString(result));      \
    }                                             \
} while(0)



namespace cuda {
    __device__ unsigned long long getSeason(unsigned long long unplayed, unsigned long long season, const unsigned long long index) {
        unsigned long long x = index;
        unsigned long long y = unplayed;
        unsigned long long mask = 0;
        while (x > 0ULL) {//O(n) since len(x) == 1
            const unsigned long long res = x & 1ULL;//result of match = LSB of x
            const unsigned long long z = y & y - 1ULL;//z = y with rightmost 1 removed
            const unsigned long long maskAdd = res == 0ULL ? 0ULL : y-z;//maskAdd = rightmost 1 of y
            mask |= maskAdd;//add maskAdd to the mask
            y = z;//remove rightmost 1 from y
            x = x>>1;//shift x over 1
        }

        return season | mask;
    }

    __device__ void calculateSeason(int teamCount, const unsigned long long season, int *order, int *highSeed, int *lowSeed, const long long *t1Masks, const long long *t2Masks, const int *shifts, unsigned long long *results, int *wins, int *tiebreakers) {
        const unsigned long long invSeason = ~season;
        for (int i = 0; i < teamCount; i ++) {//O(m^2)
            results[i] = (invSeason & t1Masks[i]) | (season & t2Masks[i]);
            wins[i] = 0;
            tiebreakers[i] = 0;
            order[i] = i;
            unsigned long long x = results[i];
            while (x > 0) {
                x &= x-1ULL;
                wins[i] += 1;
            }
            for (int team = 0; team < i; team ++) {
                if (wins[team] != wins[i]) {
                    continue;
                }
                if ((results[i] & (1ULL << (i + shifts[team]))) == 0) {
                    tiebreakers[team] += 1;
                } else {
                    tiebreakers[i] += 1;
                }
            }
        }

        for (int i = 1; i < teamCount; i ++) {
            int j = i-1;
            const int value = order[i];
            while (j >= 0 && (wins[order[j]] < wins[value] || (wins[order[j]] == wins[value] && tiebreakers[order[j]] < tiebreakers[value]))) {
                order[j+1] = order[j];
                j = j - 1;
            }
            order[j+1] = value;
        }

        for (int i = 0; i < teamCount; i += 1) {
            int newMin = i;
            int newMax = i;
            for (int j = i-1; j >= 0; j --) {
                if (wins[order[i]] == wins[order[j]] and tiebreakers[order[i]] == tiebreakers[order[j]]) {
                    newMin = j;
                }else {
                    break;
                }
            }
            for (int j = i+1; j < teamCount; j ++) {
                if (wins[order[i]] == wins[order[j]] and tiebreakers[order[i]] == tiebreakers[order[j]]) {
                    newMax = j;
                }else {
                    break;
                }
            }
            highSeed[order[i]] = newMin;
            lowSeed[order[i]] = newMax;
        }
    }

    __global__ void runPerSeason(int teamCount, int unplayedCount, unsigned long long unplayed, unsigned long long season, float *fstats, int *istats, const long long *t1Masks, const long long *t2Masks, const int *shifts) {
        int *order = static_cast<int *>(malloc(sizeof(int) * teamCount));
        int *highSeed = static_cast<int *>(malloc(sizeof(int) * teamCount));
        int *lowSeed = static_cast<int *>(malloc(sizeof(int) * teamCount));

        auto *results = static_cast<unsigned long long *>(malloc(sizeof(unsigned long long) * teamCount));
        auto *wins = static_cast<int*>(malloc(sizeof(int) * teamCount));
        auto *tiebreakers = static_cast<int*>(malloc(teamCount * sizeof(int)));

        unsigned int pos = threadIdx.x + blockDim.x * blockIdx.x;
        unsigned int total = blockDim.x * gridDim.x;
        unsigned long long maxSeason = 1ULL << unplayedCount;
        unsigned long long start = pos * maxSeason / total;
        unsigned long long end = (pos+1) * maxSeason / total;

        for (unsigned int u = start; u < end; u++) {
            unsigned long long nSeason = getSeason(unplayed, season, u);
            calculateSeason(teamCount, nSeason, order, highSeed, lowSeed, t1Masks, t2Masks, shifts, results, wins, tiebreakers);

            for (int i = 0; i < teamCount; i ++) {
                const int team = order[i];
                const float seed = static_cast<float>(highSeed[team] + lowSeed[team])/2.0f;
                fstats[pos*teamCount*2 + 2*team] += seed;
                int a = istats[pos*teamCount*2 + 2*team];
                int b = highSeed[team];
                istats[pos*teamCount*2 + 2*team] = a < b ? a : b;
                a = istats[pos*teamCount*2 + 2*team + 1];
                b = lowSeed[team];
                istats[pos*teamCount*2 + 2*team + 1] = a < b ? b : a;
                const float range = 5.0f - highSeed[team] + 1.0f;
                const float coveredRange = range > 0 ? range : 0;
                const float totalRange = lowSeed[team] - highSeed[team] + 1;
                fstats[pos*teamCount*2 + 2*team + 1] += std::min(coveredRange/totalRange, 1.0f);
            }
        }
        free(order);
        free(highSeed);
        free(lowSeed);

        free(results);
        free(wins);
        free(tiebreakers);
    }

    void cudaRunPerSeason(int blockCount, int blockSize, const seasonData &data, const ts::statsProcessor &proc) {
        float *g_fstats = nullptr;
        int *g_istats = nullptr;
        long long *g_t1Masks = nullptr;
        long long *g_t2Masks = nullptr;
        int *g_shifts = nullptr;

        cudaMallocManaged(&g_fstats, sizeof(float)*blockCount * blockSize * data.teamCount * 2);
        cudaMallocManaged(&g_istats, sizeof(int)*blockCount * blockSize * data.teamCount * 2);
        cudaMallocManaged(&g_t1Masks, sizeof(long long) * data.teamCount);
        cudaMallocManaged(&g_t2Masks, sizeof(long long) * data.teamCount);
        cudaMallocManaged(&g_shifts, sizeof(int) * data.teamCount);

        for (int i = 0; i < blockCount * blockSize; i++) {
            for (int j = 0; j < data.teamCount; j++) {
                g_istats[i * data.teamCount * 2 + j * 2] = 9;
                g_istats[i * data.teamCount * 2 + j * 2 + 1] = 0;
                g_fstats[i * data.teamCount * 2 + j * 2] = 0.0;
                g_fstats[i * data.teamCount * 2 + j * 2 + 1] = 0.0;
            }
        }

        for (int i = 0; i < data.teamCount; i++) {
            g_t1Masks[i] = data.t1Masks[i];
            g_t2Masks[i] = data.t2Masks[i];
            g_shifts[i] = data.shifts[i];
        }

        auto start = std::chrono::high_resolution_clock::now();

        cudaProfilerStart();
        runPerSeason<<<blockCount, blockSize>>>(data.teamCount, data.unplayedCount, data.unplayed, data.season, g_fstats, g_istats, g_t1Masks, g_t2Masks, g_shifts);

        cudaDeviceSynchronize();
        cudaProfilerStop();

        auto end = std::chrono::high_resolution_clock::now();

        std::printf("%f\n", (end-start).count()/1.0e6);

        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaGetLastError());

        for (int i = 0; i < blockCount * blockSize; i++) {
            for (int j = 0; j < data.teamCount; j++) {
                proc.istats[i * data.teamCount * 2 + j * 2] = g_istats[i * data.teamCount * 2 + j * 2];
                proc.istats[i * data.teamCount * 2 + j * 2+1] = g_istats[i * data.teamCount * 2 + j * 2 + 1];
                proc.fstats[i * data.teamCount * 2 + j * 2] = g_fstats[i * data.teamCount * 2 + j * 2];
                proc.fstats[i * data.teamCount * 2 + j * 2+1] = g_fstats[i * data.teamCount * 2 + j * 2 + 1];
            }
        }

        cudaFree(g_fstats);
        cudaFree(g_istats);
        cudaFree(g_t1Masks);
        cudaFree(g_t2Masks);
        cudaFree(g_shifts);
    }
} // cuda