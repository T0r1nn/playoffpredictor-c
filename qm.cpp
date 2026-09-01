//
// Created by Tyler on 8/28/2026.
//

#include "qm.h"

#include <algorithm>
#include <assert.h>
#include <bitset>
#include <chrono>
#include <format>
#include <functional>
#include <map>
#include <ranges>
#include <utility>

static int table[] = {-1, 0, 1, 72, 2, 46, 73, 96, 3, 14, 47, 56, 74, 18, 97, 118, 4, 43, 15, 35, 48, 38, 57, 23, 75, 92, 19, 86, 98, 51, 119, 29, 5, -1, 44, 12, 16, 41, 36, 90, 49, 126, 39, 124, 58, 60, 24, 105, 76, 62, 93, 115, 20, 26, 87, 102, 99, 107, 52, 82, 120, 78, 30, 110, 6, 64, -1, 71, 45, 95, 13, 55, 17, 117, 42, 34, 37, 22, 91, 85, 50, 28, 127, 11, 40, 89, 125, 123, 59, 104, 61, 114, 25, 101, 106, 81, 77, 109, 63, 70, 94, 54, 116, 33, 21, 84, 27, 10, 88, 122, 103, 113, 100, 80, 108, 69, 53, 32, 83, 9, 121, 112, 79, 68, 31, 8, 111, 67, 7, 66, 65};

uint128 ONE = 1;
uint128 HS_VALUE = ONE << 126;
uint128 HS_VALUE_MASK = HS_VALUE - 1;

struct HashSet {
    int n;
    uint64_t mask;
    std::vector<uint128> space;
    std::vector<uint128> contents;

    HashSet() {
        constexpr int bits = 4;
        n = 4;
        space.resize(1ull << bits);
        mask = space.size() - 1;
    }

    static uint64_t hash(uint128 x) {
        x ^= 0x53d4cdfafda2f0b1ull;
        #ifdef N_EXT
        x *= 0xcdfafda2f0b13ef7ull;
        x ^= x >> 60;
        #endif
        x *= 0x7fabcdef;
        x ^= x >> 11;
        x ^= x >> 27;
        x *= 0x7f17316b;
        x ^= x >> 11;
        x ^= x >> 27;
        x ^= x >> 17;
        x ^= x >> 13;
        return static_cast<uint64_t>(x);
    }

    void reserve(size_t amount) {
        amount *= 2;
        int e = 0;
        while (amount) {
            amount >>= 1;
            e++;
        }
        if (e > n) {
            widen(e-n);
        }
    }

    void widen(const int add_bits=1) {
        n += add_bits;
        space.assign(1ull << n, 0);
        mask = space.size() - 1;
        for (const auto x: contents) {
            insert(x, false);
        }
    }

    void insert(uint128 x, bool addToContents=true) {
        uint64_t h = hash(x);
        uint64_t idx = h & mask;
        uint128 val = x | HS_VALUE;
        int itr = 0;
        while (space[idx] != 0) {
            if (space[idx] == val) {
                return;
            }
            idx = (idx+1) & mask;
            if (++itr == 50) {
                widen();
                idx = h&mask;
            }
        }
        space[idx] = val;
        if (addToContents) {
            contents.push_back(x);
        }
        if (contents.size() >= space.size()) {
            widen();
        }
    }

    uint128 find(uint128 x) {
        uint64_t idx = hash(x) & mask;
        while (space[idx] != 0) {
            if ((space[idx] & HS_VALUE_MASK) == x) {
                return space[idx];
            }
            idx = (idx + 1) & mask;
        }
        return 0;
    }

    void mark(uint128 x) {
        uint64_t idx = hash(x) & mask;
        uint128 val = x | HS_VALUE;
        uint128 markedVal = val | (HS_VALUE << 1);
        while (space[idx] != 0) {
            if (space[idx] == val || space[idx] == markedVal) {
                space[idx] = markedVal;
                return;
            }
            idx = (idx+1)&mask;
        }
    }

    std::vector<uint128> list() {
        std::vector<uint128> ret;
        ret.reserve(contents.size());
        for (auto x : contents) {
            if (find(x) == (x | HS_VALUE)) {
                ret.push_back(x);
            }
        }

        return ret;
    }

