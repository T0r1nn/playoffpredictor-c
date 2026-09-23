//
// Created by Tyler on 8/28/2026.
//

#ifndef PLAYOFFPREDICTOR_QM_H
#define PLAYOFFPREDICTOR_QM_H
#include <bitset>
#include <string>
#include <vector>

namespace  qm {
    std::string getStrFromQm(int n, std::vector<std::bitset<256>> *terms, std::vector<std::string> posNames, std::vector<std::string> negNames);
    unsigned long long toTernary(unsigned long long term, int n);
    unsigned long long pow3(int e);
    struct HashSet;
    std::vector<unsigned long long> phase2Qm(int n, const std::vector<unsigned long long> &primes, std::vector<long long>& terms);
    std::vector<unsigned long long> skipPhase2(int n, const std::vector<unsigned long long>& p);
    HashSet calcDenseQm(int n, std::vector<std::bitset<256>> S);
};


#endif //PLAYOFFPREDICTOR_QM_H
