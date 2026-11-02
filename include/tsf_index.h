#ifndef TSF_INDEX_H
#define TSF_INDEX_H

#include "common.h"
#include <vector>
#include <string>

class TSFIndex {
private:
    int n;
    int m;
    int tmax;
    int block_len;
    int num_blocks;

    // Point array: Fp[x] stores all edges with tj == x, sorted by ti ascending
    std::vector<std::vector<TSFEdge>> Fp;

    // Block array: Fb[y] stores all edges with tj in [y * block_len, (y + 1) * block_len), sorted by ti ascending
    std::vector<std::vector<TSFEdge>> Fb;

    // Original edges stored by timestamp for construction & maintenance
    std::vector<std::vector<std::pair<int, int>>> edges_by_time;

    void initArrays(int max_time);

public:
    TSFIndex();
    TSFIndex(int num_vertices, const std::vector<Edge>& edges);
    explicit TSFIndex(const std::string& filename);
    ~TSFIndex() = default;

    TSFIndex(const TSFIndex&) = default;
    TSFIndex& operator=(const TSFIndex&) = default;
    TSFIndex(TSFIndex&&) noexcept = default;
    TSFIndex& operator=(TSFIndex&&) noexcept = default;

    // Load graph from file (u v t) and construct TSF-index via Algorithm 4
    bool loadFromFile(const std::string& filename);

    // Algorithm 4: TSF-construct in O(m * tmax) time, O(m) space
    void construct(int num_vertices, const std::vector<Edge>& edges);

    // Algorithm 3: TSF-query in O(n + sqrt(tmax)) time
    std::vector<std::vector<int>> query(int ts, int te) const;

    // Formatted query output string
    std::string solve(int ts, int te) const;

    // Incremental edge maintenance for t_new = tmax + 1 in O(n * tmax)
    void insertEdge(int u, int v, int t_new);

    // Index space in bytes: O(m)
    unsigned long long size() const;

    int getNumVertices() const { return n; }
    int getNumEdges() const { return m; }
    int getMaxTimestamp() const { return tmax; }
    int getBlockLen() const { return block_len; }
    int getNumBlocks() const { return num_blocks; }

    const std::vector<std::vector<TSFEdge>>& getFp() const { return Fp; }
    const std::vector<std::vector<TSFEdge>>& getFb() const { return Fb; }
};

#endif // TSF_INDEX_H