    void clear() {
        space.assign(1ull << n, 0);
        contents.clear();
    }
};

static int countOnes(uint128 m) {
    int ones = 0;
    while (m > 0) {
        ones += m&1;
        m = m>>2;
    }
    return ones;
}

static uint128 convert(long long term, int n) {
    uint128 result = 0;
    for (int i = 0; i < n; i ++) {
        result = result << 2;
        result += (term >> (n-i-1)) & 1;
    }
    return result;
}

static auto convert_back(uint128 prime, int n, long long arr[]) {
    long long ones = 0;
    long long zeros = 0;
    for (int k = 0; k < n; k++) {
        uint128 check = (prime >> ((n-k-1)*2)) & 0b11;
        if (check == 1) {
            ones += static_cast<uint128>(1) << (n-k-1);
        }
        if (check == 2) {
            zeros += static_cast<uint128>(1) << (n-k-1);
        }
    }
    arr[0] = ones;
    arr[1] = zeros;
}

namespace qm {
    HashSet calcSparseQm(int n, const std::vector<long long>& terms) {
        HashSet S;
        HashSet S2;
        HashSet primes;

        S.reserve(terms.size());
        primes.reserve(terms.size());
        for (long long term : terms) {
            S.insert(convert(term, n));
        }
        for (int w = 0; w < n+1; w++) {
            std::vector<uint128> news;
            news.reserve(S.contents.size());
            for (auto impl : S.contents) {
                auto implCheck = impl;
                uint128 setNum = 1;
                uint128 pairNum = 2;
                for (int i = 0; i < n; i++) {
                    auto type = implCheck&3;
                    if (type == 0 && S.find(impl | setNum) != 0) {
                        news.push_back(impl | pairNum);
                    } else if (type == 2) {
                        break;
                    }
                    implCheck >>= 2;
                    setNum <<= 2;
                    pairNum <<= 2;
                }
            }
            S2.reserve(news.size());
            for (auto u : news) {
                S2.insert(u);
                uint128 zeroFlip = 2;
                uint128 oneFlip = 3;
                for (int i = 0; i < n; i++) {
                    auto type = (u >> (2*i))&3;
                    if (type == 2) {
                        auto zero = u ^ zeroFlip;
                        auto one = u ^ oneFlip;
                        S.mark(zero);
                        S.mark(one);
                    }
                    zeroFlip <<= 2;
                    oneFlip <<= 2;
                }
            }

            for (auto s: S.list()) {
                primes.insert(s);
            }

            S.clear();
            std::swap(S.space, S2.space);
            std::swap(S.contents, S2.contents);
            std::swap(S.mask, S2.mask);
            std::swap(S.n, S2.n);

            if (S.contents.empty()) {
                break;
            }
        }

        S.clear();
        S2.clear();

        return primes;
    }

    std::vector<long long> phase2Qm(int n, HashSet primes, std::vector<long long>& terms) {
        std::vector<uint128> finalPrimes;
        std::vector<uint128> victim;
        std::vector<uint128> nextVictim;
        std::vector<HashSet> sets;
        sets.assign(primes.contents.size(), HashSet());
        for (auto term : terms) {
            auto nt = convert(term, n);
            for (int i = 0; i < primes.contents.size(); i++) {
                bool works = true;
                auto prime = primes.contents.at(i);
                for (int j = 0; j < n; j++) {
                    auto primeType = (prime >> (2*j))&3;
                    if (primeType != 2) {
                        auto termType = (nt >> (2*j))&3;
                        if (primeType != termType) {
                            works = false;
                            break;
                        }
                    }
                }
                if (works) {
                    sets.at(i).insert(nt);
                }
            }
        }
        unsigned long long maxLen = 1;
        while (maxLen != 0) {
            maxLen = 0;
            uint128 temp = -1;
            std::vector<uint128> primesList = primes.list();
            for (int i = 0; i < primesList.size(); i++) {
                uint128 prime = primesList.at(i);
                HashSet set = sets.at(i);
                for (uint128 item : victim) {
                    if (set.find(item) != 0) {
                        set.mark(item);
                    }
                }
                std::vector<uint128> setList = set.list();
                if (setList.size() > maxLen) {
                    maxLen = setList.size();
                    temp = prime;
                    nextVictim = setList;
                }
            }

            if (temp != -1) {
                primes.mark(temp);
                std::swap(victim, nextVictim);
                nextVictim.clear();
                finalPrimes.push_back(temp);
            }
        }

        std::vector<long long> result = std::vector<long long>();
        for (uint128 finalPrime : finalPrimes) {
            long long minterm[2];
            convert_back(finalPrime, n, minterm);
            result.push_back(minterm[0]);
            result.push_back(minterm[1]);
        }

        return result;
    }

