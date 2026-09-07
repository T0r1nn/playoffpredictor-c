#include <bitset>
#include <chrono>
#include <cstring>
#include <format>
#include <iostream>
#include <fstream>
#include <functional>
#include <random>
#include <thread>
#include <vector>

#include "ts.h"

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

    if (!csv.is_open()) {
        throw std::invalid_argument(filename);
    }

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
            n = std::stoi(argv[i+1]);
            i+=1;
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

    std::size_t seed;
    if (std::random_device device; device.entropy() != 0) {
        seed = device();
    } else {
        seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    }
    std::mt19937 gen(seed);

    if (benchmarkTC == 0) {
        readCSV(filename, &teams, season, unplayed, unplayedCount);
    } else {
        std::uniform_int_distribution rng(0, 1);

        int totalMatches = teamCount * (teamCount - 1) / 2;
        for (int i = 0; i < teamCount; i+=1) {
            teams[i] = std::format("T{}", i);
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

    auto data = ts::seasonData(teamCount, unplayedCount[0], season[0], unplayed[0], t1Masks, t2Masks, shifts, teams);

    auto readFile = std::chrono::high_resolution_clock::now();

    long long total = 1LL << unplayedCount[0];
    long long increment = total/threads;
    if (total % threads != 0) {
        increment += 1;
    }

    std::vector<std::thread> threadsVec;
    threadsVec.reserve(threads);

    ts::seasonProcessor* proc;
    if (QM) {
        proc = new ts::qmProcessor();
    } else if (FOLDY){
        proc = new ts::foldyProcessor();
    } else {
        proc = new ts::statsProcessor();
    }
    proc->setupData(threads, data);

    auto setupData = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < threads; i++) {
        if (n == 0) {
            threadsVec.emplace_back(ts::runPerSeason, increment, i, data, proc);
        } else {
            threadsVec.emplace_back(ts::runPerRandomSeason, n/threads, i, data, gen, proc);
        }
    }

    auto threadSetup = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < threads; i++) {
        threadsVec.at(i).join();
    }

    auto ranThreads = std::chrono::high_resolution_clock::now();

    std::cout << "Thread count: " << threads << ", Increment: " << (n == 0 ? increment : n / threads) << ", Unplayed Count: " << unplayedCount[0] << std::endl << std::endl;

    threadsVec.clear();

    for (int i = 0; i < threads; i++) {
        threadsVec.push_back(proc->spawnDataThread(i, threads, n==0 ? total : n));
    }

    for (int i = 0; i < threads; i++) {
        threadsVec.at(i).join();
    }

    if (benchmarkTC == 0) {
        proc->displayData();
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

//Still to do:
/*
 * CUDA mode
 * Small-scope QM(for specific seed ranges)
 * Performance testing vs compressed unplayed
 * Granular performance testing
 * Any way to reduce QM RAM usage
 * Multithread the QM algo itself instead of just running multiple QMs in parallel
 */