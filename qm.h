//
// Created by Tyler on 8/28/2026.
//

#ifndef PLAYOFFPREDICTOR_QM_H
#define PLAYOFFPREDICTOR_QM_H
#include <string>
#include <vector>
#if defined(_MSC_VER)
#include <__msvc_int128.hpp>
#define uint128 std::_Unsigned128
#else
#define uint128 __uint128_t
#endif

namespace  qm {
    std::string getStrFromQm(int n, std::vector<long long> terms, std::vector<std::string> posNames, std::vector<std::string> negNames);

    std::vector<uint128> calcSparseQm(int n, const std::vector<long long>& terms);
};


#endif //PLAYOFFPREDICTOR_QM_H
