//
// Created by Tyler on 9/7/2026.
//

#include "ts.h"

#include <bitset>
#include <chrono>
#include <condition_variable>
#include <format>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "qm.h"

namespace ts {

    unsigned long long getSeason(const seasonData &data, const unsigned long long index) {
        unsigned long long x = index;
        unsigned long long y = data.unplayed;
        unsigned long long mask = 0;
        while (x > 0) {//O(n) since len(x) == 1
            const int res = static_cast<int>(x & 1LL);//result of match = LSB of x
            const unsigned long long z = y & y-1LL;//z = y with rightmost 1 removed
            const unsigned long long maskAdd = res == 0 ? 0LL : y-z;//maskAdd = rightmost 1 of y
            mask |= maskAdd;//add maskAdd to the mask
            y = z;//remove rightmost 1 from y
            x = x>>1;//shift x over 1
        }

        return data.season | mask;
    }

    void calculateSeason(const unsigned long long season, std::vector<int> *order, std::vector<int> *highSeed, std::vector<int> *lowSeed, const seasonData &data) {
        std::vector<unsigned long long> results(data.teamCount);
        std::vector<int> wins(data.teamCount);
        std::vector<int> tiebreakers(data.teamCount);
        const unsigned long long invSeason = ~season;
        for (int i = 0; i < data.teamCount; i ++) {//O(m^2)
            results[i] = (invSeason & data.t1Masks[i]) | (season & data.t2Masks[i]);
            wins[i] = 0;
            tiebreakers[i] = 0;
            order->at(i) = i;
            unsigned long long x = results[i];
            while (x > 0) {
                x &= x-1;
                wins[i] += 1;
            }
            for (int team = 0; team < i; team ++) {
                if (wins[team] != wins[i]) {
                    continue;
                }
                if ((results[i] & (1LL << (i + data.shifts[team]))) == 0) {
                    tiebreakers[team] += 1;
                } else {
                    tiebreakers[i] += 1;
                }
            }
        }

        for (int i = 1; i < data.teamCount; i ++) {
            int j = i-1;
            const int value = order->at(i);
            while (j >= 0 && (wins[order->at(j)] < wins[value] || (wins[order->at(j)] == wins[value] && tiebreakers[order->at(j)] < tiebreakers[value]))) {
                order->at(j+1) = order->at(j);
                j = j - 1;
            }
            order->at(j+1) = value;
        }

        for (int i = 0; i < data.teamCount; i += 1) {
            int newMin = i;
            int newMax = i;
            for (int j = i-1; j >= 0; j --) {
                if (wins[order->at(i)] == wins[order->at(j)] and tiebreakers[order->at(i)] == tiebreakers[order->at(j)]) {
                    newMin = j;
                }else {
                    break;
                }
            }
            for (int j = i+1; j < data.teamCount; j ++) {
                if (wins[order->at(i)] == wins[order->at(j)] and tiebreakers[order->at(i)] == tiebreakers[order->at(j)]) {
                    newMax = j;
                }else {
                    break;
                }
            }
            highSeed->at(order->at(i)) = newMin;
            lowSeed->at(order->at(i)) = newMax;
        }
    }

    void foldyProcessor::setupData(int dThreadCount, int sThreadCount, seasonData data) {
        foldyRows.resize(1ULL << data.unplayedCount, "");
        zeroNames.reserve(data.unplayedCount);
        oneNames.reserve(data.unplayedCount);
        sdata = data;

        for (int i = 0; i < sdata.unplayedCount; i++) {
            unsigned long long matchMask = 0;
            unsigned long long y = sdata.unplayed;
            for (int j = 0; j < i+1; j++) {
                unsigned long long z = y & y-1LL;
                matchMask = y-z;
                y = z;
            }
            int x = 0;
            unsigned long long m = matchMask;
            while (m > 1) {
                x += 1;
                m = m >> 1;
            }
            int a = 0;
            int b = 0;
            while (b < sdata.teamCount && sdata.shifts[b] < x) {
                b += 1;
            }
            b -= 1;
            a = x - sdata.shifts[b];
            while (a <= b) {
                b -= 1;
                a = x-sdata.shifts[b];
            }
            oneNames.push_back(sdata.teamNames[b]);
            zeroNames.push_back(sdata.teamNames[a]);

            headerRow += oneNames[i] + " vs " + zeroNames[i] + ",";
        }

        for (int i = 0; i < sdata.teamCount; i++) {
            headerRow += std::format("Team {}, Best Seed, Worst Seed", i) + (i + 1 < sdata.teamCount ? "," : "");
        }
    }