    std::vector<long long> skipPhase2(int n, HashSet p) {
        std::vector<long long> result = std::vector<long long>();
        for (uint128 finalPrime : p.contents) {
            long long minterm[2];
            convert_back(finalPrime, n, minterm);
            result.push_back(minterm[0]);
            result.push_back(minterm[1]);
        }

        return result;
    }

    static inline uint64_t pow3(int e) {
        assert(e >= 0);
        uint64_t ret = 1;
        uint64_t cur = 3;
        while (e) {
            if (e&1) {
                ret = ret * cur;
            }
            e >>= 1;
            cur *= cur;
        }
        return ret;
    }

    static uint64_t toTernary(uint64_t term, int n) {
        uint64_t result = 0;
        for (int i = 0; i < n; i++) {
            result = result*3 + ((term >> i)&1);
        }
        return result;
    }

    const size_t shifts[] = {1,3,9,27,81};
    const std::bitset<256> masks[] = {
        std::bitset<256>("001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001"),
        std::bitset<256>("000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111"),
        std::bitset<256>("000000000000000000111111111000000000000000000111111111000000000000000000111111111000000000000000000111111111000000000000000000111111111000000000000000000111111111000000000000000000111111111000000000000000000111111111000000000000000000111111111"),
        std::bitset<256>("000000000000000000000000000000000000000000000000000000111111111111111111111111111000000000000000000000000000000000000000000000000000000111111111111111111111111111000000000000000000000000000000000000000000000000000000111111111111111111111111111"),
        std::bitset<256>("000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000111111111111111111111111111111111111111111111111111111111111111111111111111111111")
    };

    HashSet calcDenseQm(int n, const std::vector<long long>& terms) {
        int nh = n-5;

        std::vector<std::bitset<256>> S;
        uint64_t block_size = nh > 0 ? pow3(nh) : 1;

        S.resize(block_size);

        for (long long term : terms) {
            uint64_t idx = toTernary(term, n);
            S[idx/243][idx%243] = 1;
        }

        {
            size_t step = 1;
            for (int i = 1; i <= nh; i++) {
                size_t shift = step;
                step *= 3;
                for (size_t b = 0; b < block_size; b+=step) {
                    for (size_t c = 0; c < shift; c++) {
                        size_t id_s = b+c;
                        size_t id_t = id_s + shift;
                        size_t id_u = id_t + shift;
                        S[id_u] |= S[id_s] & S[id_u];
                    }
                }
            }
        }
        {
            for (auto &Sa : S) {
                for (int i = 0; i < 5; i++) {
                    auto mask = masks[i];
                    auto shift = shifts[i];

                    auto s = Sa & mask;
                    auto t = (Sa >> shift) & mask;
                    auto u = (Sa >> (shift * 2)) & mask;
                    u |= s & t;
                    Sa = s | (t << shift) | (u << (shift * 2));
                }
            }
        }
        {
            size_t step = 1;
            for (int i = 1; i <= nh; i++) {
                size_t shift = step;
                step *= 3;
                for (size_t b = 0; b < block_size; b+=step) {
                    for (size_t c = 0; c < shift; c++) {
                        size_t id_s = b+c;
                        size_t id_t = id_s + shift;
                        size_t id_u = id_t + shift;
                        auto tmp = ~S[id_u];
                        S[id_s] &= tmp;
                        S[id_t] &= tmp;
                    }
                }
            }
        }
        {
            for (auto &Sa : S) {
                for (int i = 0; i < 5; i++) {
                    auto mask = masks[i];
                    auto shift = shifts[i];

                    auto s = Sa & mask;
                    auto t = (Sa >> shift) & mask;
                    auto u = (Sa >> (shift * 2)) & mask;
                    auto iu = ~u;
                    s &= iu;
                    t &= iu;
                    Sa = s | (t << shift) | (u << (shift * 2));
                }
            }
        }

        //Now S[index] should be 1 for each prime implicant
        HashSet primes;
        for (uint64_t term = 0; term < block_size * 243; term ++) {
            if (S[term/243][term%243]) {
                uint128 prime = 0;
                uint64_t t2 = term;
                for (int i = 0; i < n; i++) {
                    prime = (prime << 2) + (t2%3);
                    t2/=3;
                }
               primes.insert(prime);
            }
        }
        return primes;
    }

