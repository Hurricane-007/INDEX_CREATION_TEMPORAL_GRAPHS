#include "tsf_index.h"
#include <fstream>
#include <iostream>
#include <map>
#include <set>

void TSFIndex::initArrays(int max_time) {
    tmax = max_time;
    block_len = std::max(1, static_cast<int>(std::sqrt(std::max(1, tmax))));
    num_blocks = tmax / block_len + 1;
    Fp.assign(tmax + 1, std::vector<TSFEdge>());
    Fb.assign(num_blocks, std::vector<TSFEdge>());
}

TSFIndex::TSFIndex() : n(0), m(0), tmax(0), block_len(1), num_blocks(1) {}

TSFIndex::TSFIndex(int num_vertices, const std::vector<Edge>& edges)
    : n(0), m(0), tmax(0), block_len(1), num_blocks(1) {
    construct(num_vertices, edges);
}

TSFIndex::TSFIndex(const std::string& filename)
    : n(0), m(0), tmax(0), block_len(1), num_blocks(1) {
    loadFromFile(filename);
}

bool TSFIndex::loadFromFile(const std::string& filename) {
    std::ifstream fin(filename);
    if (!fin.is_open()) {
        std::cerr << "Error: cannot open file: " << filename << std::endl;
        return false;
    }

    std::vector<Edge> edges;
    std::string line;
    int max_v = 0;

    while (std::getline(fin, line)) {
        if (line.empty() || line[0] == '#' || line[0] == '%') continue;
        std::stringstream ss(line);
        int u, v, t;
        if (ss >> u >> v >> t) {
            if (u == v) continue;
            edges.emplace_back(u, v, t);
            max_v = std::max(max_v, std::max(u, v));
        }
    }

    if (edges.empty()) {
        std::cerr << "Warning: no edges loaded from " << filename << std::endl;
        return false;
    }

    construct(max_v + 1, edges);
    return true;
}

// Algorithm 4: TSF-construct in O(m * tmax) time, O(m) space
void TSFIndex::construct(int num_vertices, const std::vector<Edge>& edges) {
    n = num_vertices;
    m = static_cast<int>(edges.size());
    int max_time = 0;
    for (const auto& e : edges) max_time = std::max(max_time, e.t);

    initArrays(max_time);

    edges_by_time.assign(tmax + 1, std::vector<std::pair<int, int>>());
    for (const auto& e : edges) {
        if (e.u < n && e.v < n && e.u != e.v) {
            edges_by_time[e.t].emplace_back(e.u, e.v);
        }
    }

    struct StoredEdge {
        int u, v, id;
    };
    std::vector<std::vector<StoredEdge>> timed_edges(tmax + 1);
    int edge_id_gen = 0;
    for (int t = 0; t <= tmax; ++t) {
        for (const auto& e : edges_by_time[t]) {
            timed_edges[t].push_back({e.first, e.second, edge_id_gen++});
        }
    }

    int total_unique = edge_id_gen;
    std::vector<bool> in_prev_psi(total_unique, false);
    std::vector<bool> in_curr_psi(total_unique, false);

    // Algorithm 4 lines 1-9: For ti = 0 ... tmax
    DisjointSet dsu(n);
    for (int ti = 0; ti <= tmax; ++ti) {
        dsu.init(n);
        std::fill(in_curr_psi.begin(), in_curr_psi.end(), false);

        for (int tj = ti; tj <= tmax; ++tj) {
            for (const auto& ed : timed_edges[tj]) {
                int u = ed.u, v = ed.v, id = ed.id;
                if (dsu.unite(u, v)) {
                    in_curr_psi[id] = true;
                    // Line 8: if (u, v, tj) not in Psi[ti - 1, tmax]
                    if (!in_prev_psi[id]) {
                        // Belongs to F(ti, tj)
                        Fp[tj].emplace_back(u, v, ti);
                        int block_idx = tj / block_len;
                        if (block_idx < num_blocks) {
                            Fb[block_idx].emplace_back(u, v, ti);
                        }
                    }
                }
            }
        }
        in_prev_psi = in_curr_psi;
    }

    // Sort all arrays by ti in ascending order to enable early termination in queries
    for (int tj = 0; tj <= tmax; ++tj) {
        std::sort(Fp[tj].begin(), Fp[tj].end());
    }
    for (int b = 0; b < num_blocks; ++b) {
        std::sort(Fb[b].begin(), Fb[b].end());
    }
}

