#include "msf_index.h"
#include <fstream>
#include <iostream>
#include <queue>
#include <algorithm>

void MSFIndex::initArrays(int max_time) {
    tmax = max_time;
    block_len = std::max(1, static_cast<int>(std::sqrt(std::max(1, tmax))));
    num_blocks = tmax / block_len + 1;
    Fp.assign(tmax + 1, std::vector<TSFEdge>());
    Fb.assign(num_blocks, std::vector<TSFEdge>());
}

MSFIndex::MSFIndex() : n(0), m(0), tmax(0), block_len(1), num_blocks(1), edge_node_count(0) {}

MSFIndex::MSFIndex(int num_vertices, const std::vector<Edge>& edges)
    : n(0), m(0), tmax(0), block_len(1), num_blocks(1), edge_node_count(0) {
    construct(num_vertices, edges);
}

MSFIndex::MSFIndex(const std::string& filename)
    : n(0), m(0), tmax(0), block_len(1), num_blocks(1), edge_node_count(0) {
    loadFromFile(filename);
}

bool MSFIndex::loadFromFile(const std::string& filename) {
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

// Algorithm 5: MSF-construct in O(m log n) time using Link-Cut Tree, O(m) space
void MSFIndex::construct(int num_vertices, const std::vector<Edge>& edges) {
    n = num_vertices;
    m = static_cast<int>(edges.size());
    int max_time = 0;
    for (const auto& e : edges) max_time = std::max(max_time, e.t);

    initArrays(max_time);

    // Sort edges chronologically
    std::vector<Edge> sorted_edges = edges;
    std::sort(sorted_edges.begin(), sorted_edges.end(), [](const Edge& a, const Edge& b) {
        return a.t < b.t;
    });

    // Initialize Link-Cut Tree:
    // Nodes 1 .. n represent vertices (with val = INF)
    // Nodes (n + 1) .. (n + m) represent dynamic edge nodes
    lct.init(n + m + 1);
    for (int i = 1; i <= n; ++i) {
        lct.setNodeVal(i, 1e9);
    }

    edge_endpoints.assign(m + 1, {0, 0});
    edge_node_count = 0;

    for (const auto& e : sorted_edges) {
        int u = e.u, v = e.v, t = e.t;
        if (u >= n || v >= n || u == v) continue;

        int u_node = u + 1;
        int v_node = v + 1;
        edge_node_count++;
        int eid = n + edge_node_count;
        edge_endpoints[edge_node_count] = {u_node, v_node};
        lct.setNodeVal(eid, t);

        lct.makeRoot(u_node);
        if (lct.findRoot(v_node) == u_node) {
            // Case 1: u and v are connected in current MSF
            auto min_info = lct.queryPathMin(u_node, v_node);
            int min_t = min_info.first;
            int min_id = min_info.second;

            if (min_t < t) {
                // Minimum timestamp edge on cycle is replaced
                int hat_ti = min_t + 1;
                int old_idx = min_id - n;
                int ou = edge_endpoints[old_idx].first;
                int ov = edge_endpoints[old_idx].second;

                lct.cut(min_id, ou);
                lct.cut(min_id, ov);

                lct.link(eid, u_node);
                lct.link(eid, v_node);

                Fp[t].emplace_back(u, v, hat_ti);
                int b = t / block_len;
                if (b < num_blocks) {
                    Fb[b].emplace_back(u, v, hat_ti);
                }
            }
        } else {
            // Case 2: u and v are not connected in current MSF
            int hat_ti = 0;
            lct.link(eid, u_node);
            lct.link(eid, v_node);

            Fp[t].emplace_back(u, v, hat_ti);
            int b = t / block_len;
            if (b < num_blocks) {
                Fb[b].emplace_back(u, v, hat_ti);
            }
        }
    }

    // Sort all arrays by ti ascending for efficient early-termination in queries
    for (int tj = 0; tj <= tmax; ++tj) {
        std::sort(Fp[tj].begin(), Fp[tj].end());
    }
    for (int b = 0; b < num_blocks; ++b) {
        std::sort(Fb[b].begin(), Fb[b].end());
    }
}

// Baseline ablation: MSF-construct-brute in O(m * n) time using tree traversal (Section 3.2.5)
void MSFIndex::constructBrute(int num_vertices, const std::vector<Edge>& edges) {
    n = num_vertices;
    m = static_cast<int>(edges.size());
    int max_time = 0;
    for (const auto& e : edges) max_time = std::max(max_time, e.t);

    initArrays(max_time);

    std::vector<Edge> sorted_edges = edges;
    std::sort(sorted_edges.begin(), sorted_edges.end(), [](const Edge& a, const Edge& b) {
        return a.t < b.t;
    });

    struct TreeEdge {
        int to;
        int t;
        int id;
    };

    std::vector<std::vector<TreeEdge>> adj(n);
    int current_edge_id = 0;

    for (const auto& e : sorted_edges) {
        int u = e.u, v = e.v, t = e.t;
        if (u >= n || v >= n || u == v) continue;

        // BFS traversal in current MSF to find path between u and v
        std::vector<int> parent_node(n, -1);
        std::vector<int> parent_edge_t(n, -1);
        std::vector<int> parent_edge_id(n, -1);
        std::queue<int> q;

        q.push(u);
        parent_node[u] = u;

        bool found = false;
        while (!q.empty()) {
            int curr = q.front(); q.pop();
            if (curr == v) {
                found = true;
                break;
            }
            for (const auto& te : adj[curr]) {
                if (parent_node[te.to] == -1) {
                    parent_node[te.to] = curr;
                    parent_edge_t[te.to] = te.t;
                    parent_edge_id[te.to] = te.id;
                    q.push(te.to);
                }
            }
        }

        if (found) {
            // Retrieve min timestamp on path
            int min_t = 1e9;
            int min_edge_id = -1;
            int min_u = -1, min_v = -1;
            int curr = v;
            while (curr != u) {
                int p = parent_node[curr];
                int edge_t = parent_edge_t[curr];
                int edge_id = parent_edge_id[curr];
                if (edge_t < min_t) {
                    min_t = edge_t;
                    min_edge_id = edge_id;
                    min_u = p;
                    min_v = curr;
                }
                curr = p;
            }

            if (min_t < t) {
                // Delete min edge from adj
                auto remove_edge = [&](int from, int to, int id) {
                    auto& vec = adj[from];
                    for (auto it = vec.begin(); it != vec.end(); ++it) {
                        if (it->to == to && it->id == id) {
                            vec.erase(it);
                            break;
                        }
                    }
                };
                remove_edge(min_u, min_v, min_edge_id);
                remove_edge(min_v, min_u, min_edge_id);

                // Add new edge
                current_edge_id++;
                adj[u].push_back({v, t, current_edge_id});
                adj[v].push_back({u, t, current_edge_id});

                int hat_ti = min_t + 1;
                Fp[t].emplace_back(u, v, hat_ti);
                int b = t / block_len;
                if (b < num_blocks) Fb[b].emplace_back(u, v, hat_ti);
            }
        } else {
            // Disconnected
            current_edge_id++;
            adj[u].push_back({v, t, current_edge_id});
            adj[v].push_back({u, t, current_edge_id});

            int hat_ti = 0;
            Fp[t].emplace_back(u, v, hat_ti);
            int b = t / block_len;
            if (b < num_blocks) Fb[b].emplace_back(u, v, hat_ti);
        }
    }

    for (int tj = 0; tj <= tmax; ++tj) std::sort(Fp[tj].begin(), Fp[tj].end());
    for (int b = 0; b < num_blocks; ++b) std::sort(Fb[b].begin(), Fb[b].end());
}

// Algorithm 3: TSF-query in O(n + sqrt(tmax)) time
std::vector<std::vector<int>> MSFIndex::query(int ts, int te) const {
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

    // Extract connected components in O(n)
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

std::string MSFIndex::solve(int ts, int te) const {
    auto ccs = query(ts, te);
    return formatCCs(ts, te, ccs);
}

// Incremental edge maintenance for t_new = tmax + 1 in strictly O(log n) time (Lemma 3.12)
void MSFIndex::insertEdge(int u, int v, int t_new) {
    if (u >= n || v >= n || u == v) return;

    if (t_new > tmax) {
        tmax = t_new;
        block_len = std::max(1, static_cast<int>(std::sqrt(std::max(1, tmax))));
        num_blocks = tmax / block_len + 1;
        Fp.resize(tmax + 1);
        Fb.resize(num_blocks);
    }

    int u_node = u + 1;
    int v_node = v + 1;
    edge_node_count++;
    int eid = n + edge_node_count;
    if (edge_node_count >= static_cast<int>(edge_endpoints.size())) {
        edge_endpoints.resize(edge_node_count + 1);
    }
    edge_endpoints[edge_node_count] = {u_node, v_node};
    lct.setNodeVal(eid, t_new);

    lct.makeRoot(u_node);
    if (lct.findRoot(v_node) == u_node) {
        auto min_info = lct.queryPathMin(u_node, v_node);
        int min_t = min_info.first;
        int min_id = min_info.second;

        if (min_t < t_new) {
            int hat_ti = min_t + 1;
            int old_idx = min_id - n;
            int ou = edge_endpoints[old_idx].first;
            int ov = edge_endpoints[old_idx].second;

            lct.cut(min_id, ou);
            lct.cut(min_id, ov);

            lct.link(eid, u_node);
            lct.link(eid, v_node);

            TSFEdge new_edge(u, v, hat_ti);
            Fp[t_new].push_back(new_edge);
            std::sort(Fp[t_new].begin(), Fp[t_new].end());

            int b = t_new / block_len;
            if (b < num_blocks) {
                Fb[b].push_back(new_edge);
                std::sort(Fb[b].begin(), Fb[b].end());
            }
        }
    } else {
        int hat_ti = 0;
        lct.link(eid, u_node);
        lct.link(eid, v_node);

        TSFEdge new_edge(u, v, hat_ti);
        Fp[t_new].push_back(new_edge);
        std::sort(Fp[t_new].begin(), Fp[t_new].end());

        int b = t_new / block_len;
        if (b < num_blocks) {
            Fb[b].push_back(new_edge);
            std::sort(Fb[b].begin(), Fb[b].end());
        }
    }
    m++;
}

// Index space in bytes: O(m)
unsigned long long MSFIndex::size() const {
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
