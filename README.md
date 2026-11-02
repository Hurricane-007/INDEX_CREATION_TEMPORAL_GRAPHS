# Window Connected Components (Window-CC) in Large Temporal Graphs

A high-performance C++ implementation of connected component indexing and querying algorithms on temporal graphs, based on the research paper:

> **"Efficiently querying connected components in large temporal graphs via scalable and maintainable indices"**  
> *Yuyang Xia, Haoxuan Xie, Yixiang Fang, Wensheng Luo, Chenhao Ma, Dong Wen*  
> **The VLDB Journal (2026) / Proceedings of the ACM on Management of Data (SIGMOD 2023)**

This repository provides clean, mathematically rigorous C++ implementations of:
1. **U-baseline**: Compressed label-time index ($\mathcal{O}(n \log n \cdot t_{\max})$ space).
2. **TSF-index (Basic)**: Spanning forest index using window enumeration via **Algorithm 4** ($\mathcal{O}(m \cdot t_{\max})$ time, $\mathcal{O}(m)$ space).
3. **MSF-index (Optimized TSF)**: Maximum Spanning Forest index using **Link-Cut Trees (LCT)** via **Algorithm 5** ($\mathcal{O}(m \log n)$ time, $\mathcal{O}(m)$ space) with strictly $\mathcal{O}(\log n)$ dynamic maintenance (**Lemma 3.12**).
4. **MSF-construct-brute**: Traversal-based MSF replacement baseline ($\mathcal{O}(m \cdot n)$ time) for ablation analysis.

---

## 1. Problem Formulation

Consider an undirected temporal graph $G = (V, E)$, with $|V| = n$, $|E| = m$. Each edge $e \in E$ is a triplet $(u, v, t)$ with interaction timestamp $t \in [0, t_{\max}]$.

- **Projected Graph $G[t_s, t_e]$:** The static graph $(V, E[t_s, t_e])$ formed by all edges in $G$ whose timestamps fall in the query window $[t_s, t_e]$:
  $$E[t_s, t_e] = \{(u, v) \mid (u, v, t) \in E \wedge t \in [t_s, t_e]\}$$
- **Span-reachability ($u \sim_{[t_s, t_e]} v$):** Vertices $u$ and $v$ are span-reachable if they are connected by an undirected path in $G[t_s, t_e]$.
- **Window-CC (Definition 2.3):** A maximal set of vertices $S \subseteq V$ such that every pair of vertices $u, v \in S$ is mutually span-reachable in $[t_s, t_e]$.

---

## 2. Theoretical Complexity Guarantees (Paper vs. Code)

The table below strictly matches **Table 1** and theoretical proofs from the VLDB 2026 paper:

| Algorithm | Query Time | Index Space | Construction Time | Dynamic Maintenance | Key Mechanism |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **U-online** | $\mathcal{O}(m + n)$ | $\emptyset$ | $\emptyset$ | $\emptyset$ | Ad-hoc BFS/DFS traversal over projected edges |
| **U-baseline** | $\mathcal{O}(n \log \log n)$ | $\mathcal{O}(n \log n \cdot t_{\max})$ | $\mathcal{O}((m + n \log n)t_{\max})$ | $\mathcal{O}(n \cdot t_{\max})$ | Binary search on $\langle\text{label}, \text{time}\rangle$ pairs in $B(t_s, v)$ |
| **TSF-index (Alg 4)** | $\mathcal{O}(n + \sqrt{t_{\max}})$ | $\mathcal{O}(m)$ | $\mathcal{O}(m \cdot t_{\max})$ | $\mathcal{O}(n \cdot t_{\max})$ | Chronological TSF enumeration with 2-tier block decomposition |
| **MSF-index (Alg 5 LCT)** | $\mathcal{O}(n + \sqrt{t_{\max}})$ | $\mathcal{O}(m)$ | $\mathcal{O}(m \log n)$ | $\mathcal{O}(\log n)$ | Dynamic Maximum Spanning Forest via Splay-based Link-Cut Tree |
| **MSF-construct-brute** | $\mathcal{O}(n + \sqrt{t_{\max}})$ | $\mathcal{O}(m)$ | $\mathcal{O}(m \cdot n)$ | $\mathcal{O}(n)$ | Path search on MSF via standard BFS/DFS traversal |

---

## 3. Algorithmic Architecture