// Algorithm 3: TSF-query in O(n + sqrt(tmax)) time
std::vector<std::vector<int>> TSFIndex::query(int ts, int te) const {
    std::vector<std::vector<int>> ccs;
    if (n == 0) return ccs;
    if (ts < 0 || ts > tmax || te < ts) {
        for (int v = 0; v < n; ++v) ccs.push_back({v});
        return ccs;
    }

    int effective_te = std::min(te, tmax);
    DisjointSet dsu(n);

    // Block decomposition: l = floor(ts / sqrt(tmax)) + 1, r = floor(te / sqrt(tmax))
    int l = ts / block_len + 1;
    int r = effective_te / block_len;

    auto process_list = [&](const std::vector<TSFEdge>& edge_list) {
        for (const auto& edge : edge_list) {
            if (edge.ti <= ts) {
                dsu.unite(edge.u, edge.v);
            } else {
                // Since edge_list is sorted by ti ascending, we break early
                break;
            }
        }
    };

    if (l < r) {
        // Part 1: F(ti, tj) with ti <= ts and tj in [ts, l * block_len)
        int p1_end = std::min(effective_te, l * block_len - 1);
        for (int tj = ts; tj <= p1_end; ++tj) {
            process_list(Fp[tj]);
        }

        // Part 2: F(ti, tj) with ti <= ts and tj in [l * block_len, r * block_len) via Fb
        for (int x = l; x < r; ++x) {
            process_list(Fb[x]);
        }

        // Part 3: F(ti, tj) with ti <= ts and tj in [r * block_len, effective_te]
        int p3_start = std::max(ts, r * block_len);
        for (int tj = p3_start; tj <= effective_te; ++tj) {
            process_list(Fp[tj]);
        }
    } else {
        // ts and effective_te fall into the same or adjacent block
        for (int tj = ts; tj <= effective_te; ++tj) {
            process_list(Fp[tj]);
        }
    }

    // Extract all connected components in O(n)
    std::vector<int> root_map(n, -1);
    for (int v = 0; v < n; ++v) {
        int r_v = dsu.find(v);
        if (root_map[r_v] == -1) {
            root_map[r_v] = r_v;
        }
        if (v < root_map[r_v]) {
            root_map[r_v] = v;
        }
    }

    std::vector<std::vector<int>> comp_lists(n);
    for (int v = 0; v < n; ++v) {
        int canonical = root_map[dsu.find(v)];
        comp_lists[canonical].push_back(v);
    }

    for (int v = 0; v < n; ++v) {
        if (!comp_lists[v].empty()) {
            std::sort(comp_lists[v].begin(), comp_lists[v].end());
            ccs.push_back(std::move(comp_lists[v]));
        }
    }

    return ccs;
}

std::string TSFIndex::solve(int ts, int te) const {
    auto ccs = query(ts, te);
    return formatCCs(ts, te, ccs);
}

// Incremental edge maintenance in O(n * tmax)
void TSFIndex::insertEdge(int u, int v, int t_new) {
    if (u >= n || v >= n || u == v) return;

    if (t_new > tmax) {
        tmax = t_new;
        block_len = std::max(1, static_cast<int>(std::sqrt(std::max(1, tmax))));
        num_blocks = tmax / block_len + 1;
        Fp.resize(tmax + 1);
        Fb.resize(num_blocks);
        edges_by_time.resize(tmax + 1);
    }

    edges_by_time[t_new].emplace_back(u, v);
    m++;

    // Find appearing interval start ti_hat by checking connectivity in Psi[ti, t_new]
    int ti_hat = t_new;
    DisjointSet dsu(n);

    for (int ti = 0; ti <= t_new; ++ti) {
        dsu.init(n);
        // Connect all previous edges in [ti, t_new - 1]
        for (int tj = ti; tj < t_new; ++tj) {
            for (const auto& ed : edges_by_time[tj]) {
                dsu.unite(ed.first, ed.second);
            }
        }
        if (!dsu.connected(u, v)) {
            ti_hat = ti;
            break;
        }
    }

    TSFEdge new_edge(u, v, ti_hat);
    Fp[t_new].push_back(new_edge);
    std::sort(Fp[t_new].begin(), Fp[t_new].end());

    int b = t_new / block_len;
    if (b < num_blocks) {
        Fb[b].push_back(new_edge);
        std::sort(Fb[b].begin(), Fb[b].end());
    }
}

// Index space in bytes: O(m)
unsigned long long TSFIndex::size() const {
    unsigned long long total_bytes = 0;
    for (int t = 0; t <= tmax; ++t) {
        total_bytes += Fp[t].size() * sizeof(TSFEdge);
    }
    for (int b = 0; b < num_blocks; ++b) {
        total_bytes += Fb[b].size() * sizeof(TSFEdge);
    }
    total_bytes += (tmax + 1) * sizeof(std::vector<TSFEdge>);
    total_bytes += num_blocks * sizeof(std::vector<TSFEdge>);
    return total_bytes;
}