    void foldyProcessor::processSeason(const std::vector<int> &order, const std::vector<int> &highSeed, const std::vector<int> &lowSeed, unsigned long long season, long long u, int threadPos) {
        /*
         * Steps:
         * 1. Get match results, print winning team shortcodes.
         * 2. Print order, highest possible seed, and lowest possible seed
         * 3. String is stored in foldyRows[u]
         */
        std::string row;
        for (int i = 0; i < sdata.unplayedCount; i++) {
            if (((u >> i) & 1) == 1) {
                row += oneNames[i] + ",";
            } else {
                row += zeroNames[i] + ",";
            }
        }
        for (int i = 0; i < order.size(); i++){
            const int team = order[i];
            row += sdata.teamNames[team] + ",";
            row += std::to_string(highSeed[team]) + ",";
            row += std::to_string(lowSeed[team]) + (i + 1 < order.size() ? "," : "");
        }

        foldyRows[u] = row;
    };

    void foldyProcessor::processData(int pos, int threadCount, unsigned long long total){}

    void foldyProcessor::displayData() {
        std::ofstream csv("foldy.csv");
        csv.write(headerRow.c_str(), headerRow.size());
        csv.write("\n",1);
        for (const auto& row : foldyRows) {
            csv.write(row.c_str(), row.size());
            csv.write("\n",1);
        }
        csv.close();
    }

    void statsProcessor::setupData(int dThreadCount, int sThreadCount, seasonData data) {
        istats = static_cast<int *>(malloc(sThreadCount * data.teamCount * sizeof(int) * 2));
        fstats = static_cast<long double *>(malloc(sThreadCount * data.teamCount * sizeof(long double) * 2));
        sdata = data;

        done = std::vector<std::atomic<bool>>(sThreadCount);

        for (int i = 0; i < sThreadCount; i++) {
            for (int j = 0; j < sdata.teamCount; j++) {
                istats[i * sdata.teamCount * 2 + j * 2] = 9;
                istats[i * sdata.teamCount * 2 + j * 2 + 1] = 0;
                fstats[i * sdata.teamCount * 2 + j * 2] = 0.0;
                fstats[i * sdata.teamCount * 2 + j * 2 + 1] = 0.0;
            }
            done[i] = false;
        }

        mins.resize(sdata.teamCount, 9);
        maxs.resize(sdata.teamCount, 0);
        seeds.resize(sdata.teamCount, 0);
        prob.resize(sdata.teamCount, 0);
        sThreads = sThreadCount;
    }

    void statsProcessor::processSeason(const std::vector<int> &order, const std::vector<int> &highSeed, const std::vector<int> &lowSeed, unsigned long long season, long long u, int threadPos) {
        #pragma omp simd
        for (int i = 0; i < sdata.teamCount; i ++) {
            const int team = order[i];
            const double seed = static_cast<float>(highSeed[team] + lowSeed[team])/2.0f;
            fstats[threadPos*sdata.teamCount*2 + 2*team] += seed;
            int a = istats[threadPos*sdata.teamCount*2 + 2*team];
            int b = highSeed[team];
            istats[threadPos*sdata.teamCount*2 + 2*team] = a < b ? a : b;
            a = istats[threadPos*sdata.teamCount*2 + 2*team + 1];
            b = lowSeed[team];
            istats[threadPos*sdata.teamCount*2 + 2*team + 1] = a < b ? b : a;
            const double range = 5 - highSeed[team] + 1;
            const double coveredRange = range > 0 ? range : 0;
            const double total = lowSeed[team] - highSeed[team] + 1;
            fstats[threadPos*sdata.teamCount*2 + 2*team + 1] += std::min(coveredRange/total, 1.0);
        }
    }

