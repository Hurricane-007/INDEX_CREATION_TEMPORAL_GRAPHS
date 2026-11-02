#include "u_baseline.h"
#include "tsf_index.h"
#include "msf_index.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>

enum class AlgorithmType {
    UBaseline,
    TSF,
    MSF,
    All
};

void printUsage(const char* progName) {
    std::cout << "====================================================================\n";
    std::cout << " Window-CC: Connected Components in Temporal Graphs (VLDB '26)\n";
    std::cout << " Algorithms: U-baseline | TSF-index (Alg 4) | MSF-index (Alg 5 LCT)\n";
    std::cout << "====================================================================\n";
    std::cout << "Usage:\n";
    std::cout << "  " << progName << " <graph_file> [query_file] [output_file] [--method <name>]\n";
    std::cout << "  " << progName << " <graph_file> --benchmark\n";
    std::cout << "  " << progName << " --demo\n";
    std::cout << "  " << progName << " --help\n\n";
    std::cout << "Options:\n";
    std::cout << "  --method <name>   Choose index: 'ubaseline', 'tsf', 'msf' (default: 'msf'), 'all'\n";
    std::cout << "  --benchmark       Run comparative benchmark across all algorithms\n";
    std::cout << "  --demo            Run paper Figure 1(a) example across all three algorithms\n";
    std::cout << "  --help, -h        Show this help message\n\n";
    std::cout << "Arguments:\n";
    std::cout << "  <graph_file>      Temporal graph file (each line: 'u v t').\n";
    std::cout << "  [query_file]      (Optional) Query file with 'ts te' pairs. If omitted, opens REPL.\n";
    std::cout << "  [output_file]     (Optional) Destination file for query results.\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << progName << " data/contact.txt data/contact_query.txt results/contact_out.txt --method msf\n";
    std::cout << "  " << progName << " data/paper_graph.txt --benchmark\n";
    std::cout << "  " << progName << " --demo\n";
    std::cout << "====================================================================\n";
}

void runDemo() {
    std::cout << "\n====================================================================\n";
    std::cout << " [Paper Demonstration: Figure 1(a) Temporal Graph]\n";
    std::cout << "====================================================================\n";
    std::vector<Edge> edges = {
        {0, 1, 0}, {0, 2, 0},
        {3, 4, 1},
        {4, 5, 2},
        {3, 5, 3},
        {2, 3, 4}
    };
    int n = 6;

    std::cout << "Graph: 6 vertices (v1..v6 indexed as 0..5), 6 temporal edges, tmax = 4\n\n";

    Timer timer;

    // 1. U-Baseline
    timer.reset();
    UBaseline ubase(n, edges);
    double ubase_time = timer.elapsedMicroseconds();

    // 2. TSF-Index (Alg 4)
    timer.reset();
    TSFIndex tsf(n, edges);
    double tsf_time = timer.elapsedMicroseconds();

    // 3. MSF-Index (Alg 5 LCT)
    timer.reset();
    MSFIndex msf(n, edges);
    double msf_time = timer.elapsedMicroseconds();

    std::cout << "Construction Time:\n";
    std::cout << "  - U-baseline (Alg 2): " << std::fixed << std::setprecision(2) << ubase_time << " us | Size: " << ubase.size() << " B\n";
    std::cout << "  - TSF-index  (Alg 4): " << tsf_time << " us | Size: " << tsf.size() << " B\n";
    std::cout << "  - MSF-index  (Alg 5): " << msf_time << " us | Size: " << msf.size() << " B\n\n";

    std::vector<std::pair<int, int>> query_windows = {
        {0, 4}, // Whole graph connected -> 1 CC
        {0, 3}, // Two CCs: {0 1 2} and {3 4 5}
        {2, 4}  // Three CCs: {0}, {1}, {2 3 4 5}
    };

    bool all_match = true;

    for (const auto& w : query_windows) {
        int ts = w.first, te = w.second;
        std::cout << "--------------------------------------------------------------------\n";
        std::cout << "Query Window [" << ts << ", " << te << "]:\n";

        std::string res_u = ubase.solve(ts, te);
        std::string res_t = tsf.solve(ts, te);
        std::string res_m = msf.solve(ts, te);

        bool match = (res_u == res_t && res_t == res_m);
        if (!match) all_match = false;

        std::cout << res_m;
        std::cout << "Verification: [UBaseline == TSF == MSF]: " << (match ? "PASSED (100% Identical)" : "FAILED") << "\n";
    }

    std::cout << "====================================================================\n";
    std::cout << "Overall Verification Status: " << (all_match ? "ALL TESTS PASSED SUCCESSFULLY!" : "MISMATCH DETECTED!") << "\n";
    std::cout << "====================================================================\n\n";
}

