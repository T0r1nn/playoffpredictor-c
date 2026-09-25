//
// Created by Tyler on 9/7/2026.
//

#ifndef PLAYOFFPREDICTOR_THREADSEASONS_H
#define PLAYOFFPREDICTOR_THREADSEASONS_H
#include <bitset>
#include <random>
#include <thread>
#include <vector>

namespace ts {
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

    struct performance {
        double getMS = 0.0;
        double seasonMS = 0.0;
        double procMS = 0.0;
    };

    unsigned long long getSeason(const seasonData &data, unsigned long long index);
    void calculateSeason(unsigned long long season, std::vector<int> *order, std::vector<int> *highSeed, std::vector<int> *lowSeed, const seasonData &data);
    class seasonProcessor {
    public:
        virtual ~seasonProcessor() = default;

        virtual void setupData(int dThreadCount, int sThreadCount, seasonData data) = 0;

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

        void setupData(int dThreadCount, int sThreadCount, seasonData data) override;
        void processSeason(const std::vector<int> &order, const std::vector<int> &highSeed, const std::vector<int> &lowSeed, unsigned long long season, long long u, int threadPos) override;
        void processData(int pos, int threadCount, unsigned long long total) override;
        void displayData() override;
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
        std::vector<std::atomic<bool>> done;
        int sThreads;

        void setupData(int dThreadCount, int sThreadCount, seasonData data) override;
        void processSeason(const std::vector<int> &order, const std::vector<int> &highSeed, const std::vector<int> &lowSeed, unsigned long long season, long long u, int threadPos) override;
        void processData(int pos, int threadCount, unsigned long long total) override;
        void displayData() override;
    };
    class qmProcessor : public seasonProcessor {
    public:
        std::vector<std::vector<std::vector<unsigned long long>>> tbMatches;
        std::vector<std::vector<std::vector<unsigned long long>>> ntbMatches;
        std::vector<std::string> results;
        std::vector<std::string> posNames;
        std::vector<std::string> negNames;
        seasonData sdata;
        std::vector<std::atomic<bool>> done;
        int sThreads;

        void setupData(int dThreadCount, int sThreadCount, seasonData data) override;
        void processSeason(const std::vector<int> &order, const std::vector<int> &highSeed, const std::vector<int> &lowSeed, unsigned long long season, long long u, int threadPos) override;
        void processData(int pos, int threadCount, unsigned long long total) override;
        void displayData() override;
    };

    class narrowQmProcessor : public seasonProcessor {
    public:
        std::vector<std::vector<std::vector<unsigned long long>>> tbMatches;
        std::vector<std::vector<std::vector<unsigned long long>>> ntbMatches;
        std::vector<std::string> results;
        std::vector<std::string> posNames;
        std::vector<std::string> negNames;
        seasonData sdata;
        std::vector<std::atomic<bool>> done;
        int sThreads;
        int targetMinSeed;
        int targetMaxSeed;

        void setupData(int dThreadCount, int sThreadCount, seasonData data) override;
        void processSeason(const std::vector<int> &order, const std::vector<int> &highSeed, const std::vector<int> &lowSeed, unsigned long long season, long long u, int threadPos) override;
        void processData(int pos, int threadCount, unsigned long long total) override;
        void displayData() override;
    };
    void runPerSeason(long long increment, int pos, const seasonData &data, seasonProcessor *proc, std::vector<performance> *perfs);
    void runPerRandomSeason(long long count, int pos, const seasonData &data, std::mt19937 rng, seasonProcessor *proc);
} // ts

#endif //PLAYOFFPREDICTOR_THREADSEASONS_H