    void statsProcessor::processData(int pos, int threadCount, unsigned long long total) {
        //calculate the minimum, maximum, seed, and prob of this thread's data, then use the tree

        long double totalD = static_cast<double>(total);

        auto tMins = std::vector<int>(sdata.teamCount);
        auto tMaxs = std::vector<int>(sdata.teamCount);
        auto tSeeds = std::vector<double>(sdata.teamCount);
        auto tProbs = std::vector<double>(sdata.teamCount);
        int start = pos * sThreads / threadCount;
        int end = (pos + 1) * sThreads / threadCount;

        tMins.assign(sdata.teamCount, 9);
        tMaxs.assign(sdata.teamCount, 0);
        tSeeds.assign(sdata.teamCount, 0.0);
        tProbs.assign(sdata.teamCount, 0.0);

        for (int j = start; j < end; j++) {
            for (int i = 0; i < sdata.teamCount; i++) {
                tMins[i] = std::min(tMins[i], istats[j*sdata.teamCount*2 + i*2]);
                if (i == 9 && istats[j*sdata.teamCount*2 + i*2] < 8) {
                    std::printf("%d %d %d\n", pos, istats[j*sdata.teamCount*2 + i*2], j);
                }
                tMaxs[i] = std::max(tMaxs[i], istats[j*sdata.teamCount*2 + i*2+1]);
                tSeeds[i] += fstats[j*sdata.teamCount*2 + i*2];
                tProbs[i] += fstats[j*sdata.teamCount*2 + i*2+1];
            }
        }


        int mask = 1;

        while (mask < threadCount) {
            if ((pos & mask) == 0 && (pos ^ mask) < threadCount) {
                while (!done[pos ^ mask].load()) {
                    if (done[pos ^ mask].load()) {
                        break;
                    }
                }
                const int tIndex = ((pos^mask) * sThreads / threadCount) * sdata.teamCount*2;
                for (int i = 0; i < sdata.teamCount; i++) {
                    tMins[i] = std::min(tMins[i], istats[tIndex + i*2]);
                    tMaxs[i] = std::max(tMaxs[i], istats[tIndex + i*2 + 1]);
                    tSeeds[i] += fstats[tIndex + i*2];
                    tProbs[i] += fstats[tIndex + i*2 + 1];
                }
                mask <<= 1;
            } else {
                for (int i = 0; i < sdata.teamCount; i++) {
                    istats[start * sdata.teamCount * 2 + i*2] = tMins[i];
                    istats[start * sdata.teamCount * 2 + i*2 + 1] = tMaxs[i];
                    fstats[start * sdata.teamCount * 2 + i*2] = tSeeds[i];
                    fstats[start * sdata.teamCount * 2 + i*2 + 1] = tProbs[i];
                }
                done[pos] = true;
                break;
            }
        }

        if (pos == 0) {
            for (int i = 0; i < sdata.teamCount; i++) {
                tSeeds[i] /= totalD;
                tProbs[i] /= totalD;
            }
            mins = tMins;
            maxs = tMaxs;
            seeds = tSeeds;
            prob = tProbs;
        }
    }

    void statsProcessor::displayData() {
        for (int i = 0; i < sdata.teamCount; i++) {
            std::printf("%s:\n\tAvg. Seed: %f\n\tMin. Seed: %d\n\tMax. Seed: %d\n\tPlayoff Probability: %f\n\n",sdata.teamNames[i].c_str(), seeds[i]+1, mins[i]+1, maxs[i]+1, prob[i]);
        }
    }

    void qmProcessor::setupData(int dThreadCount, int sThreadCount, seasonData data) {
        sdata = data;
        int tc2 = sdata.teamCount * sdata.teamCount;
        tbMatches.reserve(sThreadCount);
        ntbMatches.reserve(sThreadCount);
        results.resize(2*tc2);
        done = std::vector<std::atomic<bool>>(sThreadCount);
        for (int i = 0; i < sThreadCount; i++) {
            tbMatches.emplace_back(tc2);
            ntbMatches.emplace_back(tc2);
        }

        for (int i = 0; i < sThreadCount; i++) {
            done[i] = false;
        }

        sThreads = sThreadCount;

        for (int i = 0; i < sdata.unplayedCount; i++) {
            unsigned long long matchMask = 0;
            unsigned long long y = sdata.unplayed;
            for (int j = 0; j < i+1; j++) {
                unsigned long long z = y & y-1LL;
                matchMask = y-z;
                y = z;
            }
            int x = 0;
            unsigned long long m = matchMask;
            while (m > 1) {
                x += 1;
                m = m >> 1;
            }
            int a = 0;
            int b = 0;
            while (b < sdata.teamCount && sdata.shifts[b] < x) {
                b += 1;
            }
            b -= 1;
            a = x - sdata.shifts[b];
            while (a <= b) {
                b -= 1;
                a = x-sdata.shifts[b];
            }
            posNames.push_back(std::format("{} beats {}", sdata.teamNames[b], sdata.teamNames[a]));
            negNames.push_back(std::format("{} beats {}", sdata.teamNames[a], sdata.teamNames[b]));
        }
    }