void runBenchmark(const std::string& graphFile) {
    std::cout << "\n====================================================================\n";
    std::cout << " BENCHMARK: Comparing U-baseline, TSF-index, and MSF-index\n";
    std::cout << " Dataset: " << graphFile << "\n";
    std::cout << "====================================================================\n";

    std::ifstream fin(graphFile);
    if (!fin.is_open()) {
        std::cerr << "Error: cannot open file: " << graphFile << std::endl;
        return;
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
    int num_vertices = max_v + 1;
    int m = static_cast<int>(edges.size());
    int tmax = 0;
    for (const auto& e : edges) tmax = std::max(tmax, e.t);

    std::cout << " Graph Statistics: n = " << num_vertices << ", m = " << m << ", tmax = " << tmax << "\n\n";

    Timer timer;

    // 1. Build MSF-Index
    timer.reset();
    MSFIndex msf(num_vertices, edges);
    double msf_build = timer.elapsedMilliseconds();
    unsigned long long msf_bytes = msf.size();

    // 2. Build TSF-Index
    timer.reset();
    TSFIndex tsf(num_vertices, edges);
    double tsf_build = timer.elapsedMilliseconds();
    unsigned long long tsf_bytes = tsf.size();

    // 3. Build U-Baseline
    double ubase_build = -1.0;
    unsigned long long ubase_bytes = 0;
    bool ubase_skipped = false;

    // Check if UBaseline might cause huge memory (e.g. tmax * n > 5e7)
    if (1ULL * num_vertices * tmax > 100000000ULL) {
        std::cout << " Note: Skipping U-baseline construction due to high theoretical space O(n log n * tmax).\n";
        ubase_skipped = true;
    } else {
        timer.reset();
        UBaseline ubase(num_vertices, edges);
        ubase_build = timer.elapsedMilliseconds();
        ubase_bytes = ubase.size();
    }

    std::cout << "--------------------------------------------------------------------\n";
    std::cout << " Construction Performance & Memory Footprint:\n";
    std::cout << "--------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(20) << "Algorithm" 
              << std::setw(20) << "Complexity" 
              << std::setw(20) << "Time (ms)" 
              << std::setw(20) << "Memory (MB)" << "\n";
    std::cout << "--------------------------------------------------------------------\n";

    if (!ubase_skipped) {
        std::cout << std::left << std::setw(20) << "U-baseline"
                  << std::setw(20) << "O((m+n log n)tmax)"
                  << std::setw(20) << std::fixed << std::setprecision(2) << ubase_build
                  << std::setw(20) << (ubase_bytes / (1024.0 * 1024.0)) << "\n";
    } else {
        std::cout << std::left << std::setw(20) << "U-baseline"
                  << std::setw(20) << "O((m+n log n)tmax)"
                  << std::setw(20) << "SKIPPED (OOM)"
                  << std::setw(20) << "N/A" << "\n";
    }

    std::cout << std::left << std::setw(20) << "TSF-index (Alg 4)"
              << std::setw(20) << "O(m * tmax)"
              << std::setw(20) << std::fixed << std::setprecision(2) << tsf_build
              << std::setw(20) << (tsf_bytes / (1024.0 * 1024.0)) << "\n";

    std::cout << std::left << std::setw(20) << "MSF-index (Alg 5)"
              << std::setw(20) << "O(m log n)"
              << std::setw(20) << std::fixed << std::setprecision(2) << msf_build
              << std::setw(20) << (msf_bytes / (1024.0 * 1024.0)) << "\n";

    // Dynamic insertion test: edge at tmax + 1
    std::cout << "--------------------------------------------------------------------\n";
    std::cout << " Dynamic Edge Maintenance Test (inserting 1 edge at tmax + 1):\n";
    std::cout << "--------------------------------------------------------------------\n";

    timer.reset();
    msf.insertEdge(0, num_vertices > 1 ? 1 : 0, tmax + 1);
    double msf_insert_us = timer.elapsedMicroseconds();
    std::cout << " MSF-index (LCT update): " << std::fixed << std::setprecision(3) 
              << msf_insert_us << " us  [Theoretical: O(log n)]\n";

    timer.reset();
    tsf.insertEdge(0, num_vertices > 1 ? 1 : 0, tmax + 1);
    double tsf_insert_us = timer.elapsedMicroseconds();
    std::cout << " TSF-index (recompute):  " << std::fixed << std::setprecision(3) 
              << tsf_insert_us << " us  [Theoretical: O(n * tmax)]\n";

    std::cout << "====================================================================\n\n";
}

int main(int argc, char* argv[]) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    if (argc < 2) {
        printUsage(argv[0]);
        runDemo();
        return 0;
    }

    std::string first_arg = argv[1];
    if (first_arg == "--help" || first_arg == "-h") {
        printUsage(argv[0]);
        return 0;
    }
    if (first_arg == "--demo") {
        runDemo();
        return 0;
    }

    std::string graphFile = first_arg;
    std::string queryFile = "";
    std::string outputFile = "";
    AlgorithmType algType = AlgorithmType::MSF;
    bool isBenchmark = false;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--benchmark") {
            isBenchmark = true;
        } else if (arg == "--method" && i + 1 < argc) {
            std::string mname = argv[++i];
            if (mname == "ubaseline") algType = AlgorithmType::UBaseline;
            else if (mname == "tsf") algType = AlgorithmType::TSF;
            else if (mname == "msf") algType = AlgorithmType::MSF;
            else if (mname == "all") algType = AlgorithmType::All;
            else {
                std::cerr << "Unknown method: " << mname << " (options: ubaseline, tsf, msf, all)\n";
                return 1;
            }
        } else if (arg == "--demo") {
            runDemo();
            return 0;
        } else if (queryFile.empty() && arg[0] != '-') {
            queryFile = arg;
        } else if (outputFile.empty() && arg[0] != '-') {
            outputFile = arg;
        }
    }

    if (isBenchmark) {
        runBenchmark(graphFile);
        return 0;
    }

    std::cout << "====================================================================\n";
    std::cout << " Loading Temporal Graph: " << graphFile << "\n";
    std::cout << " Selected Algorithm:    " 
              << (algType == AlgorithmType::MSF ? "MSF-index (Algorithm 5 LCT, O(m log n))" :
                  algType == AlgorithmType::TSF ? "TSF-index (Algorithm 4, O(m * tmax))" :
                  algType == AlgorithmType::UBaseline ? "U-baseline (Algorithm 2, O((m+n log n)tmax))" : "All Algorithms")
              << "\n";
    std::cout << "====================================================================\n";

    Timer timer;
    MSFIndex msfIndex;
    TSFIndex tsfIndex;
    UBaseline ubaseIndex;

    double buildMs = 0.0;
    unsigned long long memBytes = 0;
    int n = 0, m = 0, tmax = 0;

    if (algType == AlgorithmType::MSF) {
        timer.reset();
        if (!msfIndex.loadFromFile(graphFile)) return 1;
        buildMs = timer.elapsedMilliseconds();
        memBytes = msfIndex.size();
        n = msfIndex.getNumVertices();
        m = msfIndex.getNumEdges();
        tmax = msfIndex.getMaxTimestamp();
    } else if (algType == AlgorithmType::TSF) {
        timer.reset();
        if (!tsfIndex.loadFromFile(graphFile)) return 1;
        buildMs = timer.elapsedMilliseconds();
        memBytes = tsfIndex.size();
        n = tsfIndex.getNumVertices();
        m = tsfIndex.getNumEdges();
        tmax = tsfIndex.getMaxTimestamp();
    } else if (algType == AlgorithmType::UBaseline) {
        timer.reset();
        if (!ubaseIndex.loadFromFile(graphFile)) return 1;
        buildMs = timer.elapsedMilliseconds();
        memBytes = ubaseIndex.size();
        n = ubaseIndex.getNumVertices();
        m = ubaseIndex.getNumEdges();
        tmax = ubaseIndex.getMaxTimestamp();
    }

    std::cout << " Graph Statistics:\n";
    std::cout << "   - Vertices (n):        " << n << "\n";
    std::cout << "   - Edges (m):           " << m << "\n";
    std::cout << "   - Max timestamp (tmax):" << tmax << "\n";
    std::cout << " Index Statistics:\n";
    std::cout << "   - Construction Time:   " << std::fixed << std::setprecision(2) << buildMs << " ms (" 
              << (buildMs / 1000.0) << " s)\n";
    std::cout << "   - Index Memory Size:   " << memBytes << " bytes ("
              << std::fixed << std::setprecision(2) << (memBytes / (1024.0 * 1024.0)) << " MB)\n";
    std::cout << "====================================================================\n";

    // Mode 1: Batch queries from file
    if (!queryFile.empty()) {
        std::ifstream qin(queryFile);
        if (!qin.is_open()) {
            std::cerr << "Error: cannot open query file: " << queryFile << std::endl;
            return 1;
        }

        std::ofstream fout;
        std::ostream* outStream = &std::cout;
        if (!outputFile.empty() && outputFile != "-") {
            fout.open(outputFile);
            if (!fout.is_open()) {
                std::cerr << "Error: cannot open output file: " << outputFile << std::endl;
                return 1;
            }
            outStream = &fout;
            std::cout << "Processing queries from " << queryFile << " -> " << outputFile << " ...\n";
        }

        int qCount = 0;
        int ts, te;
        timer.reset();

        while (qin >> ts >> te) {
            std::string res;
            if (algType == AlgorithmType::MSF) res = msfIndex.solve(ts, te);
            else if (algType == AlgorithmType::TSF) res = tsfIndex.solve(ts, te);
            else if (algType == AlgorithmType::UBaseline) res = ubaseIndex.solve(ts, te);

            *outStream << res;
            qCount++;
        }

        double totalQueryTimeUs = timer.elapsedMicroseconds();

        if (qCount > 0) {
            double avgTimeUs = totalQueryTimeUs / qCount;
            std::cout << "--------------------------------------------------------------------\n";
            std::cout << " Query Performance:\n";
            std::cout << "   - Total Queries:       " << qCount << "\n";
            std::cout << "   - Total Query Time:    " << std::fixed << std::setprecision(2) << (totalQueryTimeUs / 1000.0) << " ms\n";
            std::cout << "   - Average Time/Query:  " << std::fixed << std::setprecision(3) << avgTimeUs << " us\n";
            std::cout << "   - Queries per Second:  " << std::fixed << std::setprecision(1) << (qCount / (totalQueryTimeUs / 1e6)) << " QPS\n";
            std::cout << "====================================================================\n";
        } else {
            std::cout << "No queries found in " << queryFile << "\n";
        }
    } 
    // Mode 2: Interactive REPL
    else {
        std::cout << "\n[Interactive Query Mode]\n";
        std::cout << "Enter query window 'ts te' (or 'q' to quit):\n";
        std::string line;
        while (true) {
            std::cout << "\nquery [ts te]> ";
            if (!std::getline(std::cin, line) || line == "q" || line == "quit" || line == "exit") {
                break;
            }
            if (line.empty()) continue;
            std::stringstream ss(line);
            int ts, te;
            if (!(ss >> ts >> te)) {
                std::cout << "Invalid input. Please enter two integers: ts te\n";
                continue;
            }

            timer.reset();
            std::vector<std::vector<int>> ccs;
            if (algType == AlgorithmType::MSF) ccs = msfIndex.query(ts, te);
            else if (algType == AlgorithmType::TSF) ccs = tsfIndex.query(ts, te);
            else if (algType == AlgorithmType::UBaseline) ccs = ubaseIndex.query(ts, te);
            double elapsedUs = timer.elapsedMicroseconds();

            std::cout << "Found " << ccs.size() << " connected components in " 
                      << std::fixed << std::setprecision(2) << elapsedUs << " us:\n";
            for (const auto& cc : ccs) {
                std::cout << "  { ";
                for (int v : cc) std::cout << v << " ";
                std::cout << "}\n";
            }
        }
    }

    return 0;
}
