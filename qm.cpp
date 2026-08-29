//
// Created by Tyler on 8/28/2026.
//

#include "qm.h"

#include <algorithm>
#include <bitset>
#include <format>
#include <functional>
#include <map>
#include <ranges>
#include <utility>
#include <__msvc_int128.hpp>

static int table[] = {-1, 0, 1, 72, 2, 46, 73, 96, 3, 14, 47, 56, 74, 18, 97, 118, 4, 43, 15, 35, 48, 38, 57, 23, 75, 92, 19, 86, 98, 51, 119, 29, 5, -1, 44, 12, 16, 41, 36, 90, 49, 126, 39, 124, 58, 60, 24, 105, 76, 62, 93, 115, 20, 26, 87, 102, 99, 107, 52, 82, 120, 78, 30, 110, 6, 64, -1, 71, 45, 95, 13, 55, 17, 117, 42, 34, 37, 22, 91, 85, 50, 28, 127, 11, 40, 89, 125, 123, 59, 104, 61, 114, 25, 101, 106, 81, 77, 109, 63, 70, 94, 54, 116, 33, 21, 84, 27, 10, 88, 122, 103, 113, 100, 80, 108, 69, 53, 32, 83, 9, 121, 112, 79, 68, 31, 8, 111, 67, 7, 66, 65};

#define int128 std::_Signed128

static int countOnes(int128 m) {
    int ones = 0;
    while (m > 0) {
        ones += m&1;
        m = m>>2;
    }
    return ones;
}

static std::vector<std::vector<int128>> makeGroups(int n, const std::vector<int128>& impls) {
    std::vector<std::vector<int128>> groups = std::vector<std::vector<int128>>();
    groups.assign(n+1, std::vector<int128>());

    for (int128 impl : impls) {
        groups.at(countOnes(impl)).push_back(impl);
    }

    return groups;
}

static int compare(int128 a, int128 b) {
    int128 temp = a^b;
    if (temp != 0 && (temp & temp - 1) == 0){
        return table[static_cast<int>(temp % 131)];
    }
    return -1;
}

static int128 convert(long long term, int n) {
    int128 result = 0;
    for (int i = 0; i < n; i ++) {
        result = result << 2;
        result += (term >> (n-i-1)) & 1;
    }
    return result;
}

static auto convert_back(int128 prime, int n, long long arr[]) {
    long long ones = 0;
    long long zeros = 0;
    for (int k = 0; k < n; k++) {
        int check = static_cast<int>((prime >> (n * 2 - 2 - k * 2)) & 0b11);
        if (check == 1) {
            ones += static_cast<int128>(1) << (n-k-1);
        }
        if (check == 2) {
            zeros += static_cast<int128>(1) << (n-k-1);
        }
    }
    arr[0] = ones;
    arr[1] = zeros;
}

std::vector<long long> qm::calcQm(const int n, const std::vector<long long>& terms) {
    std::vector<int128> impls = std::vector<int128>();
    impls.assign(terms.size(), 0);
    std::vector<int128> primes = std::vector<int128>();
    bool combined = true;
    std::vector<int128> nextImpls = std::vector<int128>();
    std::map<int128, std::vector<long long>> termMap = std::map<int128, std::vector<long long>>();
    for (long long term : terms) {
        int128 newTerm = convert(term, n);
        impls.push_back(newTerm);
        termMap[newTerm] = std::vector<long long>();
        termMap[newTerm].push_back(term);
    }

    while (combined) {
        nextImpls.clear();
        combined = false;
        std::vector<std::vector<int128>> groups = makeGroups(n, impls);
        std::vector<int128> used = std::vector<int128>();
        for (int i = 0; i < n; i++) {
            for (int128 x1 : groups[i]) {
                for (int128 x2 : groups[i+1]) {
                    int pos = compare(x1, x2);
                    if (pos != -1) {
                        combined = true;
                        int128 setMask = static_cast<int128>(3) << pos;
                        int128 newTerm = (x1&~setMask) | (static_cast<int128>(2) << pos);
                        used.push_back(x1);
                        used.push_back(x2);
                        termMap[newTerm] = std::vector<long long>();
                        termMap[newTerm].insert(termMap[newTerm].end(), termMap[x1].begin(), termMap[x1].end());
                        termMap[newTerm].insert(termMap[newTerm].end(), termMap[x2].begin(), termMap[x2].end());
                        bool contained = false;
                        for (int128 nextImpl : nextImpls) {
                            if (nextImpl == newTerm) {
                                contained = true;
                                break;
                            }
                        }
                        if (!contained) {
                            nextImpls.push_back(newTerm);
                        }
                    }
                }
            }
        }

        for (int128 impl : impls) {
            if (std::ranges::find(used, impl) == used.end() && std::ranges::find(primes, impl) == primes.end()) {
                primes.push_back(impl);
            }
        }

        impls.clear();
        impls.insert(impls.end(), nextImpls.begin(), nextImpls.end());
    }

    std::vector<int128> finalPrimes = std::vector<int128>();
    std::vector<long long> victim = std::vector<long long>();
    unsigned long long maxLen = 1;
    while (maxLen != 0) {
        maxLen = 0;
        int128 temp = -1;
        int maxInd = 0;
        for (int i = 0; i < primes.size(); i++) {
            int128 prime = primes.at(i);
            for (long long item : victim) {
                auto ptr = std::ranges::find(termMap[prime], item);
                if (ptr != termMap[prime].end()) {
                    termMap[prime].erase(ptr);
                }
            }
            if (termMap[prime].size() > maxLen) {
                maxLen = termMap[prime].size();
                temp = prime;
                maxInd = i;
            }
        }

        if (temp != -1) {
            primes.erase(primes.begin()+maxInd);
            victim = termMap[temp];
            finalPrimes.push_back(temp);
        }
    }

    std::vector<long long> result = std::vector<long long>();
    for (int128 finalPrime : finalPrimes) {
        long long minterm[2];
        convert_back(finalPrime, n, minterm);
        result.push_back(minterm[0]);
        result.push_back(minterm[1]);
    }

    return result;
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
}

namespace {
    struct TreeInfo {
        Node baseNode;
        std::vector<Node> terms;
        std::map<std::string, int> duplicityMap;
    };
}

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
    //
    for (int i = 0; i < qm.size(); i+=2) {
        std::vector<Node> andTerms = std::vector<Node>();
        for (int k = 0; k < n; k++) {
            if ((qm.at(i) & (static_cast<int128>(1) << k)) != 0) {
                andTerms.push_back(terms.at(2*k));
                duplicityMap[terms.at(2*k).nodeType] += 1;
            } else if ((~qm.at(i+1) & (static_cast<int128>(1)) << k) != 0) {
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

std::string qm::getStrFromQm(int n, std::vector<long long> qm, std::vector<std::string> posNames, std::vector<std::string> negNames) {
    return treeToStr(distribute(createTree(n, std::move(qm), std::move(posNames), std::move(negNames))));
}
