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

static std::vector<std::vector<std::string>> readCSV(const char* filename) {
    std::vector<std::vector<std::string>> output;
    std::ifstream csv(filename);

    if (!csv.is_open()) {
        throw std::invalid_argument(filename);
    }

    std::string line;
    int lineNo = 0;
    while (std::getline(csv, line)) {
        output.emplace_back();
        std::string item;
        for (const char chr : line) {
            if (chr == ',') {
                output[lineNo].push_back(item);
                item = "";
            }else {
                item += chr;
            }
        }
        output[lineNo].push_back(item);
        lineNo += 1;
    }

    return output;
}

static ts::seasonData getSeasonData(const std::vector<std::vector<std::string>> &csv) {
    int teamCount = (csv.size() & INT_MAX) - 1;
    int unplayedCount = 0;
    unsigned long long unplayed = 0;
    unsigned long long season = 0;
    std::vector<std::string> teamNames;
    teamNames.reserve(teamCount);

    for (int i = 1; i < csv[0].size(); i++) {
        teamNames.push_back(csv[0][i]);
    }

    int ind = 0;
    for (int i = 1; i < csv.size(); i++) {
        for (int j = i + 1; j < csv[i].size(); j++) {
            std::string result = csv[i][j];
            if (result.empty()) {
                unplayedCount ++;
                unplayed |= 1ULL << ind;
            } else if (result == "1") {
                season |= 1ULL << ind;
            }
            ind ++;
        }
    }
    ind = 0;

    return ts::seasonData{.teamCount = teamCount, .unplayedCount = unplayedCount, .season = season, .unplayed = unplayed, .teamNames = teamNames};
}

