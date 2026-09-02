#include <bitset>
#include <chrono>
#include <cstring>
#include <format>
#include <iostream>
#include <fstream>
#include <random>
#include <thread>
#include <vector>

#include "qm.h"

static int getTeamCount(const char* filename) {
    std::ifstream csv(filename);

    std::string line;

    std::getline(csv, line);
    int teamCount = 0;
    for (int i = 0; i < line.length(); i++) {
        if (line[i] == ',') {
            teamCount += 1;
        }
    }

    csv.close();

    return teamCount;
}

static void readCSV(const char* filename, std::vector<std::string> *teams, long long *season, long long *unplayed, int *unplayedCount) {
    unplayedCount[0] = 0;
    std::ifstream csv(filename);

    std::string line;

    std::getline(csv, line);

    int ind = -1;
    std::string team;
    for (int i = 0; i < line.length(); i++) {
        if (line[i] == ',' || line[i] == '\n') {
            if (ind != -1) {
                teams->at(ind) = team;
            }
            team = "";
            ind += 1;
        } else {
            team += line[i];
        }
    }
    teams->at(ind) = team;

    season[0] = 0;
    unplayed[0] = 0;
    ind = 0;

    int row = 0;
    while (std::getline(csv, line)) {
        int commaCount = 0;
        for (int i = 0; i < line.length(); i++) {
            char chr = line[i];
            if (chr == ',' && commaCount <= row+1) {
                commaCount += 1;
            } else if (commaCount > row+1) {
                if (chr == '0') {
                    i += 1;
                    ind += 1;
                } else if (chr == '1') {
                    i += 1;
                    season[0] += (1LL << ind);
                    ind += 1;
                } else {
                    unplayed[0] += (1LL << ind);
                    unplayedCount[0] += 1;
                    ind += 1;
                }
            }
        }
        if (line[line.length()-1] == ',') {
            unplayed[0] += (1LL << ind);
            unplayedCount[0] += 1;
            ind += 1;
        }
        row += 1;
    }

    csv.close();
}

static long long getSeason(long long season, long long unplayed, long long index, int unplayedCount) {
    long long x = index;
    long long y = unplayed;
    long long mask = 0;
    while (x > 0) {
        int res = static_cast<int>(x & 1LL); // if rightmost bit is 1, result = 1
        long long z = y & (y-1LL); // z with the rightmost 1 flipped to 0
        long long maskAdd = res == 0 ? 0LL : y-z; //
        mask += maskAdd;
        y = z;
        x = x>>1;
    }

    return season | mask;
}

