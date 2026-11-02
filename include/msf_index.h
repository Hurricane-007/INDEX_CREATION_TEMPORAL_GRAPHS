#ifndef MSF_INDEX_H
#define MSF_INDEX_H

#include "common.h"
#include <vector>
#include <string>

// Link-Cut Tree with Splay Trees for dynamic MSF maintenance in O(log n) per operation
class LinkCutTree {
public:
    struct Node {
        int ch[2];  // Left and right child
        int fa;     // Parent
        int rev;    // Lazy reversal tag
        int val;    // Weight (timestamp for edge nodes, INF for vertex nodes)
        int min_val;// Minimum weight in subtree
        int min_id; // Node ID corresponding to min_val

        Node() : fa(0), rev(0), val(1e9), min_val(1e9), min_id(0) {
            ch[0] = ch[1] = 0;
        }
    };

private:
    std::vector<Node> tree;
    std::vector<int> stack;

    inline bool isRoot(int x) const {
        int p = tree[x].fa;
        return (tree[p].ch[0] != x && tree[p].ch[1] != x);
    }

    inline int getDir(int x) const {
        return tree[tree[x].fa].ch[1] == x ? 1 : 0;
    }

    inline void pushUp(int x) {
        tree[x].min_val = tree[x].val;
        tree[x].min_id = x;
        int l = tree[x].ch[0], r = tree[x].ch[1];
        if (l && tree[l].min_val < tree[x].min_val) {
            tree[x].min_val = tree[l].min_val;
            tree[x].min_id = tree[l].min_id;
        }
        if (r && tree[r].min_val < tree[x].min_val) {
            tree[x].min_val = tree[r].min_val;
            tree[x].min_id = tree[r].min_id;
        }
    }

    inline void pushRev(int x) {
        if (!x) return;
        tree[x].rev ^= 1;
        std::swap(tree[x].ch[0], tree[x].ch[1]);
    }

    inline void pushDown(int x) {
        if (tree[x].rev) {
            pushRev(tree[x].ch[0]);
            pushRev(tree[x].ch[1]);
            tree[x].rev = 0;
        }
    }

    void rotate(int x) {
        int y = tree[x].fa, z = tree[y].fa;
        int k = getDir(x);
        if (!isRoot(y)) {
            tree[z].ch[tree[z].ch[1] == y ? 1 : 0] = x;
        }
        tree[x].fa = z;
        tree[y].ch[k] = tree[x].ch[k ^ 1];
        if (tree[x].ch[k ^ 1]) tree[tree[x].ch[k ^ 1]].fa = y;
        tree[x].ch[k ^ 1] = y;
        tree[y].fa = x;
        pushUp(y);
        pushUp(x);
    }

    void splay(int x) {
        int top = 0;
        int cur = x;
        stack[top++] = cur;
        while (!isRoot(cur)) {
            cur = tree[cur].fa;
            stack[top++] = cur;
        }
        while (top > 0) {
            pushDown(stack[--top]);
        }
        while (!isRoot(x)) {
            int y = tree[x].fa;
            if (!isRoot(y)) {
                if (getDir(x) ^ getDir(y)) rotate(x);
                else rotate(y);
            }
            rotate(x);
        }
    }

public:
    LinkCutTree() = default;
    explicit LinkCutTree(int total_nodes) {
        init(total_nodes);
    }

    void init(int total_nodes) {
        tree.assign(total_nodes + 1, Node());
        stack.assign(total_nodes + 1, 0);
        for (int i = 1; i <= total_nodes; ++i) {
            tree[i].min_id = i;
        }
    }

    void setNodeVal(int x, int val) {
        tree[x].val = val;
        tree[x].min_val = val;
        tree[x].min_id = x;
    }

    void access(int x) {
        for (int t = 0; x; t = x, x = tree[x].fa) {
            splay(x);
            tree[x].ch[1] = t;
            pushUp(x);
        }
    }

    void makeRoot(int x) {
        access(x);
        splay(x);
        pushRev(x);
    }

    int findRoot(int x) {
        access(x);
        splay(x);
        while (tree[x].ch[0]) {
            pushDown(x);
            x = tree[x].ch[0];
        }
        splay(x);
        return x;
    }

    void split(int x, int y) {
        makeRoot(x);
        access(y);
        splay(y);
    }

    void link(int x, int y) {
        makeRoot(x);
        if (findRoot(y) != x) {
            tree[x].fa = y;
        }
    }

    void cut(int x, int y) {
        split(x, y);
        if (tree[y].ch[0] == x && !tree[x].ch[1]) {
            tree[y].ch[0] = 0;
            tree[x].fa = 0;
            pushUp(y);
        }
    }

    std::pair<int, int> queryPathMin(int u, int v) {
        split(u, v);
        return {tree[v].min_val, tree[v].min_id};
    }
};

class MSFIndex {
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

    // Maintained Link-Cut Tree for O(log n) dynamic edge maintenance
    LinkCutTree lct;
    std::vector<std::pair<int, int>> edge_endpoints; // 1-based edge index -> (u, v)
    int edge_node_count;

    void initArrays(int max_time);

public:
    MSFIndex();
    MSFIndex(int num_vertices, const std::vector<Edge>& edges);
    explicit MSFIndex(const std::string& filename);
    ~MSFIndex() = default;

    MSFIndex(const MSFIndex&) = default;
    MSFIndex& operator=(const MSFIndex&) = default;
    MSFIndex(MSFIndex&&) noexcept = default;
    MSFIndex& operator=(MSFIndex&&) noexcept = default;

    // Load graph from file (u v t) and construct index via Algorithm 5 (LCT)
    bool loadFromFile(const std::string& filename);

    // Algorithm 5: MSF-construct in O(m log n) time using Link-Cut Tree, O(m) space
    void construct(int num_vertices, const std::vector<Edge>& edges);

    // Baseline ablation: MSF-construct-brute in O(m * n) time using tree traversal (Section 3.2.5)
    void constructBrute(int num_vertices, const std::vector<Edge>& edges);

    // Algorithm 3: TSF-query in O(n + sqrt(tmax)) time
    std::vector<std::vector<int>> query(int ts, int te) const;

    // Formatted query output string
    std::string solve(int ts, int te) const;

    // Incremental edge maintenance for t_new = tmax + 1 in strictly O(log n) time (Lemma 3.12)
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

#endif // MSF_INDEX_H