### 3.1 U-baseline Index (Section 3.1)
- For each start time $t_s \in [0, t_{\max}]$, edges with $t \ge t_s$ are processed in chronological order.
- Disjoint sets are merged with union-by-size. Intrusive linked lists enable $\mathcal{O}(1)$ component splicing and $\mathcal{O}(|S_{\text{smaller}}|)$ label updates.
- Total label changes per vertex across time is bounded by $\lfloor \log_2 n \rfloor$ (**Lemma 3.1**).
- Querying window $[t_s, t_e]$ uses binary search to find the active component label for each vertex in $\mathcal{O}(\log \log n)$ time, followed by linear BFS on $G'$.

### 3.2 TSF-Index & 2-Tier Block Decomposition (Section 3.2.1 - 3.2.3)
- **Temporal Spanning Forest (TSF):** The minimum edge set $\Psi[t_s, t_e] \subseteq E$ preserving the CCs of $G[t_s, t_e]$.
- **Overlapping Property (Lemma 3.6):** For each edge $(u, v, t)$, there exists a unique start time $\hat{t} \in [0, t]$ such that the edge belongs to $\Psi[t', t_{\max}]$ if and only if $t' \in [\hat{t}, t]$.
- **2-Tier Data Structure:**
  - Let $B = \lceil \sqrt{t_{\max}} \rceil$.
  - **Point array ($F_p[x]$):** Stores edges $(u, v, \hat{t})$ with timestamp $t = x$, sorted by $\hat{t}$ ascending.
  - **Block array ($F_b[y]$):** Stores edges $(u, v, \hat{t})$ with timestamps $t \in [y \cdot B, (y+1) \cdot B)$, sorted by $\hat{t}$ ascending.
- **Algorithm 3 (TSF-Query):**
  - Partition query range $[t_s, t_e]$ into three segments using block boundaries $l = \lfloor t_s / B \rfloor + 1$ and $r = \lfloor t_e / B \rfloor$:
    1. $t \in [t_s, l \cdot B - 1]$ retrieved from $F_p[t]$
    2. Block indices $x \in [l, r - 1]$ retrieved from $F_b[x]$
    3. $t \in [r \cdot B, t_e]$ retrieved from $F_p[t]$
  - Early-termination when $\hat{t} > t_s$ yields an optimal query time of $\mathcal{O}(n + \sqrt{t_{\max}})$.

### 3.3 Basic TSF Construction (Algorithm 4)
- Iterates through all $t_i \in [0, t_{\max}]$.
- Incrementally expands $t_j \in [t_i, t_{\max}]$ and maintains $\Psi[t_i, t_j]$ using disjoint set union.
- Edges added to $\Psi$ for the first time at $t_i$ are assigned interval start $\hat{t} = t_i$.
- Time complexity: $\mathcal{O}(m \cdot t_{\max})$, Space complexity: $\mathcal{O}(m)$.

### 3.4 Optimized MSF Construction via Link-Cut Tree (Algorithm 5)
- **Maximum Spanning Forest ($\Upsilon$):** The spanning forest with the maximum sum of edge timestamps (**Definition 3.4**).
- **Lemma 3.10:** When inserting $(u, v, t)$ chronologically:
  - If $u, v$ are connected in $\Upsilon$, let $e_{\min} = (x, y, t_{\min})$ be the edge on the unique tree path $P_{uv}$ with the minimum timestamp. The appearing interval start is $\hat{t} = t_{\min} + 1$. We delete $e_{\min}$ from $\Upsilon$ and insert $(u, v, t)$.
  - If $u, v$ are disconnected in $\Upsilon$, $\hat{t} = 0$, and we insert $(u, v, t)$ into $\Upsilon$.
- **Link-Cut Tree (LCT):**
  - Vertices $1 \dots n$ carry weight $\infty$.
  - Dynamic edge nodes $n + 1 \dots n + m$ carry weight $t$.
  - Splay tree aggregations maintain path minimum weights and node IDs.
  - Cycle detection (`findRoot`), cycle path minimum extraction (`split`), edge deletion (`cut`), and edge addition (`link`) each execute in amortized $\mathcal{O}(\log n)$ time.
- Total construction time: strictly $\mathcal{O}(m \log n)$ (**Lemma 3.11**).

### 3.5 Dynamic Edge Maintenance (Lemma 3.12)
- When a new edge $(u, v, t_{\max} + 1)$ arrives:
  - MSF maintains the dynamic state of $\Upsilon[0, t_{\max}]$.
  - Checking connectivity, querying path minimum, replacing the bottleneck edge, and updating $F_p$ / $F_b$ takes $\mathcal{O}(1)$ LCT operations.
  - Total update latency: **strictly $\mathcal{O}(\log n)$**, completely independent of $m$ and $t_{\max}$.

---

## 4. Project Directory Structure

```
D:\BTP-1\
├── include/                 # Header files
│   ├── common.h             # Shared primitives (Edge, TSFEdge, DisjointSet, Timer)
│   ├── u_baseline.h         # UBaseline class declaration
│   ├── tsf_index.h          # TSFIndex class declaration (Algorithm 4)
│   └── msf_index.h          # MSFIndex & LinkCutTree class declarations (Algorithm 5)
├── src/                     # C++ source implementations
│   ├── u_baseline.cpp       # U-baseline implementation (Algorithms 1 & 2)
│   ├── tsf_index.cpp        # TSF-index implementation (Algorithms 3 & 4)
│   ├── msf_index.cpp        # MSF-index with Link-Cut Tree & Brute ablation (Algorithms 3 & 5)
│   └── main.cpp             # Unified CLI driver (demo, benchmark, batch, REPL)
├── data/                    # Dataset directory
│   ├── paper_graph.txt      # Running example from Figure 1(a) (n=6, m=6, tmax=4)
│   ├── paper_query.txt      # Query windows for Figure 1(a)
│   ├── small_graph.txt      # Synthetic graph (n=100, m=500, tmax=50)
│   ├── small_query.txt      # Synthetic workload queries
│   ├── contact.txt          # Real-world KONECT human contact graph (n=274, m=28,244, tmax=15,661)
│   └── contact_query.txt    # 1,000 query windows (window size 0.8 * tmax)
├── scripts/                 # Utility scripts
│   └── get_data.py          # KONECT dataset downloader & query workload generator
├── results/                 # Query outputs and validation logs
│   ├── contact_results.txt  # 100% verified baseline ground truth (1,000 queries)
│   └── contact_msf.txt      # Output generated by MSF-index
├── build/                   # Compilation binaries and object files
│   └── window_cc.exe        # Main executable
├── .vscode/
│   └── tasks.json           # VS Code build task configuration
├── Makefile                 # GNU Make / MinGW build file
├── CMakeLists.txt           # CMake build configuration
└── README.md                # Comprehensive documentation
```

---

## 5. Build and Compilation Instructions

### 5.1 Windows with MSYS2 UCRT64 and CMake

Install the MSYS2 UCRT64 GCC toolchain (including `g++` and `mingw32-make`) and CMake. Make sure `C:\msys64\ucrt64\bin` is on `PATH`; CMake can locate `g++.exe` by its full path, but the compiler also needs this directory to find its helper programs.

From PowerShell in the repository root, add the toolchain directory to the current session's `PATH`, configure with the MinGW Makefiles generator, and build:

```powershell
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_CXX_COMPILER="C:\msys64\ucrt64\bin\g++.exe"
cmake --build build
```

The executable is created at `build\window_cc.exe`. The `PATH` command only affects the current PowerShell window; add `C:\msys64\ucrt64\bin` to your Windows user `Path` to make it available in new terminals and VS Code.

If `build` was previously configured with a different compiler or generator, configure into a new build directory instead (for example, replace `build` with `build-mingw` in both CMake commands and in the executable path below).

### 5.2 Using Visual Studio Code

Open the repository in VS Code after ensuring its environment includes `C:\msys64\ucrt64\bin`, then press `Ctrl+Shift+B` to run the configured build task.

---

## 6. Execution Modes and Usage

### 6.1 Paper Figure 1(a) Demonstration (`--demo`)
Validates that **U-baseline**, **TSF-index**, and **MSF-index** generate 100% identical connected components for the paper's running example:
```powershell
.\build\window_cc.exe --demo
```
**Output:**
```text
====================================================================
 [Paper Demonstration: Figure 1(a) Temporal Graph]
====================================================================
Graph: 6 vertices (v1..v6 indexed as 0..5), 6 temporal edges, tmax = 4

Construction Time:
  - U-baseline (Alg 2): 19.60 us | Size: 1080 B
  - TSF-index  (Alg 4): 13.00 us | Size: 336 B
  - MSF-index  (Alg 5): 20.60 us | Size: 336 B

--------------------------------------------------------------------
Query Window [0, 4]:
The spanned connected components in [0, 4] are:
{ 0 1 2 3 4 5 }
Verification: [UBaseline == TSF == MSF]: PASSED (100% Identical)
--------------------------------------------------------------------
Query Window [0, 3]:
The spanned connected components in [0, 3] are:
{ 0 1 2 }
{ 3 4 5 }
Verification: [UBaseline == TSF == MSF]: PASSED (100% Identical)
--------------------------------------------------------------------
Query Window [2, 4]:
The spanned connected components in [2, 4] are:
{ 0 }
{ 1 }
{ 2 3 4 5 }
Verification: [UBaseline == TSF == MSF]: PASSED (100% Identical)
====================================================================
Overall Verification Status: ALL TESTS PASSED SUCCESSFULLY!
====================================================================
```

### 6.2 Side-by-Side Algorithm Benchmark (`--benchmark`)
Executes a direct benchmark on construction time, memory usage, and dynamic edge maintenance:
```powershell
.\build\window_cc.exe data/contact.txt --benchmark
```
**Benchmark Output on KONECT `contact` dataset ($n=274, m=28,244, t_{\max}=15,661$):**
```text
====================================================================
 BENCHMARK: Comparing U-baseline, TSF-index, and MSF-index
 Dataset: data/contact.txt
====================================================================
 Graph Statistics: n = 274, m = 28244, tmax = 15661

--------------------------------------------------------------------
 Construction Performance & Memory Footprint:
--------------------------------------------------------------------
Algorithm           Complexity          Time (ms)           Memory (MB)         
--------------------------------------------------------------------
U-baseline          O((m+n log n)tmax)  996.03              157.31              
TSF-index (Alg 4)   O(m * tmax)         1154.87             1.01                
MSF-index (Alg 5)   O(m log n)          16.80               1.01                
--------------------------------------------------------------------
 Dynamic Edge Maintenance Test (inserting 1 edge at tmax + 1):
--------------------------------------------------------------------
 MSF-index (LCT update): 241.100 us  [Theoretical: O(log n)]
 TSF-index (recompute):  1439797.800 us  [Theoretical: O(n * tmax)]
====================================================================
```

### 6.3 Batch Workload Processing
Execute queries from a query file using MSF-index:
```powershell
.\build\window_cc.exe data/contact.txt data/contact_query.txt results/contact_msf.txt --method msf
```
Query performance: **1,000 queries answered in 28.82 ms** (Average: **28.82 $\mu$s/query**, **34,693 QPS**).

### 6.4 Interactive REPL Query Mode
```powershell
.\build\window_cc.exe data/small_graph.txt --method msf
```
Type `ts te` (e.g., `10 30`) to inspect connected components and retrieval latencies interactively. Enter `q` to quit. Run `.\build\window_cc.exe --help` to display all command-line options.

---

## 7. Scope of Improvement in the Algorithms

While the VLDB 2026 paper establishes elegant theoretical boundaries, analyzing the theoretical properties and physical memory architectures reveals significant avenues for both algorithmic extension and low-level optimization.

### 7.1 Adaptive Chunking & Hierarchy (Beyond Uniform $\sqrt{t_{\max}}$)
- **Current Limitation:** The paper divides the timestamp domain into static uniform blocks of size $B = \lceil \sqrt{t_{\max}} \rceil$. This assumes query window widths $\Delta t = t_e - t_s$ are uniformly distributed.
- **Problem:**
  - For narrow query windows ($\Delta t \ll \sqrt{t_{\max}}$), queries fall entirely within one or two blocks, rendering the block array $F_b$ unused while incurring overhead from block array allocation.
  - For broad query windows ($\Delta t \approx t_{\max}$), scanning $F_b$ requires traversing $\approx \sqrt{t_{\max}}$ block lists.
- **Improvement:**
  - **Dynamic Multi-Level Chunking (Hierarchical Blocks):** Implement a 2-level or logarithmic block hierarchy (akin to a Segment Tree or B-tree over the time domain). Level 0 covers spans of size $B$, Level 1 covers spans of size $B^2$, etc.
  - **Workload-Adaptive $B$:** Dynamically calibrate $B = \Theta(\sqrt{\mathbb{E}[\Delta t]})$ based on historical query logs or sliding-window averages.

### 7.2 Priority Search Trees (PST) for Massive Temporal Horizons
- **Current Limitation:** In large-scale temporal datasets (e.g., StackOverflow where $t_{\max} \approx 4.1 \times 10^7$ or Graph500 where $t_{\max} \approx 6.8 \times 10^6$), $\sqrt{t_{\max}} \approx 6,400$. Scanning $\sqrt{t_{\max}}$ lists in $F_b$ becomes a runtime bottleneck.
- **Theoretical Alternative:** Finding edges in $F(t_i, t_j)$ with $t_i \le t_s$ and $t_s \le t_j \le t_e$ is an instance of the 3-sided 2D Range Reporting problem.
- **Improvement:**
  - Utilizing a **Priority Search Tree (PST)** or **Radix Range Tree** reduces the theoretical query time from $\mathcal{O}(n + \sqrt{t_{\max}})$ to $\mathcal{O}(n + \log t_{\max} + k)$, where $k \le n - 1$.
  - While pointer chasing in tree structures introduces constant-factor cache misses on small graphs, for datasets with $t_{\max} \ge 10^7$, PST strictly dominates $\sqrt{t_{\max}}$ block decomposition.

### 7.3 Multi-Edge Compression & Temporal Deduplication
- **Current Limitation:** Temporal interaction networks (e.g., communication, financial transactions, packet traces) exhibit extreme burstiness: the same pair of vertices $(u, v)$ often interacts repeatedly within a small time window.
- **Problem:** Algorithm 5 processes each repeated interaction $(u, v, t_1), (u, v, t_2)$ as an independent edge insertion, triggering redundant LCT `split`, `cut`, and `link` operations.
- **Improvement:**
  - Pre-filter consecutive duplicate temporal edges between the same vertex pair $(u, v)$.
  - Maintain an interval-collapsed temporal representation where multiple timestamps between $(u, v)$ are coalesced, bypassing the Link-Cut Tree entirely when a newer interaction between $(u, v)$ arrives before an older interaction has been evicted from the MSF.

### 7.4 Cache-Conscious Flat-Array Link-Cut Tree Layout
- **Current Limitation:** Standard LCT implementations use pointer-heavy node representations (`ch[0]`, `ch[1]`, `fa`, `val`, `min_val`, `min_id`).
- **Memory Overhead:** For graphs with $m = 10^8$ edges, allocating $n + m$ heap nodes results in cache-thrashing during splay rotations.
- **Improvement:**
  - Structure-of-Arrays (SoA) layout: store `ch0[]`, `ch1[]`, `parent[]`, `val[]` in contiguous primitive vectors.
  - Compact 32-bit integer indexing to guarantee CPU cache line sharing for splay tree traversal.
  - Epoch-based node recycling: since an MSF maintains at most $n - 1$ active tree edges at any instant, dynamically recycle edge node IDs $1 \dots n - 1$ rather than allocating $m$ total edge nodes.

### 7.5 Zero-Allocation Querying via Rollback Disjoint-Set Union (DSU)
- **Current Limitation:** In Algorithm 3, each query constructs a `DisjointSet dsu(n)` and allocates auxiliary root maps and component vectors.
- **Problem:** When processing tens of thousands of queries per second, dynamic heap allocation dominates runtime overhead.
- **Improvement:**
  - Maintain a persistent, single-allocated DSU buffer.
  - Employ a **Rollback DSU** (using an operation stack). After extracting connected components, rollback only the modified entries in $\mathcal{O}(k)$ time, achieving **zero dynamic memory allocations per query**.

### 7.6 Lock-Free Multi-Threaded Query Serving
- **Current Limitation:** Index queries are evaluated sequentially in a single thread.
- **Observation:** Index arrays $F_p[]$ and $F_b[]$ are completely read-only once constructed. Queries with different $[t_s, t_e]$ parameters are entirely independent.
- **Improvement:**
  - Implement a thread-safe query batch dispatcher using OpenMP or modern C++ thread pools (`std::jthread`).
  - Distribute query batches across CPU cores to achieve linear speedup ($\approx 16\times\text{--}32\times$ QPS on multi-core servers).

### 7.7 Decremental Dynamic Maintenance (Edge Deletions)
- **Current Limitation:** Section 5.2 outlines a basic replacement search taking $\mathcal{O}(n + m_t)$ time per deleted edge. Arbitrary deletions in the RES-index are completely unsupported.
- **Theoretical Extension:**
  - Integrate fully dynamic connectivity frameworks (e.g., Holm-de Lichtenberg-Thorup or decremental 2-hop reachability) to support arbitrary historical edge modifications without triggering full $\mathcal{O}(m \log n)$ reconstruction.

---

## 8. Verification and Correctness Guarantee

The codebase includes verification across multiple datasets:
- **`data/paper_graph.txt`**: Verified 100% identical component partitions across U-baseline, TSF-index, and MSF-index for all query windows.
- **`data/contact.txt`**: Processed 1,000 real-world query windows ($0.8 \cdot t_{\max}$). The generated output `results/contact_msf.txt` (39,150 lines) was compared against the benchmark ground truth `contact_results.txt`: **0 differences detected (100% exact match)**.
