//
// Created by Tyler on 8/28/2026.
//

#ifndef PLAYOFFPREDICTOR_QM_H
#define PLAYOFFPREDICTOR_QM_H
#include <string>
#include <vector>


namespace  qm {
    std::string getStrFromQm(int n, std::vector<long long> qm, std::vector<std::string> posNames, std::vector<std::string> negNames);

    std::vector<long long> calcSparseQm(int n, const std::vector<long long>& terms);
};


#endif //PLAYOFFPREDICTOR_QM_H
