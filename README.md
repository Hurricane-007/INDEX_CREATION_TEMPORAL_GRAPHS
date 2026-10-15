# U-baseline: Window Connected Components (Window-CC)

A concise, high-performance C++ implementation of the **U-baseline** index for querying Connected Components (CCs) over arbitrary time windows in undirected temporal graphs.

> **Reference Paper:**  
> *"Efficiently querying connected components in large temporal graphs via scalable and maintainable indices"*  
> **Authors:** Yuyang Xia, Haoxuan Xie, Yixiang Fang, Wensheng Luo, Chenhao Ma, Dong Wen  
> *The VLDB Journal (2026) / SIGMOD (2023)*

---

## 1. Problem Overview

In an undirected temporal graph $G = (V, E)$, $|V| = n$, $|E| = m$, each edge is a triplet $(u, v, t)$ indicating an interaction between vertices $u$ and $v$ at timestamp $t \in [0, t_{\max}]$.

- **Projected Graph $G[t_s, t_e]$:** The static graph formed by all edges in $G$ whose timestamps fall in the query window $[t_s, t_e]$.
- **Span-reachability ($u \sim_{[t_s, t_e]} v$):** Vertices $u$ and $v$ are span-reachable if they are connected by a path in $G[t_s, t_e]$.
- **Window-CC:** A maximal set of vertices $S \subseteq V$ such that every pair $u, v \in S$ is mutually span-reachable in $[t_s, t_e]$.

---

## 2. Theoretical Complexity Guarantees

The implementation strictly satisfies all mathematical bounds established in Section 3.1 of the paper:

| Property / Operation | Theoretical Bound | Paper Reference | Implementation Mechanism |
| :--- | :--- | :--- | :--- |
| **Index Space** | $\mathcal{O}(n \log n \cdot t_{\max})$ | **Lemma 3.2** | Only records $\langle \text{label}, \text{time} \rangle$ pairs in $B(t_s, v)$. Each vertex's label changes at most $\lfloor \log_2 n \rfloor$ times (Lemma 3.1). |
| **Construction Time** | $\mathcal{O}((m + n \log n) t_{\max})$ | **Lemma 3.4 (Alg. 2)** | For each start time $t_s$, edges with $t \ge t_s$ are processed chronologically. Intrusive linked lists enable $\mathcal{O}(1)$ component splicing and $\mathcal{O}(\|S_{\text{smaller}}\|)$ label updates. Total label updates per $t_s$ is $\le n \log_2 n$. |
| **Query Time** | $\mathcal{O}(n \log \log n)$ | **Lemma 3.3 (Alg. 1)** | Per vertex $v \in V$, binary searches the latest label at $t_e$ in $B(t_s, v)$ in $\mathcal{O}(\log \|B\|) = \mathcal{O}(\log \log n)$ time. Graph $G'$ is built with $\le n$ edges, and CCs are retrieved in $\mathcal{O}(n)$ time via linear BFS traversal. |
| **Edge Maintenance** | $\mathcal{O}(n \cdot t_{\max})$ | **Section 3.1.3** | Incrementally inserts an edge at $t_{\max} + 1$ by checking and updating components across all $t_s \in [0, t_{\max}]$ in $\mathcal{O}(n)$ time per $t_s$. |

---

## 3. Code Structure

```
d:\BTP-1\
├── u_baseline.h     # Header: Data structures, UBaseline class, complexity docs
├── u_baseline.cpp   # Implementation: Algorithms 1 & 2, file loader, maintenance
├── main.cpp         # CLI driver: File input, batch queries, interactive REPL, benchmarks
├── get_data.py      # Data script: Downloads KONECT datasets, generates synthetic data & queries
├── README.md        # Comprehensive documentation
└── data/            # Pre-generated and downloaded datasets
    ├── paper_graph.txt, paper_query.txt     # Paper Figure 1(a) running example
    ├── small_graph.txt, small_query.txt     # Synthetic graph (n=100, m=500, tmax=50)
    └── contact.txt, contact_query.txt       # Real-world KONECT dataset (n=274, m=28,244, tmax=15,661)
```

### Component Details
- **`u_baseline.h`**: Declares `LabelTime`, `Edge`, and `UBaseline`. Exposes `loadFromFile`, `construct`, `query`, `solve`, `insertEdge`, and `size`.
- **`u_baseline.cpp`**: 
  - `loadFromFile`: Parses graph lines `u v t`, ignoring `#` or `%` comments.
  - `construct`: Implements **Algorithm 2** with intrusive linked lists (`head`, `tail`, `next_node`, `comp_size`, `L`) for $\mathcal{O}(1)$ component splicing.
  - `query`: Implements **Algorithm 1** using binary search in $\mathcal{O}(\log \log n)$ per vertex, followed by $\mathcal{O}(n)$ linear CC extraction on $G'$.
  - `insertEdge`: Implements dynamic edge maintenance in $\mathcal{O}(n \cdot t_{\max})$ time.
