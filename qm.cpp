//
// Created by Tyler on 8/28/2026.
//

#include "qm.h"

#include <algorithm>
#include <bitset>
#include <cassert>
#include <chrono>
#include <format>
#include <functional>
#include <map>
#include <ranges>
#include <utility>

static unsigned long long convert(long long term, int n) {
    unsigned long long result = 0;
    for (int i = 0; i < n; i ++) {
        result = result << 2;
        result += (term >> (n-i-1)) & 1;
    }
    return result;
}

static auto convert_back(unsigned long long prime, int n, unsigned long long arr[]) {
    unsigned long long ones = 0;
    unsigned long long zeros = 0;
    for (int k = 0; k < n; k++) {
        unsigned long long check = (prime >> ((n-k-1)*2)) & 0b11;
        if (check == 1) {
            ones += 1ULL << (n-k-1);
        }
        if (check == 2) {
            zeros += 1ULL << (n-k-1);
        }
    }
    arr[0] = ones;
    arr[1] = zeros;
}

namespace {
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
}

namespace qm {
    struct HashSet {
        unsigned long long ONE = 1;
        unsigned long long HS_VALUE = ONE << 62;
        unsigned long long HS_VALUE_MASK = HS_VALUE - 1;

        int n;
        unsigned long long mask;
        std::vector<unsigned long long> space;
        std::vector<unsigned long long> contents;

        HashSet() {
            constexpr int bits = 4;
            n = 4;
            space.resize(1ull << bits);
            mask = space.size() - 1;
        }

        void reserve(size_t amount) {
            amount *= 2;
            int e = 0;
            while (amount) {
                amount >>= 1;
                e++;
            }
            if (e > n) {
                widen(e - n);
            }
        }

        static unsigned long long hash(unsigned long long x) {
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
            return static_cast<unsigned long long>(x);
        }

        void widen(const int add_bits=1) {
            n += add_bits;
            space.assign(1ull << n, 0);
            mask = space.size() - 1;
            for (const auto x: contents) {
                insert(x, false);
            }
        }

