#ifndef U_BASELINE_H
#define U_BASELINE_H

#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <queue>
#include <cstdint>


struct LabelTime {
    int label;
    int time;
    LabelTime(int l = -1, int t = -1) : label(l), time(t) {}
};

struct Edge {
    int u, v, t;
    Edge(int _u = 0, int _v = 0, int _t = 0) : u(_u), v(_v), t(_t) {}
};

class UBaseline {
private:
    int n, m, tmax;
    
    std::vector<LabelTime>** B;

    void freeIndex();

public:
    UBaseline();
    UBaseline(int num_vertices, const std::vector<Edge>& edges);
    explicit UBaseline(const std::string& filename);
    ~UBaseline();

    UBaseline(const UBaseline&) = delete;
    UBaseline& operator=(const UBaseline&) = delete;
    UBaseline(UBaseline&& other) noexcept;
    UBaseline& operator=(UBaseline&& other) noexcept;
    // Load graph from file (each line: u v t) and build index
     bool loadFromFile(const std::string& filename);
    // Algorithm 2: U-baseline-construct in O((m + n log n) * tmax)
    void construct(int num_vertices, const std::vector<Edge>& edges);

    // Algorithm 1: U-baseline-query in O(n log log n)
    std::vector<std::vector<int>> query(int ts, int te) const;

    // Formatted query output string
    std::string solve(int ts, int te) const;

    // Incremental edge maintenance for t_new = tmax + 1 in O(n * tmax)
    void insertEdge(int u, int v, int t_new);

    // Index size in bytes: O(n log n * tmax)
    unsigned long long size() const;

    int getNumVertices() const { return n; }
    int getNumEdges() const { return m; }
    int getMaxTimestamp() const { return tmax; }
    const std::vector<LabelTime>& getB(int ts, int v) const { return B[ts][v]; }
};

#endif // U_BASELINE_H