    class Node {
    public:
        std::string nodeType;
        std::vector<Node> children = std::vector<Node>();
        void connect(const Node& node) {
            this->children.push_back(node);
        }

        bool operator<(const Node& other) const {
            if (this->nodeType != other.nodeType) {
                return this->nodeType < other.nodeType;
            }
            return this->children.begin() < other.children.begin();
        }

        bool operator==(const Node &other) const {
            if (this->nodeType != other.nodeType) {
                return false;
            }
            if (this->children.size() != other.children.size()) {
                return false;
            }
            for (int i = 0; i < this->children.size(); i++) {
                if (this->children.at(i) != other.children.at(i)) {
                    return false;
                }
            }
            return true;
        }
    };


    struct TreeInfo {
        Node baseNode;
        std::vector<Node> terms;
        std::map<std::string, int> duplicityMap;
    };

    static TreeInfo createTree(int n, std::vector<long long> qm, std::vector<std::string> posNames, std::vector<std::string> negNames) {
        Node baseNode = Node{.nodeType = "|"};
        std::vector<Node> terms = std::vector<Node>();
        std::map duplicityMap = std::map<std::string, int>();
        for (int a = 0; a < n; a++) {
            Node pTerm = Node(posNames.empty() ? std::format("{}", a) : posNames[a]);
            Node nTerm = Node(negNames.empty() ? (posNames.empty() ? std::format("~{}", a) : std::format("NOT {}", posNames[a])) : negNames[a]);
            terms.push_back(pTerm);
            terms.push_back(nTerm);
            duplicityMap[pTerm.nodeType] = 0;
            duplicityMap[nTerm.nodeType] = 0;
        }
        for (int i = 0; i < qm.size(); i+=2) {
            std::vector<Node> andTerms = std::vector<Node>();
            for (int k = 0; k < n; k++) {
                if ((qm.at(i) & (static_cast<uint128>(1) << k)) != 0) {
                    andTerms.push_back(terms.at(2*k));
                    duplicityMap[terms.at(2*k).nodeType] += 1;
                } else if ((~qm.at(i+1) & (static_cast<uint128>(1)) << k) != 0) {
                    andTerms.push_back(terms.at(2*k+1));
                    duplicityMap[terms.at(2*k+1).nodeType] += 1;
                }
            }

            if (andTerms.size() == 1) {
                baseNode.connect(andTerms.at(0));
            } else {
                Node andNode = Node("&");
                andNode.children = andTerms;
                baseNode.connect(andNode);
            }
        }

        return TreeInfo(baseNode, terms, duplicityMap);
    }

    static std::string treeToStr(const Node &tree) {
        std::string result;
        if (tree.nodeType == "|" or tree.nodeType == "&") {
            if (tree.children.size() == 1) {
                result = treeToStr(tree.children.at(0));
            } else {
                for (int i = 0; i < tree.children.size(); i++) {
                    Node child = tree.children.at(i);
                    if (child.nodeType == "|" or child.nodeType == "&") {
                        result += std::format("({})", treeToStr(child));
                    } else {
                        result += child.nodeType;
                    }
                    if (i < tree.children.size() - 1) {
                        result += std::format(" {} ", tree.nodeType == "|" ? "or" : "and");
                    }
                }
            }
        } else {
            result = tree.nodeType;
        }
        return result;
    }