    void qmProcessor::processSeason(const std::vector<int> &order, const std::vector<int> &highSeed, const std::vector<int> &lowSeed, unsigned long long season, const long long u, int threadPos) {
        for (int i = 0; i < sdata.teamCount; i++) {
            if (const int team = order[i]; highSeed[team] == lowSeed[team]) {
                ntbMatches.at(threadPos).at(team*sdata.teamCount + highSeed[team]).push_back(u);
            } else {
                for (int seed = highSeed[team]; seed <= lowSeed[team]; seed++) {
                    tbMatches.at(threadPos).at(team*sdata.teamCount + seed).push_back(u);
                }
            }
        }
    }

    void qmProcessor::processData(int pos, int threadCount, unsigned long long total) {
        //use threads to join

        int mask = 1;
        while (mask < threadCount) {
            if ((pos&mask)==0 && (pos^mask)<threadCount) {
                while (true) {
                    if (done[pos^mask].load()) {
                        break;
                    }
                }
                for (int i = 0; i < sdata.teamCount * sdata.teamCount; i++) {
                    for (auto u : tbMatches[pos^mask][i]) {
                        tbMatches[pos][i].push_back(u);
                    }
                    for (auto u : ntbMatches[pos^mask][i]) {
                        ntbMatches[pos][i].push_back(u);
                    }
                }
                mask <<= 1;
            }else {
                done[pos] = true;
                break;
            }
        }

        if (pos == 0) {
            done[0] = true;
        }

        while (true) {
            if (done[0].load()) {
                break;
            }
        }

        const int start = pos * sdata.teamCount * sdata.teamCount / threadCount;
        const int end = (pos + 1) * sdata.teamCount * sdata.teamCount / threadCount;

        for (int i = start; i < end; i++) {
            results[i*2] = qm::getStrFromQm(sdata.unplayedCount, tbMatches.at(0).at(i), posNames, negNames);
            results[i*2 + 1] = qm::getStrFromQm(sdata.unplayedCount, ntbMatches.at(0).at(i), posNames, negNames);
        }
    }

    void qmProcessor::displayData() {
        for (int team = 0; team < sdata.teamCount; team++) {
            std::cout << sdata.teamNames[team] << ":" << std::endl;
            for (int seed = 0; seed < sdata.teamCount; seed ++) {
                if (const std::string& tbString = results[team*sdata.teamCount*2 + seed*2]; !tbString.empty()) {
                    std::cout << "\tSeed " << seed+1 << "(tb): " << tbString << std::endl;
                }
                if (const std::string& ntbString = results[team*sdata.teamCount*2 + seed*2+1]; !ntbString.empty()) {
                    std::cout << "\tSeed " << seed+1 << ": " << ntbString << std::endl;
                }
            }
            std::cout << std::endl;
        }
    }

    void narrowQmProcessor::setupData(int dThreadCount, int sThreadCount, seasonData data) {
        sdata = data;
        tbMatches.resize(dThreadCount);
        ntbMatches.resize(dThreadCount);
        results.resize(2*sdata.teamCount);
        done = std::vector<std::atomic<bool>>(dThreadCount);

        for (int j = 0; j < dThreadCount; j++) {
            tbMatches[j].resize(sdata.teamCount);
            ntbMatches[j].resize(sdata.teamCount);
        }

        for (int i = 0; i < sdata.teamCount; i++) {
            done[i] = false;
        }

        sThreads = sThreadCount;

        for (int i = 0; i < sdata.unplayedCount; i++) {
            unsigned long long matchMask = 0;
            unsigned long long y = sdata.unplayed;
            for (int j = 0; j < i+1; j++) {
                const unsigned long long z = y & y-1LL;
                matchMask = y-z;
                y = z;
            }
            int x = 0;
            unsigned long long m = matchMask;
            while (m > 1) {
                x += 1;
                m = m >> 1;
            }
            int a = 0;
            int b = 0;
            while (b < sdata.teamCount && sdata.shifts[b] < x) {
                b += 1;
            }
            b -= 1;
            a = x - sdata.shifts[b];
            while (a <= b) {
                b -= 1;
                a = x-sdata.shifts[b];
            }
            posNames.push_back(std::format("{} beats {}", sdata.teamNames[b], sdata.teamNames[a]));
            negNames.push_back(std::format("{} beats {}", sdata.teamNames[a], sdata.teamNames[b]));
        }
    }