        void insert(unsigned long long x, bool addToContents=true) {
            unsigned long long h = hash(x);
            unsigned long long idx = h & mask;
            unsigned long long val = x | HS_VALUE;
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

        [[nodiscard]] unsigned long long find(const unsigned long long x) const {
            unsigned long long idx = hash(x) & mask;
            while (space[idx] != 0) {
                if ((space[idx] & HS_VALUE_MASK) == x) {
                    return space[idx];
                }
                idx = (idx + 1) & mask;
            }
            return 0;
        }

        void mark(const unsigned long long x) {
            unsigned long long idx = hash(x) & mask;
            unsigned long long val = x | HS_VALUE;
            unsigned long long markedVal = val | (HS_VALUE << 1);
            while (space[idx] != 0) {
                if (space[idx] == val || space[idx] == markedVal) {
                    space[idx] = markedVal;
                    return;
                }
                idx = (idx+1)&mask;
            }
        }

        [[nodiscard]] std::vector<unsigned long long> list() const {
            std::vector<unsigned long long> ret;
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

    std::vector<unsigned long long> phase2Qm(int n, const std::vector<unsigned long long> &primes, std::vector<long long>& terms) {
        std::vector<unsigned long long> finalPrimes;
        std::vector<unsigned long long> victim;
        std::vector<unsigned long long> nextVictim;
        std::vector<HashSet> sets;
        HashSet p;
        p.reserve(primes.size());
        for (const auto u : primes) {
            p.insert(u);
        }
        sets.assign(p.contents.size(), HashSet());
        for (const auto term : terms) {
            const auto nt = convert(term, n);
            for (int i = 0; i < p.contents.size(); i++) {
                bool works = true;
                const auto prime = p.contents.at(i);
                for (int j = 0; j < n; j++) {
                    if (auto primeType = (prime >> (2*j))&3; primeType != 2) {
                        if (auto termType = (nt >> (2*j))&3; primeType != termType) {
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
            unsigned long long temp = -1;
            std::vector<unsigned long long> primesList = p.list();
            for (int i = 0; i < primesList.size(); i++) {
                const unsigned long long prime = primesList.at(i);
                HashSet set = sets.at(i);
                for (const unsigned long long item : victim) {
                    if (set.find(item) != 0) {
                        set.mark(item);
                    }
                }
                if (std::vector<unsigned long long> setList = set.list(); setList.size() > maxLen) {
                    maxLen = setList.size();
                    temp = prime;
                    nextVictim = setList;
                }
            }

            if (temp != -1) {
                p.mark(temp);
                std::swap(victim, nextVictim);
                nextVictim.clear();
                finalPrimes.push_back(temp);
            }
        }

        p.clear();

        auto result = std::vector<unsigned long long>();
        for (unsigned long long finalPrime : finalPrimes) {
            unsigned long long minterm[2];
            convert_back(finalPrime, n, minterm);
            result.push_back(minterm[0]);
            result.push_back(minterm[1]);
        }

        return result;
    }

    std::vector<unsigned long long> skipPhase2(int n, const std::vector<unsigned long long> &p) {
        std::vector<unsigned long long> result = std::vector<unsigned long long>();
        for (unsigned long long finalPrime : p) {
            unsigned long long minterm[2];
            convert_back(finalPrime, n, minterm);
            result.push_back(minterm[0]);
            result.push_back(minterm[1]);
        }

        return result;
    }

    unsigned long long pow3(int e) {
        assert(e >= 0);
        unsigned long long ret = 1;
        unsigned long long cur = 3;
        while (e) {
            if (e&1) {
                ret = ret * cur;
            }
            e >>= 1;
            cur *= cur;
        }
        return ret;
    }

    unsigned long long toTernary(unsigned long long term, int n) {
        unsigned long long result = 0;
        for (int i = 0; i < n; i++) {
            result = result*3 + ((term >> i)&1);
        }
        return result;
    }

    const std::bitset<256> masks[] = {
        std::bitset<256>("001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001001"),
        std::bitset<256>("000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111000000111"),
        std::bitset<256>("000000000000000000111111111000000000000000000111111111000000000000000000111111111000000000000000000111111111000000000000000000111111111000000000000000000111111111000000000000000000111111111000000000000000000111111111000000000000000000111111111"),
        std::bitset<256>("000000000000000000000000000000000000000000000000000000111111111111111111111111111000000000000000000000000000000000000000000000000000000111111111111111111111111111000000000000000000000000000000000000000000000000000000111111111111111111111111111"),
        std::bitset<256>("000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000111111111111111111111111111111111111111111111111111111111111111111111111111111111")
    };

    static std::vector<unsigned long long> calcSparseQm(const int n, std::vector<long long> *terms) {
        HashSet S;
        HashSet S2;
        for (long long term : *terms) {
            S.insert(convert(term, n));
        }

        std::vector<unsigned long long> primes;
        for (int w = 0; w < n; w++) {
            std::vector<unsigned long long> news;
            news.reserve(S.contents.size());
            for (const auto s : S.contents) {
                auto ss = s;
                for (int i = 0; i < n; i++) {
                    if (auto type = ss & 3; type == 0) {
                        if (const unsigned long long t = s ^ (1ULL << (2 * i)); S.find(t)) {
                            unsigned long long u = s ^ (2ULL << (2 * i));
                            news.push_back(u);
                        }
                    } else if (type == 2) {
                        break;
                    }
                }
            }
            S2.reserve(news.size());
            for (const auto u : news) {
                S2.insert(u);
                for (int i = 0; i < n; i++) {
                    if (const auto type = (u >> (2*i))&3; type == 2) { // 10 -> *
                        const auto s = u ^ (2ULL << (2*i)); // 00 -> 0
                        const auto t = u ^ (3ULL << (2*i)); // 01 -> 1
                        S.mark(s);
                        S.mark(t);
                    }
                }
            }

            for (const auto s : S.list()) {
                primes.push_back(s);
            }

            S.clear();
            std::swap(S, S2);

            if (S.contents.empty()) {
                break;
            }
        }
        for (auto u : S.contents) {
            primes.push_back(u);
        }
        return primes;
    }

    static std::vector<unsigned long long> calcDenseQm(const int n, std::vector<std::bitset<256>> *S) {
        const int nh = n-5;

        const unsigned long long block_size = nh > 0 ? pow3(nh) : 1;

        {
            size_t step = 1;
            for (int i = 1; i <= nh; i++) {//this loop's order matters, but if we run it twice then it stops mattering
                const size_t shift = step;
                step *= 3;
                for (size_t b = 0; b < block_size; b+=step) {
                    for (size_t c = 0; c < shift; c++) {
                        const size_t id_s = b+c;
                        const size_t id_t = id_s + shift;
                        const size_t id_u = id_t + shift;
                        S->at(id_u) |= S->at(id_s) & S->at(id_u);
                    }
                }
            }
        }
        {
            for (auto & i : *S) {//super easily parralizable, each bitset is a thread
                auto Sa = i;
                std::bitset<256> s, t;

                s = Sa & masks[0];
                t = (Sa >> 1) & masks[0];
                Sa |= (s&t) << 2;

                s = Sa & masks[1];
                t = (Sa >> 3) & masks[1];
                Sa |= (s&t) << 6;

                s = Sa & masks[2];
                t = (Sa >> 9) & masks[2];
                Sa |= (s&t) << 18;

                s = Sa & masks[3];
                t = (Sa >> 27) & masks[3];
                Sa |= (s&t) << 54;

                s = Sa & masks[4];
                t = (Sa >> 81) & masks[4];
                Sa |= (s&t) << 162;

                std::bitset<256> u;

                u = (Sa >> 2) & masks[0];
                Sa &= ~(u | (u << 1));

                u = (Sa >> 6) & masks[1];
                Sa &= ~(u | (u << 3));

                u = (Sa >> 18) & masks[2];
                Sa &= ~(u | (u << 9));

                u = (Sa >> 54) & masks[3];
                Sa &= ~(u | (u << 27));

                u = (Sa >> 162) & masks[4];
                Sa &= ~(u | (u << 81));

                i = Sa;
            }
        }
        {
            size_t step = 1;
            for (int i = 1; i <= nh; i++) {//order does matter here and I don't really see an easy way to make it not matter
                size_t shift = step;
                step *= 3;
                    for (size_t b = 0; b < block_size; b+=step) {//block_size/3^i
                    for (size_t c = 0; c < shift; c++) {//3^(i-1)
                        const size_t id_s = b+c;
                        const size_t id_t = id_s + shift;
                        const size_t id_u = id_t + shift;
                        const auto tmp = ~S->at(id_u);
                        S->at(id_s) &= tmp;
                        S->at(id_t) &= tmp;
                    }
                }
            }
        }

        std::vector<unsigned long long> primes;
        for (unsigned long long term = 0; term < block_size * 243; term ++) {
            if (S->at(term/243)[term%243]) {
                unsigned long long prime = 0;
                unsigned long long t2 = term;
                for (int i = 0; i < n; i++) {
                    prime = (prime << 2) + (t2%3);
                    t2/=3;
                }
               primes.push_back(prime);
            }
        }
        return primes;
    }

    static TreeInfo createTree(int n, std::vector<unsigned long long> qm, std::vector<std::string> posNames, std::vector<std::string> negNames) {
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
                if ((qm.at(i) & (static_cast<unsigned long long>(1) << k)) != 0) {
                    andTerms.push_back(terms.at(2*k));
                    duplicityMap[terms.at(2*k).nodeType] += 1;
                } else if ((~qm.at(i+1) & (static_cast<unsigned long long>(1)) << k) != 0) {
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

    std::string getStrFromQm(int n, std::vector<std::bitset<256>> *terms, std::vector<std::string> posNames, std::vector<std::string> negNames) {
        auto simplified = skipPhase2(n, calcDenseQm(n, terms));

        if (simplified.size() == 2 && simplified[1] == (1ull << n) - 1) {
            return "Always";
        }

        return treeToStr(distribute(createTree(n, std::move(simplified), std::move(posNames), std::move(negNames))));
    }
}