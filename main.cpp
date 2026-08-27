#include <bitset>
#include <cstring>
#include <format>
#include <iostream>
#include <fstream>
#include <thread>

static int getTeamCount(char* filename) {
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

static void readCSV(char* filename, std::string teams[], long long *season, long long *unplayed, int *unplayedCount) {
    unplayedCount[0] = 0;
    std::ifstream csv(filename);

    std::string line;

    std::getline(csv, line);

    int ind = -1;
    std::string team;
    for (int i = 0; i < line.length(); i++) {
        if (line[i] == ',' || line[i] == '\n') {
            if (ind != -1) {
                teams[ind] = team;
            }
            team = "";
            ind += 1;
        } else {
            team += line[i];
        }
    }
    teams[ind] = team;

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
        int res = x & 1LL;
        long long z = y & (y-1);
        long long maskAdd = res == 0 ? 0LL : y-z;
        mask += maskAdd;
        y = z;
        x = x>>1;
    }

    return season | mask;
}

static void calculateSeason(long long season, int order[], int highSeed[], int lowSeed[], int teamCount, const long long t1Masks[], const long long t2Masks[], int shifts[]) {
    long long results[teamCount];
    int wins[teamCount];
    int tiebreakers[teamCount];
    long long invSeason = ~season;
    for (int i = 0; i < teamCount; i ++) {
        results[i] = (invSeason & t1Masks[i]) | (season & t2Masks[i]);
        wins[i] = 0;
        tiebreakers[i] = 0;
        order[i] = i;
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
        int value = order[i];
        while (j >= 0 && (wins[order[j]] < wins[value] || (wins[order[j]] == wins[value] && tiebreakers[order[j]] < tiebreakers[value]))) {
            order[j+1] = order[j];
            j = j - 1;
        }
        order[j+1] = value;
    }

    for (int i = 0; i < teamCount; i += 1) {
        int newMin = i;
        int newMax = i;
        for (int j = i-1; j >= 0; j --) {
            if (wins[order[i]] == wins[order[j]] and tiebreakers[order[i]] == tiebreakers[order[j]]) {
                newMin = i;
            }else {
                break;
            }
        }
        for (int j = i+1; j < teamCount; j ++) {
            if (wins[order[i]] == wins[order[j]] and tiebreakers[order[i]] == tiebreakers[order[j]]) {
                newMax = i;
            }else {
                break;
            }
        }
        highSeed[order[i]] = newMin;
        lowSeed[order[i]] = newMax;
    }
}

static void calculateRangeStats(long long increment, int pos, long long season, long long unplayed, int teamCount, int unplayedCount, const long long t1Masks[], const long long t2Masks[], const int shifts[], int *istats[], float *fstats[]) {
    int order[teamCount];
    int highSeed[teamCount];
    int lowSeed[teamCount];
    long long start = increment * pos;
    long long maxSeason = (1LL << unplayedCount);
    long long end = std::min(increment * (pos + 1), maxSeason);
    if (start >= maxSeason) {
        return;
    }
    for (int u = start; u < end; u++) {
        calculateSeason(getSeason(season, unplayed, u, unplayedCount), order, highSeed, lowSeed, teamCount, t1Masks, t2Masks, shifts);

        for (int i = 0; i < 10; i ++) {
            int team = order[i];
            float seed = (highSeed[team] + lowSeed[team])/2.0f;
            fstats[pos][2*team] += seed;
            istats[pos][2*team] = std::min(istats[pos][2*team], highSeed[team]);
            istats[pos][2*team + 1] = std::max(istats[pos][2*team + 1], lowSeed[team]);
            float coveredRange = std::max(5 - highSeed[team] + 1, 0);
            float total = lowSeed[team] - highSeed[team] + 1;
            fstats[pos][2*team + 1] += std::min(coveredRange/total, 1.0f);
        }
    }
}

int main(int argc, char *argv[]) {
    char* filename = argv[argc-1];
    bool CUDA = false;
    int n = 0;
    int threads = 1;
    bool FOLDY = false;
    bool QM = false;
    if (argc == 1) {
        printf("Usage:\n"
               "\tplayoffpredictor [options] <filename>\n"
               "\n"
               "Options:\n"
               "\t--cuda\t\tEnables CUDA mode to accelerate performance.\n"
               "\t-t <count>\tSets the amount of threads to use. If CUDA is enabled, each thread is a block of 256.\n"
               "\t--foldy\t\tEnables Foldy mode, generating a foldy sheet as a .csv file.\n"
               "\t-n <count>\tInstead of running every season, instead runs n random seasons.\n"
               "\t--qm\t\tUses the Quine-McCluskey algorithm to calculate each team's route to each seed.");
        return 0;
    }
    for (int i = 1; i < argc-1; i++) {
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
        } else {
            throw std::invalid_argument(std::format("Unsupported argument: {}", argv[i]));
        }
    }
    int teamCount = getTeamCount(filename);
    std::string teams[teamCount];
    long long *season = (long long*)malloc(sizeof(long long));
    long long *unplayed = (long long*)malloc(sizeof(long long));
    int *unplayedCount = (int*)malloc(sizeof(int));
    readCSV(filename, teams, season, unplayed, unplayedCount);

    int shifts[teamCount];
    long long t1Masks[teamCount];
    long long t2Masks[teamCount];
    int ind = 0;
    for (int i = 0; i < teamCount; i++) {
        shifts[i] = -0.5 * i * i + (teamCount - 1.5) * i - 1;
        t1Masks[i] = 0LL;
        t2Masks[i] = 0LL;
        for (int j = i+1; j < teamCount; j++) {
            t2Masks[i] += 1LL << ind;
            t1Masks[j] += 1LL << ind;
            ind += 1;
        }
    }

    long long total = 1LL << unplayedCount[0];
    long long increment = total/threads;
    if (total % threads != 0) {
        increment += 1;
    }

    std::thread simThreads[threads];
    int istats[threads][teamCount];
    float fstats[threads][teamCount];
    for (int i = 0; i < threads; i++) {
        simThreads[i] = std::thread(calculateRangeStats, increment, i, season[0], unplayed[0], teamCount, unplayedCount[0], t1Masks, t2Masks, shifts, &istats, &fstats);
        simThreads[i].join();
    }

    return 0;
}