    void narrowQmProcessor::processSeason(const std::vector<int> &order, const std::vector<int> &highSeed, const std::vector<int> &lowSeed, unsigned long long season, const long long u, int threadPos) {
        for (int i = 0; i < sdata.teamCount; i++) {
            if (const int team = order[i]; highSeed[team] >= targetMinSeed && lowSeed[team] <= targetMaxSeed) {
                ntbMatches.at(threadPos).at(team).push_back(u);
            } else if (highSeed[team] >= targetMinSeed && highSeed[team] <= targetMaxSeed || lowSeed[team] >= targetMinSeed && lowSeed[team] <= targetMaxSeed){
                tbMatches.at(threadPos).at(team).push_back(u);
            }
        }
    }

    void narrowQmProcessor::processData(int pos, int threadCount, unsigned long long total) {
        int mask = 1;
        while (mask < threadCount) {
            if ((pos&mask)==0 && (pos^mask)<threadCount) {
                while (true) {
                    if (done[pos^mask].load()) {
                        break;
                    }
                }
                for (int i = 0; i < sdata.teamCount; i++) {
                    for (auto u : tbMatches[pos^mask][i]) {
                        tbMatches[pos][i].push_back(u);
                    }
                    for (auto u : ntbMatches[pos^mask][i]) {
                        ntbMatches[pos][i].push_back(u);
                    }
                }
                mask <<= 1;
            } else {
                done[pos] = true;
                break;
            }
        }

        if (pos == 0) {
            done[0] = true;
        }

        while (true) {
            if (done[0].load()) {
                break;
            }
        }

        int start = pos * sdata.teamCount / threadCount;
        int end = (pos + 1) * sdata.teamCount / threadCount;

        for (int i = start; i < end; i++) {
            results[i*2] = qm::getStrFromQm(sdata.unplayedCount, tbMatches[0].at(i), posNames, negNames);
            results[i*2 + 1] = qm::getStrFromQm(sdata.unplayedCount, ntbMatches[0].at(i), posNames, negNames);
        }
    }

    void narrowQmProcessor::displayData() {
        std::cout << "Routes for each team to get between seed "<<targetMinSeed<<" and "<<targetMaxSeed<<std::endl;
        for (int team = 0; team < sdata.teamCount; team++) {
            std::cout << sdata.teamNames[team] << ":" << std::endl;
            if (const std::string& ntbString = results[team*2 + 1]; !ntbString.empty()) {
                std::cout << "\tSuccess: " << ntbString << std::endl;
            } else {
                std::cout << "\tNever guarantees" << std::endl;
            }
            if (const std::string& tbString = results[team*2]; !tbString.empty()) {
                std::cout << "\tSuccess(tb): " << tbString << std::endl;
            }
            std::cout << std::endl;
        }
    }

    void runPerSeason(const long long increment, const int pos, const seasonData &data, seasonProcessor *proc, std::vector<performance> *perfs) {
        std::vector<int> order(data.teamCount);
        std::vector<int> highSeed(data.teamCount);
        std::vector<int> lowSeed(data.teamCount);
        long long start = increment*pos;
        long long maxSeason = 1LL << data.unplayedCount;
        long long end = std::min(increment * (pos + 1), maxSeason);
        if (start >= maxSeason) {
            return;
        }

        for (long long u = start; u < end; u++) {
            auto start = std::chrono::high_resolution_clock::now();
            unsigned long long nSeason = getSeason(data, u);
            auto betGetCalc = std::chrono::high_resolution_clock::now();
            calculateSeason(nSeason, &order, &highSeed, &lowSeed, data);
            auto betCalcProc = std::chrono::high_resolution_clock::now();
            proc->processSeason(order, highSeed, lowSeed, nSeason, u, pos);
            auto end = std::chrono::high_resolution_clock::now();
            perfs->at(pos).getMS += (betGetCalc-start).count()/1.0e6;
            perfs->at(pos).seasonMS += (betCalcProc-betGetCalc).count()/1.0e6;
            perfs->at(pos).procMS += (end-betCalcProc).count()/1.0e6;
        }
    }

    void runPerRandomSeason(long long count, int pos, const seasonData &data, std::mt19937 rng, seasonProcessor *proc) {
        std::vector<int> order(data.teamCount);
        std::vector<int> highSeed(data.teamCount);
        std::vector<int> lowSeed(data.teamCount);

        std::uniform_int_distribution<long long> dist(0, 1LL<<data.unplayedCount);

        for (int i = 0; i < count; i++) {
            long long u = dist(rng);
            unsigned long long nSeason = getSeason(data, u);
            calculateSeason(nSeason, &order, &highSeed, &lowSeed, data);

            proc->processSeason(order, highSeed, lowSeed, nSeason, u, pos);
        }
    }
} // ts