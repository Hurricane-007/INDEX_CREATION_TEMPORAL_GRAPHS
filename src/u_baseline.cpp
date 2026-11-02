#include "u_baseline.h"
#include <fstream>
#include <iostream>
#include <queue>

void UBaseline::freeIndex() {
    if (B) {
        for (int ts = 0; ts <= tmax; ++ts) {
            delete [] B[ts];
        }
        delete [] B;
        B = nullptr;
    }
}

UBaseline::UBaseline() : n(0), m(0), tmax(0), B(nullptr) {}

UBaseline::UBaseline(int num_vertices, const std::vector<Edge>& edges) : n(0), m(0), tmax(0), B(nullptr) {
    construct(num_vertices, edges);
}

UBaseline::UBaseline(const std::string& filename) : n(0), m(0), tmax(0), B(nullptr) {
    loadFromFile(filename);
}

UBaseline::~UBaseline() { freeIndex(); }

UBaseline::UBaseline(UBaseline&& o) noexcept : n(o.n), m(o.m), tmax(o.tmax), B(o.B) {
    o.B = nullptr; o.n = o.m = o.tmax = 0;
}

UBaseline& UBaseline::operator=(UBaseline&& o) noexcept {
    if (this != &o) {
        freeIndex();
        n = o.n; m = o.m; tmax = o.tmax; B = o.B;
        o.B = nullptr; o.n = o.m = o.tmax = 0;
    }
    return *this;
}

