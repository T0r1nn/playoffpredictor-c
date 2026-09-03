//
// Created by Tyler on 8/28/2026.
//

#ifndef PLAYOFFPREDICTOR_QM_H
#define PLAYOFFPREDICTOR_QM_H
#include <bitset>
#include <string>
#include <vector>
#if defined(_MSC_VER)
#include <__msvc_int128.hpp>
#define uint128 std::_Unsigned128
#else
#define uint128 __uint128_t
#endif

namespace  qm {
    std::string getStrFromQm(int n, std::vector<std::bitset<256>> *terms, std::vector<std::string> posNames, std::vector<std::string> negNames);
    uint64_t toTernary(uint64_t term, int n);
    inline uint64_t pow3(int e);
    struct HashSet;
    std::vector<long long> phase2Qm(int n, HashSet primes, std::vector<long long>& terms);
    std::vector<long long> skipPhase2(int n, const HashSet& p);
    HashSet calcDenseQm(int n, std::vector<std::bitset<256>> S);
};


#endif //PLAYOFFPREDICTOR_QM_H
