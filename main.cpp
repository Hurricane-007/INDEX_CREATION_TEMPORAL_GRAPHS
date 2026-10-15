#include "u_baseline.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>

void printUsage(const char* progName) {
    std::cout << "===============================================================\n";
    std::cout << " U-baseline: Connected Components in Temporal Graphs (VLDB '26)\n";
    std::cout << "===============================================================\n";
    std::cout << "Usage:\n";
    std::cout << "  " << progName << " <graph_file> [query_file] [output_file]\n";
    std::cout << "  " << progName << " --demo\n";
    std::cout << "  " << progName << " --help\n\n";
    std::cout << "Arguments:\n";
    std::cout << "  <graph_file>  Path to temporal graph file.\n";
    std::cout << "                Format: each line contains 'u v t' (integers).\n";
    std::cout << "                Comments starting with '#' or '%' are ignored.\n";
    std::cout << "  [query_file]  (Optional) Path to query file containing 'ts te' pairs.\n";
    std::cout << "                If omitted, enters interactive query mode.\n";
    std::cout << "  [output_file] (Optional) File to write query results.\n";
    std::cout << "                If omitted, writes to 'query_output.txt' (or console).\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << progName << " data/contact.txt\n";
    std::cout << "  " << progName << " data/contact.txt data/contact_query.txt results.txt\n";
    std::cout << "  " << progName << " --demo\n";
    std::cout << "===============================================================\n";
}

void runDemo() {
    std::cout << "\n[Running Demo: Paper's Example from Figure 1(a)]\n";
    std::vector<Edge> edges = {
        {0, 1, 0}, {0, 2, 0},
        {3, 4, 1},
        {4, 5, 2},
        {3, 5, 3},
        {2, 3, 4}
    };
    int n = 6;
    UBaseline index(n, edges);

    std::cout << "Graph loaded: n = " << index.getNumVertices()
              << ", m = " << index.getNumEdges()
              << ", tmax = " << index.getMaxTimestamp()
              << ", index size = " << index.size() << " bytes.\n\n";

    std::cout << "Query [0, 4] (Whole graph connected into 1 CC):\n";
    std::cout << index.solve(0, 4);

    std::cout << "Query [0, 3] (Two CCs: {0 1 2} and {3 4 5}):\n";
    std::cout << index.solve(0, 3);

    std::cout << "Query [2, 4] (CCs: {0}, {1}, {2 3 4 5}):\n";
    std::cout << index.solve(2, 4);
}

int main(int argc, char* argv[]) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
        printUsage(argv[0]);
        if (argc < 2) runDemo();
        return 0;
    }

    if (std::string(argv[1]) == "--demo") {
        runDemo();
        return 0;
    }

    std::string graphFile = argv[1];
    std::string queryFile = (argc >= 3) ? argv[2] : "";
    std::string outputFile = (argc >= 4) ? argv[3] : (queryFile.empty() ? "" : "query_output.txt");

    std::cout << "===============================================================\n";
    std::cout << " Loading Graph: " << graphFile << "\n";
    std::cout << "===============================================================\n";

    auto tStartBuild = std::chrono::high_resolution_clock::now();
    UBaseline index;
    if (!index.loadFromFile(graphFile)) {
        std::cerr << "Failed to construct index from " << graphFile << std::endl;
        return 1;
    }
    auto tEndBuild = std::chrono::high_resolution_clock::now();
    double buildTimeMs = std::chrono::duration<double, std::milli>(tEndBuild - tStartBuild).count();

    std::cout << " Graph Statistics:\n";
    std::cout << "   - Vertices (n):        " << index.getNumVertices() << "\n";
    std::cout << "   - Edges (m):           " << index.getNumEdges() << "\n";
    std::cout << "   - Max timestamp (tmax):" << index.getMaxTimestamp() << "\n";
    std::cout << " Index Statistics:\n";
    std::cout << "   - Construction Time:   " << std::fixed << std::setprecision(2) << buildTimeMs << " ms (" 
              << (buildTimeMs / 1000.0) << " s)\n";
    unsigned long long memBytes = index.size();
    std::cout << "   - Index Memory Size:   " << memBytes << " bytes ("
              << std::fixed << std::setprecision(2) << (memBytes / (1024.0 * 1024.0)) << " MB)\n";
    std::cout << "===============================================================\n";

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
        auto tStartQuery = std::chrono::high_resolution_clock::now();

        while (qin >> ts >> te) {
            std::string res = index.solve(ts, te);
            *outStream << res;
            qCount++;
        }

        auto tEndQuery = std::chrono::high_resolution_clock::now();
        double totalQueryTimeUs = std::chrono::duration<double, std::micro>(tEndQuery - tStartQuery).count();

        if (qCount > 0) {
            double avgTimeUs = totalQueryTimeUs / qCount;
            std::cout << "---------------------------------------------------------------\n";
            std::cout << " Query Performance:\n";
            std::cout << "   - Total Queries:       " << qCount << "\n";
            std::cout << "   - Total Query Time:    " << (totalQueryTimeUs / 1000.0) << " ms\n";
            std::cout << "   - Average Time/Query:  " << std::fixed << std::setprecision(3) << avgTimeUs << " us\n";
            std::cout << "   - Queries per Second:  " << std::fixed << std::setprecision(1) << (qCount / (totalQueryTimeUs / 1e6)) << " QPS\n";
            std::cout << "===============================================================\n";
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
            auto t0 = std::chrono::high_resolution_clock::now();
            auto ccs = index.query(ts, te);
            auto t1 = std::chrono::high_resolution_clock::now();
            double elapsedUs = std::chrono::duration<double, std::micro>(t1 - t0).count();

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