static void getPrecalcMasks(ts::seasonData *data) {
    data->shifts = static_cast<int *>(malloc(data->teamCount * sizeof(int)));
    data->t1Masks = static_cast<long long *>(malloc(data->teamCount * sizeof(long long)));
    data->t2Masks = static_cast<long long *>(malloc(data->teamCount * sizeof(long long)));
    int ind = 0;
    for (int i = 0; i < data->teamCount; i++) {
        data->t1Masks[i] = 0LL;
        data->t2Masks[i] = 0LL;
        data->shifts[i] = static_cast<int>(-0.5 * i * i + (data->teamCount - 1.5) * i - 1);
    }

    for (int i = 0; i < data->teamCount; i++) {
        for (int j = i+1; j < data->teamCount; j++) {
            data->t2Masks[i] += 1LL << ind;
            data->t1Masks[j] += 1LL << ind;
            ind += 1;
        }
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
    int minQM = -1;
    int maxQM = -1;
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
               "\t--qm <maxSeed>\t\t\tUses the Quine-McCluskey algorithm to calculate each team's route to guarantee at least seed <maxSeed>\n"
               "\t--qm <minSeed> <maxSeed>"
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
            int a = -1;
            int b = -1;
            try {
                a = std::stoi(argv[i+1]);
                try {
                    b = std::stoi(argv[i+2]);
                } catch (std::invalid_argument&) {}
            } catch (std::invalid_argument&) {}

            if (a != -1 && b != -1) {
                minQM = a - 1;
                maxQM = b - 1;
                i+=2;
            } else if (a != -1) {
                i+=1;
                maxQM = a - 1;
            }
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
    ts::seasonData data{.teamCount = 0, .unplayedCount = 0, .season = 0, .unplayed = 0, .t1Masks = {}, .t2Masks = {}, .shifts = {}};


    std::size_t seed;
    if (std::random_device device; device.entropy() != 0) {
        seed = device();
    } else {
        seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    }
    std::mt19937 gen(seed);

    if (benchmarkTC == 0) {
        std::vector<std::vector<std::string>> csv = readCSV(filename);
        data = getSeasonData(csv);
        getPrecalcMasks(&data);
    } else {
        std::uniform_int_distribution rng(0, 1);

        data.teamCount = benchmarkTC;

        int totalMatches = data.teamCount * (data.teamCount - 1) / 2;
        for (int i = 0; i < data.teamCount; i+=1) {
            data.teamNames.push_back(std::format("T{}", i));
        }

        data.unplayedCount = benchmarkUM;
        data.season = 0;
        data.unplayed = 0;

        getPrecalcMasks(&data);

        std::vector<int> loop;
        loop.assign(data.teamCount, 0);
        for (int i = 0; i < data.teamCount; i++) {
            loop[i] = i;
        }
        for (int i = 0; i < totalMatches - benchmarkUM; i++) {
            if (i%(data.teamCount/2) == 0 && i != 0) {
                int temp = loop[data.teamCount-1];
                for (int j = data.teamCount-1; j >= 2; j--) {
                    loop[j] = loop[j-1];
                }
                loop[1] = temp;
            }

            long long result = rng(gen);
            int t1Ind = i%(data.teamCount/2);
            int t2Ind = data.teamCount - i%(data.teamCount/2) - 1;

            int a = std::min(loop[t1Ind], loop[t2Ind]);
            int b = std::max(loop[t1Ind], loop[t2Ind]);

            int shift = b + data.shifts[a];

            data.season |= result << shift;
        }
        for (int i = totalMatches - benchmarkUM; i < totalMatches; i++) {
            if (i%(data.teamCount/2) == 0 && i != 0) {
                int temp = loop[data.teamCount-1];
                for (int j = data.teamCount-1; j >= 2; j--) {
                    loop[j] = loop[j-1];
                }
                loop[1] = temp;
            }

            int t1Ind = i%(data.teamCount/2);
            int t2Ind = data.teamCount - i%(data.teamCount/2) - 1;

            int a = std::min(loop[t1Ind], loop[t2Ind]);
            int b = std::max(loop[t1Ind], loop[t2Ind]);

            int shift = b + data.shifts[a];

            data.unplayed |= 1ll << shift;
        }
    }

    auto readFile = std::chrono::high_resolution_clock::now();

    long long total = 1LL << data.unplayedCount;
    long long increment = total/threads;
    if (total % threads != 0) {
        increment += 1;
    }

    std::vector<std::thread> threadsVec;
    threadsVec.reserve(threads);

    ts::seasonProcessor* proc;
    if (QM) {
        if (maxQM != -1) {
            auto qmProc = ts::narrowQmProcessor();
            qmProc.targetMaxSeed = maxQM;
            qmProc.targetMinSeed = std::max(0, minQM);
            proc = &qmProc;
        }else {
            proc = new ts::qmProcessor();
        }
    } else if (FOLDY){
        proc = new ts::foldyProcessor();
    } else {
        proc = new ts::statsProcessor();
    }
    proc->setupData(threads, data);

    auto setupData = std::chrono::high_resolution_clock::now();

    std::vector<ts::performance> performances;
    performances.resize(threads);

    for (int i = 0; i < threads; i++) {
        if (n == 0) {
            threadsVec.emplace_back(ts::runPerSeason, increment, i, data, proc, &performances);
        } else {
            threadsVec.emplace_back(ts::runPerRandomSeason, n/threads, i, data, gen, proc);
        }
    }

    auto threadSetup = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < threads; i++) {
        threadsVec.at(i).join();
    }

    auto ranThreads = std::chrono::high_resolution_clock::now();

    std::cout << "Thread count: " << threads << ", Increment: " << (n == 0 ? increment : n / threads) << ", Unplayed Count: " << data.unplayedCount << std::endl << std::endl;

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
    std::printf("Total time: %f\n\tArguments: %f\n\tFile: %f\n\tSetup Data: %f\n\tSetup Threads: %f\n\tThreads: %f\n\tProcessing Data: %f\n\n", totalTime, argTime, fileTime, setupTime, threadSetupTime, threadTime, processTime);

    ts::performance totalPerf;

    for (auto [getMS, seasonMS, procMS] : performances) {
        totalPerf.getMS += getMS;
        totalPerf.procMS += procMS;
        totalPerf.seasonMS += seasonMS;
    }

    double totalThreadTime = totalPerf.getMS + totalPerf.procMS + totalPerf.seasonMS;

    std::printf("Total thread time: %.2f\n\tGet Season: %.2f(%.2f%%)\n\tCalc Season: %.2f(%.2f%%)\n\tProcess Season: %.2f(%.2f%%)\n\n", totalPerf.getMS + totalPerf.procMS + totalPerf.seasonMS, totalPerf.getMS, 100*totalPerf.getMS/(totalPerf.getMS + totalPerf.procMS + totalPerf.seasonMS), totalPerf.seasonMS, 100*totalPerf.seasonMS/(totalPerf.getMS + totalPerf.procMS + totalPerf.seasonMS), totalPerf.procMS, 100*totalPerf.procMS/(totalPerf.getMS + totalPerf.procMS + totalPerf.seasonMS));

    std::printf("Time per thread:\n");
    for (int i = 0; i < performances.size(); i++) {
        auto [getMS, seasonMS, procMS] = performances[i];
        std::printf("\tThread %2d: %.2f(%.2f%%) - (%.1f, %.1f, %.1f)\n", i, getMS + procMS + seasonMS, (getMS + procMS + seasonMS)/totalThreadTime * 100, getMS, seasonMS, procMS);
    }

    return 0;
}

//Still to do:
/*
 * CUDA mode
 * Performance testing vs compressed unplayed
 * Granular performance testing
 * Any way to reduce QM RAM usage
 * Multithread the QM algo itself instead of just running multiple QMs in parallel
 */