static void calculateSeason(long long season, std::vector<int> *order, std::vector<int> *highSeed, std::vector<int> *lowSeed, const int teamCount, const long long *t1Masks, const long long *t2Masks, const int *shifts) {
    std::vector<long long> results(teamCount);
    std::vector<int> wins(teamCount);
    std::vector<int> tiebreakers(teamCount);
    long long invSeason = ~season;
    for (int i = 0; i < teamCount; i ++) {
        results[i] = (invSeason & t1Masks[i]) | (season & t2Masks[i]);
        wins[i] = 0;
        tiebreakers[i] = 0;
        order->at(i) = i;
        long long x = results[i];
        while (x > 0) {
            x &= x-1;
            wins[i] += 1;
        }
        for (int team = 0; team < i; team ++) {
            if (wins[team] != wins[i]) {
                continue;
            }
            if ((results[i] & (1LL << (i + shifts[team]))) == 0) {
                tiebreakers[team] += 1;
            } else {
                tiebreakers[i] += 1;
            }
        }
    }

    for (int i = 1; i < teamCount; i ++) {
        int j = i-1;
        int value = order->at(i);
        while (j >= 0 && (wins[order->at(j)] < wins[value] || (wins[order->at(j)] == wins[value] && tiebreakers[order->at(j)] < tiebreakers[value]))) {
            order->at(j+1) = order->at(j);
            j = j - 1;
        }
        order->at(j+1) = value;
    }

    for (int i = 0; i < teamCount; i += 1) {
        int newMin = i;
        int newMax = i;
        for (int j = i-1; j >= 0; j --) {
            if (wins[order->at(i)] == wins[order->at(j)] and tiebreakers[order->at(i)] == tiebreakers[order->at(j)]) {
                newMin = j;
            }else {
                break;
            }
        }
        for (int j = i+1; j < teamCount; j ++) {
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

static void calculateRangeStats(long long increment, int pos, long long season, long long unplayed, const int teamCount, int unplayedCount, const long long *t1Masks, const long long *t2Masks, const int *shifts, int *istats, long double *fstats) {
    std::vector<int> order(teamCount);
    std::vector<int> highSeed(teamCount);
    std::vector<int> lowSeed(teamCount);
    long long start = increment * pos;
    long long maxSeason = (1LL << unplayedCount);
    long long end = std::min(increment * (pos + 1), maxSeason);
    if (start >= maxSeason) {
        return;
    }
    for (long long u = start; u < end; u++) {
        long long nSeason = getSeason(season, unplayed, u, unplayedCount);
        calculateSeason(nSeason, &order, &highSeed, &lowSeed, teamCount, t1Masks, t2Masks, shifts);

        for (int i = 0; i < teamCount; i ++) {
            int team = order[i];
            double seed = static_cast<float>(highSeed[team] + lowSeed[team])/2.0f;
            fstats[pos*teamCount*2 + 2*team] += seed;
            istats[pos*teamCount*2 + 2*team] = std::min(istats[pos*teamCount*2 + 2*team], highSeed[team]);
            istats[pos*teamCount*2 + 2*team + 1] = std::max(istats[pos*teamCount*2 + 2*team + 1], lowSeed[team]);
            double coveredRange = std::max(5 - highSeed[team] + 1, 0);
            double total = lowSeed[team] - highSeed[team] + 1;
            fstats[pos*teamCount*2 + 2*team + 1] += std::min(coveredRange/total, 1.0);
        }
    }
}

static void calculateRangeQm(const long long increment, const int pos, const long long season, const long long unplayed, const int teamCount, const int unplayedCount, const long long *t1Masks, const long long *t2Masks, const int *shifts, std::vector<std::vector<std::bitset<256>>> *tbmatches, std::vector<std::vector<std::bitset<256>>> *ntbmatches) {
    std::vector<int> order(teamCount);
    std::vector<int> highSeed(teamCount);
    std::vector<int> lowSeed(teamCount);
    long long start = increment * pos;
    long long maxSeason = (1LL << unplayedCount);
    long long end = std::min(increment * (pos + 1), maxSeason);
    if (start >= maxSeason) {
        return;
    }
    for (long long u = start; u < end; u++) {
        long long nSeason = getSeason(season, unplayed, u, unplayedCount);
        calculateSeason(nSeason, &order, &highSeed, &lowSeed, teamCount, t1Masks, t2Masks, shifts);

        for (int i = 0; i < teamCount; i ++) {
            int team = order[i];
            if (highSeed[team] == lowSeed[team]) {
                const uint64_t idx = qm::toTernary(u, unplayedCount);
                ntbmatches[0][team*teamCount + highSeed[team]][idx/243][idx%243] = true;
            }else {
                for (int seed = highSeed[team]; seed <= lowSeed[team]; seed ++) {
                    const uint64_t idx = qm::toTernary(u, unplayedCount);
                    tbmatches[0][team*teamCount + seed][idx/243][idx%243] = true;
                }
            }
        }
    }
}

static void processQmResults(int increment, int pos, const std::vector<std::vector<std::bitset<256>>>& tbMatches, const std::vector<std::vector<std::bitset<256>>> &ntbMatches, std::vector<std::string> *results, int teamCount, int n, const std::vector<std::string>& posNames, const std::vector<std::string>& negNames) {
    int start = pos*increment;
    int end = std::min((pos+1)*increment, teamCount*teamCount);
    if (start >= teamCount*teamCount) {
        return;
    }
    for (int item = start; item < end; item++) {
        int seed = item%teamCount;
        int team = (item - seed)/teamCount;
        results->at(team*teamCount*2 + seed*2) = qm::getStrFromQm(n, tbMatches[team*teamCount + seed], posNames, negNames);
        results->at(team*teamCount*2 + seed*2+1) = qm::getStrFromQm(n, ntbMatches[team*teamCount + seed], posNames, negNames);
    }
}

static double msDiff(std::chrono::time_point<std::chrono::high_resolution_clock> a, std::chrono::time_point<std::chrono::high_resolution_clock> b) {
    return static_cast<double>((a - b).count())/1.0e6;
}

int main(int argc, char *argv[]) {
    auto start = std::chrono::high_resolution_clock::now();
    char* filename = argv[argc-1];
    bool CUDA = false;
    long long n = 0;
    int threads = static_cast<int>(std::thread::hardware_concurrency());
    bool FOLDY = false;
    int benchmarkTC = 0;
    int benchmarkUM = 0;
    bool QM = false;
    if (argc == 1) {
        printf("Usage:\n"
               "\tplayoffpredictor [options] <filename>\n"
               "\n"
               "Options:\n"
               "\t--cuda\t\t\t\tEnables CUDA mode to accelerate performance.\n"
               "\t-t <count>\t\t\tSets the amount of threads to use. If CUDA is enabled, each thread is a block of 256.\n"
               "\t--foldy\t\t\t\tEnables Foldy mode, generating a foldy sheet as a .csv file.\n"
               "\t-n <count>\t\t\tInstead of running every season, instead runs n random seasons.\n"
               "\t--qm\t\t\t\tUses the Quine-McCluskey algorithm to calculate each team's route to each seed.\n"
               "\t-bm <teamCount> <matchCount>\tWill remove the need for a file and instead generate a random season with <teamCount> teams and <matchCount> matches remaining.");
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        if (i >= argc - 1 && benchmarkTC == 0) {
            break;
        }
        if (strcmp(argv[i], "--cuda") == 0) {
            CUDA = true;
        } else if (strcmp(argv[i], "--foldy") == 0) {
            FOLDY = true;
        } else if (strcmp(argv[i], "--qm") == 0) {
            QM = true;
        } else if (strcmp(argv[i], "-t") == 0) {
            threads = std::stoi(argv[i+1]);
            i+=1;
        } else if (strcmp(argv[i], "-n") == 0) {
            n = std::stoll(argv[i+1]);
        } else if (strcmp(argv[i], "-bm") == 0){
            benchmarkTC = std::stoi(argv[i+1]);
            benchmarkUM = std::stoi(argv[i+2]);
            i+=2;
        } else {
            throw std::invalid_argument(std::format("Unsupported argument: {}", argv[i]));
        }
    }
    auto processedArgs = std::chrono::high_resolution_clock::now();
    const int teamCount = benchmarkTC == 0 ? getTeamCount(filename) : benchmarkTC;
    std::vector<std::string> teams(teamCount);
    auto *season = static_cast<long long *>(malloc(sizeof(long long)));
    auto *unplayed = static_cast<long long *>(malloc(sizeof(long long)));
    int *unplayedCount = static_cast<int *>(malloc(sizeof(int)));

    int *shifts = static_cast<int *>(malloc(teamCount * sizeof(int)));
    auto *t1Masks = static_cast<long long *>(malloc(teamCount * sizeof(long long)));
    auto *t2Masks = static_cast<long long *>(malloc(teamCount * sizeof(long long)));
    int ind = 0;
    for (int i = 0; i < teamCount; i++) {
        t1Masks[i] = 0LL;
        t2Masks[i] = 0LL;
        shifts[i] = static_cast<int>(-0.5 * i * i + (teamCount - 1.5) * i - 1);
    }

    for (int i = 0; i < teamCount; i++) {
        for (int j = i+1; j < teamCount; j++) {
            t2Masks[i] += 1LL << ind;
            t1Masks[j] += 1LL << ind;
            ind += 1;
        }
    }

    if (benchmarkTC == 0) {
        readCSV(filename, &teams, season, unplayed, unplayedCount);
    } else {
        std::random_device device;
        std::size_t seed;
        if (device.entropy()) {
            seed = device();
        } else {
            seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        }
        std::mt19937 gen(seed);
        std::uniform_int_distribution<int> rng(0, 1);

        int totalMatches = teamCount * (teamCount - 1) / 2;
        for (int i = 0; i < teamCount; i+=1) {
            teams[i] = std::format("Team {}", i);
        }

        unplayedCount[0] = benchmarkUM;
        season[0] = 0;
        unplayed[0] = 0;
        std::vector<int> loop;
        loop.assign(teams.size(), 0);
        for (int i = 0; i < teamCount; i++) {
            loop[i] = i;
        }
        for (int i = 0; i < totalMatches - benchmarkUM; i++) {
            if (i%(teamCount/2) == 0 && i != 0) {
                int temp = loop[teamCount-1];
                for (int j = teamCount-1; j >= 2; j--) {
                    loop[j] = loop[j-1];
                }
                loop[1] = temp;
            }

            long long result = rng(gen);
            int t1Ind = i%(teamCount/2);
            int t2Ind = teamCount - i%(teamCount/2) - 1;

            int a = std::min(loop[t1Ind], loop[t2Ind]);
            int b = std::max(loop[t1Ind], loop[t2Ind]);

            int shift = b + shifts[a];

            season[0] |= result << shift;
        }
        for (int i = totalMatches - benchmarkUM; i < totalMatches; i++) {
            if (i%(teamCount/2) == 0 && i != 0) {
                int temp = loop[teamCount-1];
                for (int j = teamCount-1; j >= 2; j--) {
                    loop[j] = loop[j-1];
                }
                loop[1] = temp;
            }

            int t1Ind = i%(teamCount/2);
            int t2Ind = teamCount - i%(teamCount/2) - 1;

            int a = std::min(loop[t1Ind], loop[t2Ind]);
            int b = std::max(loop[t1Ind], loop[t2Ind]);

            int shift = b + shifts[a];

            unplayed[0] |= 1ll << shift;
        }
    }
    auto readFile = std::chrono::high_resolution_clock::now();

    long long total = 1LL << unplayedCount[0];
    long long increment = total/threads;
    if (total % threads != 0) {
        increment += 1;
    }

    std::vector<std::thread> threadsVec;
    threadsVec.reserve(threads);

    int *istats;
    long double *fstats;
    auto tbmatches = std::vector<std::vector<std::bitset<256>>>();
    auto ntbmatches = std::vector<std::vector<std::bitset<256>>>();
    if (!QM) {
        istats = static_cast<int *>(malloc(threads * teamCount * sizeof(int) * 2));
        fstats = static_cast<long double *>(malloc(threads * teamCount * sizeof(long double) * 2));
    } else {
        uint64_t blockCount = unplayedCount[0] > 5 ? qm::pow3(unplayedCount[0] - 5) : 1;
        for (int i = 0; i < teamCount * teamCount; i++) {
            tbmatches.emplace_back();
            ntbmatches.emplace_back();
            for (int j = 0; j < blockCount; j++) {
                tbmatches.at(i).emplace_back(0);
                ntbmatches.at(i).emplace_back(0);
            }
        }
    }

    auto setupData = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < threads; i++) {
        if (!QM) {
            for (int j = 0; j < teamCount; j++) {
                istats[i * teamCount * 2 + j * 2] = 9;
                istats[i * teamCount * 2 + j * 2 + 1] = 0;
                fstats[i * teamCount * 2 + j * 2] = 0.0;
                fstats[i * teamCount * 2 + j * 2 + 1] = 0.0;
            }
            threadsVec.emplace_back(calculateRangeStats, increment, i, season[0], unplayed[0], teamCount, unplayedCount[0], t1Masks, t2Masks, shifts, istats, fstats);
        }else {
            threadsVec.emplace_back(calculateRangeQm, increment, i, season[0], unplayed[0], teamCount, unplayedCount[0], t1Masks, t2Masks, shifts, &tbmatches, &ntbmatches);
        }
    }

    auto threadSetup = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < threads; i++) {
        threadsVec.at(i).join();
    }

    auto ranThreads = std::chrono::high_resolution_clock::now();

    std::cout << "Thread count: " << threads << ", Increment: " << increment << ", Unplayed Count: " << unplayedCount[0] << std::endl << std::endl;

    if (!QM) {
        std::vector<int> mins(teamCount);
        std::vector<int> maxs(teamCount);
        std::vector<double> seeds(teamCount);
        std::vector<double> prob(teamCount);

        for (int i = 0; i < teamCount; i++) {
            mins[i] = 9;
            maxs[i] = 0;
            seeds[i] = 0;
            prob[i] = 0;

            for (int j = 0; j < threads; j++) {
                mins[i] = std::min(mins[i], istats[j*teamCount*2 + i*2]);
                maxs[i] = std::max(maxs[i], istats[j*teamCount*2 + i*2 + 1]);
                seeds[i] += fstats[j*teamCount*2 + i*2];
                prob[i] += fstats[j*teamCount*2 + i*2 + 1];
            }

            seeds[i] /= static_cast<double>(total);
            prob[i] /= static_cast<double>(total);
        }


        for (int i = 0; i < teamCount; i++) {
            std::printf("%s:\n\tAvg. Seed: %f\n\tMin. Seed: %d\n\tMax. Seed: %d\n\tPlayoff Probability: %f\n\n",teams[i].c_str(), seeds[i]+1, mins[i]+1, maxs[i]+1, prob[i]);
        }
    } else {
        std::vector<std::string> posNames = std::vector<std::string>();
        std::vector<std::string> negNames = std::vector<std::string>();
        posNames.assign(unplayedCount[0], "?????????????????");
        negNames.assign(unplayedCount[0], "?????????????????");

        for (int i = 0; i < unplayedCount[0]; i++) {
            long long matchMask = 0;
            long long y = unplayed[0];
            for (int j = 0; j < i+1; j++) {
                long long z = y & y-1LL;
                matchMask = y-z;
                y = z;
            }
            int x = 0;
            long long m = matchMask;
            while (m > 1) {
                x += 1;
                m = m >> 1;
            }
            int a = 0;
            int b = 0;
            while (b < teamCount && shifts[b] < x) {
                b += 1;
            }
            b -= 1;
            a = x - shifts[b];
            while (a <= b) {
                b -= 1;
                a = x-shifts[b];
            }
            posNames.at(i) = (std::format("{} beats {}", teams[b], teams[a]));
            negNames.at(i) = (std::format("{} beats {}", teams[a], teams[b]));
        }

        auto qmResults = std::vector<std::string>();
        for (int i = 0; i < teamCount; i++) {
            for (int j = 0; j < teamCount; j++) {
                qmResults.emplace_back();
                qmResults.emplace_back();
            }
        }

        threadsVec.clear();
        int inc = teamCount*teamCount/threads;
        if (teamCount*teamCount%threads != 0) {
            inc += 1;
        }

        for (int i = 0; i < threads; i++) {
            threadsVec.emplace_back(processQmResults, inc, i, tbmatches, ntbmatches, &qmResults,teamCount, unplayedCount[0], posNames, negNames);
        }

        for (int i = 0; i < threads; i++) {
            threadsVec.at(i).join();
        }

        if (benchmarkTC == 0) {
            for (int team = 0; team < teamCount; team++) {
                std::cout << teams[team] << ":" << std::endl;
                for (int seed = 0; seed < teamCount; seed ++) {
                    const std::string& tbString = qmResults[team*teamCount*2 + seed*2];
                    if (!tbString.empty()) {
                        std::cout << "\tSeed " << seed+1 << "(tb): " << tbString << std::endl;
                    }
                    const std::string& ntbString = qmResults[team*teamCount*2 + seed*2+1];
                    if (!ntbString.empty()) {
                        std::cout << "\tSeed " << seed+1 << ": " << ntbString << std::endl;
                    }
                }
                std::cout << std::endl;
            }
        }
    }

    auto processedData = std::chrono::high_resolution_clock::now();

    auto totalTime = msDiff(processedData, start);
    auto argTime = msDiff(processedArgs, start);
    auto fileTime = msDiff(readFile, processedArgs);
    auto setupTime = msDiff(setupData, readFile);
    auto threadSetupTime = msDiff(threadSetup, setupData);
    auto threadTime = msDiff(ranThreads, threadSetup);
    auto processTime = msDiff(processedData, ranThreads);
    std::printf("Total time: %f\n\tArguments: %f\n\tFile: %f\n\tSetup Data: %f\n\tSetup Threads: %f\n\tThreads: %f\n\tProcessing Data: %f\n", totalTime, argTime, fileTime, setupTime, threadSetupTime, threadTime, processTime);
    return 0;
}