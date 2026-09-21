//
// Created by Tyler on 9/9/2026.
//

#ifndef PLAYOFFPREDICTOR_CUDA_CUH
#define PLAYOFFPREDICTOR_CUDA_CUH
#include "ts.h"

namespace cuda {
    struct seasonData {
        int teamCount = 0;
        int unplayedCount = 0;
        unsigned long long season = 0;
        unsigned long long unplayed = 0;
        long long *t1Masks{};
        long long *t2Masks{};
        int *shifts{};

        explicit seasonData(const ts::seasonData &s) : teamCount(s.teamCount),unplayedCount(s.unplayedCount),season(s.season),unplayed(s.unplayed),t1Masks(s.t1Masks),t2Masks(s.t2Masks),shifts(s.shifts) {}
    };

    void cudaRunPerSeason(int blockCount, int blockSize, const seasonData &data, const ts::statsProcessor &proc);
} // cuda

#endif //PLAYOFFPREDICTOR_CUDA_CUH