- **`main.cpp`**: Provides command-line feeding of graph files and query files, reports graph statistics, indexing time, memory consumption, query timing ($\mu\text{s/query}$, QPS), or starts an interactive query shell.
- **`get_data.py`**: Automated script to fetch real-world graphs from KONECT, create presets, or generate synthetic graphs and query files.

---

## 4. Getting & Generating Data (`get_data.py`)

Run `python get_data.py` with any of the following options:

### 4.1 Download Real-World Datasets (from the Paper)
Downloads directly from KONECT, normalizes vertex IDs to $0 \dots n-1$, sorts timestamps, and creates 1,000 query windows with length $0.8 \cdot t_{\max}$:
```bash
# Download the 'contact' (CT) human interaction network:
python get_data.py --download contact

# Download the 'mit' (MIT) reality mining network:
python get_data.py --download mit

# Download all datasets:
python get_data.py --download all
```

### 4.2 Use Built-in Presets
```bash
# Paper's Figure 1(a) 6-vertex example:
python get_data.py --preset paper

# Small synthetic graph (100 vertices, 500 edges, tmax=50):
python get_data.py --preset small

# Medium synthetic graph (1000 vertices, 8000 edges, tmax=100):
python get_data.py --preset medium
```

### 4.3 Generate Custom Synthetic Temporal Graphs
```bash
python get_data.py --synthetic --vertices 500 --edges 2500 --tmax 80 --output data/custom.txt --query-count 200 --query-output data/custom_queries.txt
```

### 4.4 Generate Queries for Any Existing Graph
```bash
python get_data.py --gen-queries --graph data/contact.txt --query-count 1000 --window-ratio 0.8 --query-output data/contact_query.txt
```

---

## 5. Feeding Data into `main.cpp` (`ubaseline.exe`)

### 5.1 Compilation
Using MSVC on Windows:
```cmd
cl /O2 /EHsc /std:c++17 u_baseline.cpp main.cpp /Fe:ubaseline.exe
```

Using GCC on Linux / MinGW:
```bash
g++ -O3 -std=c++17 u_baseline.cpp main.cpp -o ubaseline
```

---

### 5.2 Mode 1: Batch Query File with Output File
Feed a graph file and a query file:
```cmd
.\ubaseline.exe data/contact.txt data/contact_query.txt contact_results.txt
```
**Example Console Output:**
```text
===============================================================
 Loading Graph: data/contact.txt
===============================================================
 Graph Statistics:
   - Vertices (n):        274
   - Edges (m):           28244
   - Max timestamp (tmax):15661
 Index Statistics:
   - Construction Time:   1130.02 ms (1.13 s)
   - Index Memory Size:   61835216 bytes (58.97 MB)
===============================================================
Processing queries from data/contact_query.txt -> contact_results.txt ...
---------------------------------------------------------------
 Query Performance:
   - Total Queries:       1000
   - Total Query Time:    81.56 ms
   - Average Time/Query:  81.558 us
   - Queries per Second:  12261.2 QPS
===============================================================
```
Results are saved to `contact_results.txt` in the standard benchmark format:
```text
The spanned connected components in [2929, 15457] are:
{ 0 1 2 3 ... 262 }
{ 28 }
{ 77 }
...
```

---

### 5.3 Mode 2: Interactive REPL Mode
If no query file is specified, the program loads the graph, reports statistics, and prompts for query windows $[t_s, t_e]$ interactively:
```cmd
.\ubaseline.exe data/small_graph.txt
```
**Interactive Session:**
```text
===============================================================
 Loading Graph: data/small_graph.txt
===============================================================
 Graph Statistics:
   - Vertices (n):        100
   - Edges (m):           500
   - Max timestamp (tmax):50
 Index Statistics:
   - Construction Time:   1.38 ms (0.00 s)
   - Index Memory Size:   95008 bytes (0.09 MB)
===============================================================

[Interactive Query Mode]
Enter query window 'ts te' (or 'q' to quit):

query [ts te]> 10 30
Found 14 connected components in 21.40 us:
  { 0 3 5 12 18 25 ... }
  { 1 4 9 ... }
  { 2 }
  ...

query [ts te]> q
```

---

### 5.4 Mode 3: Built-in Paper Demo
Running without arguments or with `--demo`:
```cmd
.\ubaseline.exe --demo
```
Executes the paper's running example (Figure 1a) and verifies:
- Window $[0, 4]$: `{ 0 1 2 3 4 5 }` (1 CC)
- Window $[0, 3]$: `{ 0 1 2 }`, `{ 3 4 5 }` (2 CCs)
- Window $[2, 4]$: `{ 0 }`, `{ 1 }`, `{ 2 3 4 5 }` (3 CCs)
