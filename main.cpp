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

struct seasonData {
    int teamCount = 0;
    int unplayedCount = 0;
    unsigned long long season = 0;
    unsigned long long unplayed = 0;
    long long *t1Masks{};
    long long *t2Masks{};
    int *shifts{};
    std::vector<std::string> teamNames;
};

static unsigned long long getSeason(const seasonData &data, unsigned long long index) {
    unsigned long long x = index;
    unsigned long long y = data.unplayed;
    unsigned long long mask = 0;
    while (x > 0) {
        const int res = static_cast<int>(x & 1LL); // if rightmost bit is 1, result = 1
        const unsigned long long z = y & (y-1LL); // z with the rightmost 1 flipped to 0
        const unsigned long long maskAdd = res == 0 ? 0LL : y-z; //
        mask += maskAdd;
        y = z;
        x = x>>1;
    }

    return data.season | mask;
}

static void calculateSeason(unsigned long long season, std::vector<int> *order, std::vector<int> *highSeed, std::vector<int> *lowSeed, const seasonData &data) {
    std::vector<unsigned long long> results(data.teamCount);
    std::vector<int> wins(data.teamCount);
    std::vector<int> tiebreakers(data.teamCount);
    unsigned long long invSeason = ~season;
    for (int i = 0; i < data.teamCount; i ++) {
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
        int value = order->at(i);
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

class seasonProcessor {
public:
    virtual ~seasonProcessor() = default;

    virtual void setupData(int threadCount, seasonData data) = 0;

    virtual void processSeason(const std::vector<int> &order, const std::vector<int> &highSeed, const std::vector<int> &lowSeed, unsigned long long season, long long u, int threadPos) = 0;

    virtual void processData(int pos, int threadCount, unsigned long long total) = 0;

    virtual void displayData() = 0;

    virtual std::thread spawnDataThread(int pos, int threadCount, unsigned long long total) {
        return std::thread(&seasonProcessor::processData, this, pos, threadCount, total);
    }
};

class foldyProcessor : public seasonProcessor {
public:
    seasonData sdata;
    std::vector<std::string> foldyRows;
    std::vector<std::string> zeroNames;
    std::vector<std::string> oneNames;
    std::string headerRow;

    void setupData(int threadCount, seasonData data) override {
        foldyRows.resize(1ULL << data.unplayedCount, "");
        zeroNames.resize(data.unplayedCount, "");
        oneNames.resize(data.unplayedCount, "");
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
            oneNames[i] = sdata.teamNames[b];
            zeroNames[i] = sdata.teamNames[a];

            headerRow += oneNames[i] + " vs " + zeroNames[i] + ",";
        }

        for (int i = 0; i < sdata.teamCount; i++) {
            headerRow += std::format("Team {}, Best Seed, Worst Seed", i) + (i + 1 < sdata.teamCount ? "," : "");
        }
    }

    void processSeason(const std::vector<int> &order, const std::vector<int> &highSeed, const std::vector<int> &lowSeed, unsigned long long season, long long u, int threadPos) override {
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

    void processData(int pos, int threadCount, unsigned long long total) override{}

    void displayData() override {
        std::ofstream csv("foldy.csv");
        csv.write(headerRow.c_str(), headerRow.size());
        csv.write("\n",1);
        for (const auto& row : foldyRows) {
            csv.write(row.c_str(), row.size());
            csv.write("\n",1);
        }
        csv.close();
    }
};

class statsProcessor : public seasonProcessor {
public:
    int *istats{};
    long double *fstats{};
    seasonData sdata;
    std::vector<int> mins;
    std::vector<int> maxs;
    std::vector<double> seeds;
    std::vector<double> prob;

    void setupData(int threadCount, seasonData data) override {
        istats = static_cast<int *>(malloc(threadCount * data.teamCount * sizeof(int) * 2));
        fstats = static_cast<long double *>(malloc(threadCount * data.teamCount * sizeof(long double) * 2));
        sdata = data;

        for (int i = 0; i < threadCount; i++) {
            for (int j = 0; j < sdata.teamCount; j++) {
                istats[i * sdata.teamCount * 2 + j * 2] = 9;
                istats[i * sdata.teamCount * 2 + j * 2 + 1] = 0;
                fstats[i * sdata.teamCount * 2 + j * 2] = 0.0;
                fstats[i * sdata.teamCount * 2 + j * 2 + 1] = 0.0;
            }
        }

        mins.resize(sdata.teamCount, 9);
        maxs.resize(sdata.teamCount, 0);
        seeds.resize(sdata.teamCount, 0);
        prob.resize(sdata.teamCount, 0);
    }

    void processSeason(const std::vector<int> &order, const std::vector<int> &highSeed, const std::vector<int> &lowSeed, unsigned long long season, long long u, int threadPos) override {
        for (int i = 0; i < sdata.teamCount; i ++) {
            const int team = order[i];
            const double seed = static_cast<float>(highSeed[team] + lowSeed[team])/2.0f;
            fstats[threadPos*sdata.teamCount*2 + 2*team] += seed;
            istats[threadPos*sdata.teamCount*2 + 2*team] = std::min(istats[threadPos*sdata.teamCount*2 + 2*team], highSeed[team]);
            istats[threadPos*sdata.teamCount*2 + 2*team + 1] = std::max(istats[threadPos*sdata.teamCount*2 + 2*team + 1], lowSeed[team]);
            const double coveredRange = std::max(5 - highSeed[team] + 1, 0);
            const double total = lowSeed[team] - highSeed[team] + 1;
            fstats[threadPos*sdata.teamCount*2 + 2*team + 1] += std::min(coveredRange/total, 1.0);
        }
    }

    void processData(int pos, int threadCount, unsigned long long total) override {
        if (pos > 0) {
            return;
        }

        long double totalD = static_cast<double>(total);

        for (int i = 0; i < sdata.teamCount; i++) {
            for (int j = 0; j < threadCount; j++) {
                mins[i] = std::min(mins[i], istats[j*sdata.teamCount*2 + i*2]);
                maxs[i] = std::max(maxs[i], istats[j*sdata.teamCount*2 + i*2 + 1]);
                seeds[i] += fstats[j*sdata.teamCount*2 + i*2];
                prob[i] += fstats[j*sdata.teamCount*2 + i*2 + 1];
            }

            seeds[i] /= static_cast<double>(totalD);
            prob[i] /= static_cast<double>(totalD);
        }
    }

    void displayData() override {
        for (int i = 0; i < sdata.teamCount; i++) {
            std::printf("%s:\n\tAvg. Seed: %f\n\tMin. Seed: %d\n\tMax. Seed: %d\n\tPlayoff Probability: %f\n\n",sdata.teamNames[i].c_str(), seeds[i]+1, mins[i]+1, maxs[i]+1, prob[i]);
        }
    }
};

class qmProcessor : public seasonProcessor{
public:
    std::vector<std::vector<std::bitset<256>>> tbMatches;
    std::vector<std::vector<std::bitset<256>>> ntbMatches;
    std::vector<std::string> results;
    std::vector<std::string> posNames;
    std::vector<std::string> negNames;
    seasonData sdata;

    void setupData(int threadCount, seasonData data) override {
        sdata = data;
        int tc2 = sdata.teamCount * sdata.teamCount;
        uint64_t blockCount = sdata.unplayedCount > 5 ? qm::pow3(sdata.unplayedCount - 5) : 1;
        tbMatches.resize(tc2);
        ntbMatches.resize(tc2);
        results.resize(2*tc2);
        for (int i = 0; i < sdata.teamCount * sdata.teamCount; i++) {
            tbMatches.at(i).resize(blockCount);
            ntbMatches.at(i).resize(blockCount);
        }

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

    void processSeason(const std::vector<int> &order, const std::vector<int> &highSeed, const std::vector<int> &lowSeed, unsigned long long season, const long long u, int threadPos) override {
        for (int i = 0; i < sdata.teamCount; i++) {
            if (const int team = order[i]; highSeed[team] == lowSeed[team]) {
                const unsigned long long idx = qm::toTernary(u, sdata.unplayedCount);
                ntbMatches.at(team*sdata.teamCount + highSeed[team])[idx/243][idx%243] = true;
            } else {
                for (int seed = highSeed[team]; seed <= lowSeed[team]; seed++) {
                    const unsigned long long idx = qm::toTernary(u, sdata.unplayedCount);
                    tbMatches.at(team*sdata.teamCount + seed)[idx/243][idx%243] = true;
                }
            }
        }
    }

    void processData(int pos, int threadCount, unsigned long long total) override {
        int start = pos * sdata.teamCount * sdata.teamCount / threadCount;
        int end = (pos + 1) * sdata.teamCount * sdata.teamCount / threadCount;

        for (int i = start; i < end; i++) {
            results[i*2] = qm::getStrFromQm(sdata.unplayedCount, &tbMatches.at(i), posNames, negNames);
            results[i*2 + 1] = qm::getStrFromQm(sdata.unplayedCount, &ntbMatches.at(i), posNames, negNames);
        }
    }

    void displayData() override {
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
};

static void runPerSeason(const long long increment, const int pos, const seasonData &data, seasonProcessor *proc) {
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
        unsigned long long nSeason = getSeason(data, u);
        calculateSeason(nSeason, &order, &highSeed, &lowSeed, data);

        proc->processSeason(order, highSeed, lowSeed, nSeason, u, pos);
    }
}

static void runPerRandomSeason(const long long count, int pos, const seasonData &data, std::mt19937 rng, seasonProcessor *proc) {
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

    auto data = seasonData(teamCount, unplayedCount[0], season[0], unplayed[0], t1Masks, t2Masks, shifts, teams);

    auto readFile = std::chrono::high_resolution_clock::now();

    long long total = 1LL << unplayedCount[0];
    long long increment = total/threads;
    if (total % threads != 0) {
        increment += 1;
    }

    std::vector<std::thread> threadsVec;
    threadsVec.reserve(threads);

    seasonProcessor* proc;
    if (QM) {
        proc = new qmProcessor();
    } else if (FOLDY){
        proc = new foldyProcessor();
    } else {
        proc = new statsProcessor();
    }
    proc->setupData(threads, data);

    auto setupData = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < threads; i++) {
        if (n == 0) {
            threadsVec.emplace_back(runPerSeason, increment, i, data, proc);
        } else {
            threadsVec.emplace_back(runPerRandomSeason, n/threads, i, data, gen, proc);
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