    static Node distribute(const TreeInfo& info) {
        Node tree = info.baseNode;
        std::vector terms = info.terms;
        std::map duplicityMap = info.duplicityMap;

        int maxDup = 2;
        Node maxTerm = Node();
        while (maxDup > 1) {
            maxDup = 1;
            maxTerm = Node();
            for (const Node& term : terms) {
                if (duplicityMap[term.nodeType] > maxDup) {
                    maxDup = duplicityMap[term.nodeType];
                    maxTerm = term;
                }
            }
            if (maxDup > 1) {
                std::map<std::string, int> newDuplicityMap = std::map<std::string, int>();
                for (const auto &key: duplicityMap | std::views::keys) {
                    newDuplicityMap[key] = 0;
                }
                terms.erase(std::ranges::find(terms, maxTerm));
                Node newNode = Node("&");
                newNode.connect(maxTerm);
                Node orNode = Node("|");
                std::vector<int> toRemove = std::vector<int>();
                for (int i = static_cast<int>(tree.children.size())-1; i >= 0; i--) {
                    Node node = tree.children.at(i);
                    auto ind = std::ranges::find(node.children, maxTerm);
                    if (ind != node.children.end()) {
                        node.children.erase(ind);
                        for (const Node& term : node.children) {
                            duplicityMap[term.nodeType] -= 1;
                            newDuplicityMap[term.nodeType] += 1;
                        }
                        if (node.children.size() > 1) {
                            orNode.connect(node);
                        } else {
                            orNode.connect(node.children.at(0));
                        }
                        toRemove.push_back(i);
                    }
                }
                for (int ind : toRemove) {
                    tree.children.erase(tree.children.begin() + ind);
                }
                orNode = distribute(TreeInfo(orNode, terms, newDuplicityMap));
                if (orNode.children.size() == 1) {
                    newNode.children.insert(newNode.children.end(), orNode.children[0].children.begin(), orNode.children[0].children.end());
                }else {
                    newNode.connect(orNode);
                }
                tree.connect(newNode);
            }
        }
        return tree;
    }

    std::string getStrFromQm(int n, std::vector<long long> terms, std::vector<std::string> posNames, std::vector<std::string> negNames) {
        auto dqmStart = std::chrono::high_resolution_clock::now();
        auto dqm = calcDenseQm(n, terms);

        auto sqmStart = std::chrono::high_resolution_clock::now();
        auto sqm = calcSparseQm(n, terms);

        auto qmPhase2 = std::chrono::high_resolution_clock::now();
        auto simplified = skipPhase2(n, dqm);

        if (simplified.size() == 2 && simplified[1] == (1ll << n) - 1) {
            return "Always";
        }

        auto treeCreationStart = std::chrono::high_resolution_clock::now();
        auto tree = createTree(n, std::move(simplified), std::move(posNames), std::move(negNames));

        auto distributeStart = std::chrono::high_resolution_clock::now();
        auto distributedTree = distribute(tree);

        auto stringStart = std::chrono::high_resolution_clock::now();
        auto result = treeToStr(distributedTree);
        auto end = std::chrono::high_resolution_clock::now();

        auto totalTime = (end - dqmStart).count()/1.0e6;
        auto dqm1Time = (sqmStart - dqmStart).count()/1.0e6;
        auto sqm1Time = (qmPhase2 - sqmStart).count()/1.0e6;
        auto qm2Time = (treeCreationStart - qmPhase2).count()/1.0e6;
        auto treeTime = (distributeStart - treeCreationStart).count()/1.0e6;
        auto distTime = (stringStart - distributeStart).count()/1.0e6;
        auto strTime = (end - stringStart).count()/1.0e6;

        auto density = terms.size() / ((1 << n) - 1.0);

        std::printf("Density: %f\nTotal QM Time: %f\n\tDQM 1: %f\n\tSQM 1: %f\n\tQM 2: %f\n\tTree Creation: %f\n\tTree Simplification: %f\n\tString Creation: %f\n\n", density, totalTime, dqm1Time, sqm1Time, qm2Time, treeTime, distTime, strTime);

        return result;
    }
}