bool UBaseline::loadFromFile(const std::string& filename) {
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

// Algorithm 2: U-baseline-construct in O((m + n log n) * tmax)
void UBaseline::construct(int num_vertices, const std::vector<Edge>& edges) {
    freeIndex();
    n = num_vertices;
    m = static_cast<int>(edges.size());
    tmax = 0;
    for (const auto& e : edges) tmax = std::max(tmax, e.t);

    B = new std::vector<LabelTime>*[tmax + 1]();
    for (int ts = 0; ts <= tmax; ++ts) B[ts] = new std::vector<LabelTime>[n]();

    std::vector<std::vector<std::pair<int, int>>> temporal_edges(tmax + 1);
    for (const auto& e : edges) temporal_edges[e.t].emplace_back(e.u, e.v);

    // Intrusive linked-list per component for O(1) splicing and O(|S_smaller|) vertex label updates
    std::vector<int> head(n), tail(n), next_node(n), comp_size(n), L(n);

    for (int ts = 0; ts <= tmax; ++ts) {
        for (int v = 0; v < n; ++v) {
            head[v] = tail[v] = v;
            next_node[v] = -1;
            comp_size[v] = 1;
            L[v] = v;
            B[ts][v].emplace_back(v, ts);
        }

        for (int t = ts; t <= tmax; ++t) {
            for (const auto& edge : temporal_edges[t]) {
                int u = edge.first, v = edge.second;
                if (u >= n || v >= n) continue;

                int lu = L[u], lv = L[v];
                if (lu == lv) continue;

                // Union by size (Definition 3.2): smaller component merged into larger
                if (comp_size[lu] < comp_size[lv]) std::swap(lu, lv);

                int curr = head[lv];
                while (curr != -1) {
                    L[curr] = lu;
                    auto& vec = B[ts][curr];
                    if (!vec.empty() && vec.back().time == t) vec.back().label = lu;
                    else vec.emplace_back(lu, t);
                    curr = next_node[curr];
                }

                // O(1) list splicing
                next_node[tail[lu]] = head[lv];
                tail[lu] = tail[lv];
                comp_size[lu] += comp_size[lv];
                head[lv] = tail[lv] = -1;
                comp_size[lv] = 0;
            }
        }
    }
}

// Algorithm 1: U-baseline-query in O(n log log n)
std::vector<std::vector<int>> UBaseline::query(int ts, int te) const {
    std::vector<std::vector<int>> ccs;
    if (n == 0) return ccs;
    if (ts < 0 || ts > tmax || te < ts) {
        for (int v = 0; v < n; ++v) ccs.push_back({v});
        return ccs;
    }

    int effective_te = std::min(te, tmax);

    // Build G' with edges (v, lk)
    std::vector<int> head(n, -1), to_edge, next_edge;
    to_edge.reserve(2 * n); next_edge.reserve(2 * n);

    auto add_edge = [&](int u, int v) {
        to_edge.push_back(v); next_edge.push_back(head[u]);
        head[u] = static_cast<int>(to_edge.size()) - 1;
    };

    // Binary search <lk, tk> with tk <= te and tk+1 > te in O(log log n) per vertex (Lemma 3.3)
    for (int v = 0; v < n; ++v) {
        const auto& vec = B[ts][v];
        if (vec.empty()) continue;

        int low = 0, high = static_cast<int>(vec.size()) - 1, best = 0;
        while (low <= high) {
            int mid = low + (high - low) / 2;
            if (vec[mid].time <= effective_te) { best = mid; low = mid + 1; }
            else high = mid - 1;
        }

        int lk = vec[best].label;
        if (lk != v) { add_edge(v, lk); add_edge(lk, v); }
    }

    // Compute all CCs on G' in O(n) time
    std::vector<bool> vis(n, false);
    std::queue<int> q;
    std::vector<int> cc;

    for (int v = 0; v < n; ++v) {
        if (!vis[v]) {
            vis[v] = true;
            q.push(v);
            cc.clear();
            cc.push_back(v);

            while (!q.empty()) {
                int curr = q.front(); q.pop();
                for (int e = head[curr]; e != -1; e = next_edge[e]) {
                    int nxt = to_edge[e];
                    if (!vis[nxt]) {
                        vis[nxt] = true;
                        q.push(nxt);
                        cc.push_back(nxt);
                    }
                }
            }
            std::sort(cc.begin(), cc.end());
            ccs.push_back(cc);
        }
    }
    return ccs;
}

std::string UBaseline::solve(int ts, int te) const {
    auto ccs = query(ts, te);
    return formatCCs(ts, te, ccs);
}

// Incremental edge maintenance in O(n * tmax)
void UBaseline::insertEdge(int u, int v, int t_new) {
    if (u >= n || v >= n || u == v) return;

    if (t_new > tmax) {
        auto** new_B = new std::vector<LabelTime>*[t_new + 1]();
        for (int ts = 0; ts <= tmax; ++ts) new_B[ts] = B[ts];
        for (int ts = tmax + 1; ts <= t_new; ++ts) new_B[ts] = new std::vector<LabelTime>[n]();
        delete [] B;
        B = new_B;
        tmax = t_new;
    }

    for (int ts = 0; ts < t_new; ++ts) {
        if (B[ts][u].empty() || B[ts][v].empty()) continue;
        int lu = B[ts][u].back().label, lv = B[ts][v].back().label;
        if (lu == lv) continue;

        int size_u = 0, size_v = 0;
        for (int w = 0; w < n; ++w) {
            int lbl = B[ts][w].back().label;
            if (lbl == lu) size_u++;
            else if (lbl == lv) size_v++;
        }
        if (size_u < size_v) std::swap(lu, lv);

        for (int w = 0; w < n; ++w) {
            if (B[ts][w].back().label == lv) {
                if (B[ts][w].back().time == t_new) B[ts][w].back().label = lu;
                else B[ts][w].emplace_back(lu, t_new);
            }
        }
    }

    for (int w = 0; w < n; ++w) {
        B[t_new][w].clear();
        B[t_new][w].emplace_back(w, t_new);
    }
    int rep = std::min(u, v), non_rep = std::max(u, v);
    B[t_new][non_rep].back().label = rep;
    m++;
}

// Index size in bytes: O(n log n * tmax)
unsigned long long UBaseline::size() const {
    if (!B) return 0;
    unsigned long long total_pairs = 0;
    for (int ts = 0; ts <= tmax; ++ts) {
        for (int v = 0; v < n; ++v) total_pairs += B[ts][v].size();
    }
    return total_pairs * sizeof(LabelTime) + (tmax + 1) * sizeof(std::vector<LabelTime>*) + (tmax + 1) * n * sizeof(std::vector<LabelTime>);
}
