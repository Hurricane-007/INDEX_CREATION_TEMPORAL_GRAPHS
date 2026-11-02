#ifndef COMMON_H
#define COMMON_H

#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <cstdint>
#include <cmath>

// Temporal edge representation: (u, v, t)
struct Edge {
    int u, v, t;
    Edge(int _u = 0, int _v = 0, int _t = 0) : u(_u), v(_v), t(_t) {}
};

// Edge with appearing time interval start ti and timestamp tj
struct TSFEdge {
    int u, v;
    int ti; // Appearing interval start timestamp: [ti, tj]
    TSFEdge(int _u = 0, int _v = 0, int _ti = 0) : u(_u), v(_v), ti(_ti) {}

    bool operator<(const TSFEdge& other) const {
        return ti < other.ti;
    }
};

// Label-time pair for U-baseline: <label, time>
struct LabelTime {
    int label;
    int time;
    LabelTime(int l = -1, int t = -1) : label(l), time(t) {}
};

// Fast Disjoint Set Union (Union-Find) with path compression and union-by-rank
class DisjointSet {
private:
    std::vector<int> parent;
    std::vector<int> rank;
    int count;

public:
    DisjointSet() : count(0) {}
    explicit DisjointSet(int n) {
        init(n);
    }

    void init(int n) {
        parent.resize(n);
        rank.assign(n, 0);
        for (int i = 0; i < n; ++i) parent[i] = i;
        count = n;
    }

    int find(int u) {
        if (parent[u] != u) {
            parent[u] = find(parent[u]);
        }
        return parent[u];
    }

    bool unite(int u, int v) {
        int root_u = find(u);
        int root_v = find(v);
        if (root_u == root_v) return false;

        if (rank[root_u] < rank[root_v]) {
            parent[root_u] = root_v;
        } else if (rank[root_u] > rank[root_v]) {
            parent[root_v] = root_u;
        } else {
            parent[root_v] = root_u;
            rank[root_u]++;
        }
        count--;
        return true;
    }

    bool connected(int u, int v) {
        return find(u) == find(v);
    }

    int getCount() const { return count; }
    int size() const { return static_cast<int>(parent.size()); }
};

// High-resolution benchmark timer
class Timer {
private:
    std::chrono::high_resolution_clock::time_point start_time;

public:
    Timer() { reset(); }

    void reset() {
        start_time = std::chrono::high_resolution_clock::now();
    }

    double elapsedMicroseconds() const {
        auto end_time = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::micro>(end_time - start_time).count();
    }

    double elapsedMilliseconds() const {
        auto end_time = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end_time - start_time).count();
    }

    double elapsedSeconds() const {
        return elapsedMilliseconds() / 1000.0;
    }
};

// Standard formatting function for Window-CC output
inline std::string formatCCs(int ts, int te, const std::vector<std::vector<int>>& ccs) {
    std::stringstream ans;
    ans << "The spanned connected components in [" << ts << ", " << te << "] are:\n";
    for (const auto& cc : ccs) {
        ans << "{ ";
        for (int v : cc) ans << v << " ";
        ans << "}\n";
    }
    return ans.str();
}

#endif // COMMON_H
