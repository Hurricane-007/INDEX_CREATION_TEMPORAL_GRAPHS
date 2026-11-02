#!/usr/bin/env python3
"""
get_data.py - Data fetching, downloading, and generation script for Window-CC experiments.

Supports:
1. Downloading real-world temporal graph datasets from KONECT used in the paper:
   - 'contact' (CT): Human interaction network (n=275, m=28,244, tmax=15,661)
   - 'mit' (MIT): Reality mining human proximity network (n=97, m=1,086,404, tmax=33,451)
2. Generating synthetic temporal graphs with controllable parameters (vertices, edges, tmax).
3. Preset sample datasets ('paper', 'small', 'medium').
4. Generating random query workloads [ts, te] with custom window size ratio (e.g. 0.8 * tmax).
"""

import os
import sys
import tarfile
import random
import urllib.request
import argparse
from pathlib import Path

KONECT_DATASETS = {
    "contact": {
        "url": "http://konect.cc/files/download.tsv.contact.tar.bz2",
        "archive_file": "download.tsv.contact.tar.bz2",
        "raw_edge_file": "contact/out.contact",
        "output_graph": "data/contact.txt",
        "output_query": "data/contact_query.txt"
    },
    "mit": {
        "url": "http://konect.cc/files/download.tsv.mit.tar.bz2",
        "archive_file": "download.tsv.mit.tar.bz2",
        "raw_edge_file": "mit/out.mit",
        "output_graph": "data/mit.txt",
        "output_query": "data/mit_query.txt"
    }
}

def ensure_dir(path):
    os.makedirs(path, exist_ok=True)

def download_file(url, target_path):
    print(f"[*] Downloading: {url}")
    headers = {"User-Agent": "Mozilla/5.0"}
    req = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(req, timeout=30) as response, open(target_path, "wb") as out_file:
        total_size = int(response.info().get("Content-Length", 0))
        downloaded = 0
        chunk_size = 64 * 1024
        while True:
            chunk = response.read(chunk_size)
            if not chunk:
                break
            out_file.write(chunk)
            downloaded += len(chunk)
            if total_size > 0:
                percent = (downloaded / total_size) * 100
                print(f"\r    Progress: {downloaded}/{total_size} bytes ({percent:.1f}%)", end="", flush=True)
            else:
                print(f"\r    Downloaded: {downloaded} bytes", end="", flush=True)
    print("\n[+] Download completed.")

def normalize_konect_tsv(raw_path, output_path):
    """
    Parses KONECT TSV format, ignores header/comment lines,
    maps vertices to 0-based indices, and normalizes timestamps
    to consecutive integers starting from 0.
    """
    print(f"[*] Normalizing graph data from {raw_path} -> {output_path}...")
    edges = []
    with open(raw_path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("%") or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) >= 3:
                try:
                    u = int(parts[0])
                    v = int(parts[1])
                    # timestamp is typically the last column
                    t = int(parts[-1])
                    if u != v:
                        edges.append((u, v, t))
                except ValueError:
                    continue

    if not edges:
        raise ValueError(f"No edges could be extracted from {raw_path}")

    # Re-index vertices to 0..n-1
    vertex_map = {}
    cnt = 0
    for u, v, _ in edges:
        if u not in vertex_map:
            vertex_map[u] = cnt
            cnt += 1
        if v not in vertex_map:
            vertex_map[v] = cnt
            cnt += 1

    # Sort edges chronologically by timestamp
    edges.sort(key=lambda x: x[2])

    # Normalize timestamps to consecutive 0..tmax
    t_map = {}
    t_cnt = 0
    normalized_edges = []
    for u, v, t in edges:
        if t not in t_map:
            t_map[t] = t_cnt
            t_cnt += 1
        normalized_edges.append((vertex_map[u], vertex_map[v], t_map[t]))

    ensure_dir(os.path.dirname(output_path))
    with open(output_path, "w", encoding="utf-8") as f:
        for u, v, t in normalized_edges:
            f.write(f"{u} {v} {t}\n")

    tmax = t_cnt - 1
    print(f"[+] Saved {len(normalized_edges)} edges, {cnt} vertices, tmax = {tmax} to '{output_path}'.")
    return cnt, len(normalized_edges), tmax

def generate_queries(graph_path, output_query_path, num_queries=1000, window_ratio=0.8):
    """
    Generates query windows [ts, te] where (te - ts) = floor(window_ratio * tmax).
    Matches Section 6.1 of the VLDB 2026 paper.
    """
    tmax = 0
    with open(graph_path, "r", encoding="utf-8") as f:
        for line in f:
            parts = line.strip().split()
            if len(parts) >= 3:
                t = int(parts[2])
                if t > tmax:
                    tmax = t

    window_len = int(tmax * window_ratio)
    if window_len <= 0:
        window_len = max(1, tmax)

    print(f"[*] Generating {num_queries} queries (window length = {window_len}, 0.8 * {tmax}) -> {output_query_path}...")
    ensure_dir(os.path.dirname(output_query_path))
    with open(output_query_path, "w", encoding="utf-8") as f:
        for _ in range(num_queries):
            ts = random.randint(0, max(0, tmax - window_len))
            te = min(tmax, ts + window_len)
            f.write(f"{ts} {te}\n")
    print(f"[+] Saved {num_queries} queries to '{output_query_path}'.")

def handle_download(name):
    if name not in KONECT_DATASETS:
        print(f"Error: Unknown dataset '{name}'. Available: {list(KONECT_DATASETS.keys())}")
        return

    info = KONECT_DATASETS[name]
    downloads_dir = "data/raw"
    ensure_dir(downloads_dir)
    archive_path = os.path.join(downloads_dir, info["archive_file"])

    if not os.path.exists(archive_path):
        download_file(info["url"], archive_path)
    else:
        print(f"[*] Archive already exists: {archive_path}")

    # Extract
    print(f"[*] Extracting {archive_path}...")
    with tarfile.open(archive_path, "r:bz2") as tar:
        tar.extractall(downloads_dir)

    raw_path = os.path.join(downloads_dir, info["raw_edge_file"])
    if not os.path.exists(raw_path):
        # Search for out.* file in extracted folders
        candidates = list(Path(downloads_dir).rglob(f"out.{name}*"))
        if candidates:
            raw_path = str(candidates[0])
        else:
            raise FileNotFoundError(f"Could not locate raw edge file in {downloads_dir}")

    n, m, tmax = normalize_konect_tsv(raw_path, info["output_graph"])
    generate_queries(info["output_graph"], info["output_query"], num_queries=1000, window_ratio=0.8)

def generate_synthetic(num_v, num_e, tmax, out_graph, num_q=100, window_ratio=0.8, out_query=None):
    print(f"[*] Generating synthetic temporal graph: n={num_v}, m={num_e}, tmax={tmax}...")
    edges = []
    for _ in range(num_e):
        u = random.randint(0, num_v - 1)
        v = random.randint(0, num_v - 1)
        while u == v:
            v = random.randint(0, num_v - 1)
        t = random.randint(0, tmax)
        edges.append((u, v, t))

    # Sort chronologically
    edges.sort(key=lambda x: x[2])

    ensure_dir(os.path.dirname(out_graph) if os.path.dirname(out_graph) else ".")
    with open(out_graph, "w", encoding="utf-8") as f:
        for u, v, t in edges:
            f.write(f"{u} {v} {t}\n")
    print(f"[+] Saved synthetic graph to '{out_graph}'.")

    if out_query:
        generate_queries(out_graph, out_query, num_queries=num_q, window_ratio=window_ratio)

def handle_preset(preset_name):
    ensure_dir("data")
    if preset_name == "paper":
        graph_path = "data/paper_graph.txt"
        query_path = "data/paper_query.txt"
        with open(graph_path, "w", encoding="utf-8") as f:
            f.write("0 1 0\n0 2 0\n3 4 1\n4 5 2\n3 5 3\n2 3 4\n")
        with open(query_path, "w", encoding="utf-8") as f:
            f.write("0 4\n0 3\n2 4\n1 3\n")
        print(f"[+] Generated Paper Example (Figure 1a) -> '{graph_path}', '{query_path}'")
    elif preset_name == "small":
        generate_synthetic(100, 500, 50, "data/small_graph.txt", 100, 0.8, "data/small_query.txt")
    elif preset_name == "medium":
        generate_synthetic(1000, 8000, 100, "data/medium_graph.txt", 500, 0.8, "data/medium_query.txt")
    else:
        print(f"Unknown preset: {preset_name}. Choose from: paper, small, medium")

def main():
    parser = argparse.ArgumentParser(description="Data fetching & generator for Window-CC experiments.")
    parser.add_argument("--download", choices=["contact", "mit", "all"], help="Download & normalize real-world datasets from KONECT.")
    parser.add_argument("--preset", choices=["paper", "small", "medium"], help="Generate preset datasets (paper, small, medium).")
    parser.add_argument("--synthetic", action="store_true", help="Generate a custom synthetic temporal graph.")
    parser.add_argument("--vertices", type=int, default=200, help="Number of vertices for synthetic graph (default: 200).")
    parser.add_argument("--edges", type=int, default=1000, help="Number of edges for synthetic graph (default: 1000).")
    parser.add_argument("--tmax", type=int, default=50, help="Max timestamp for synthetic graph (default: 50).")
    parser.add_argument("--output", type=str, default="data/graph.txt", help="Output path for graph file.")
    parser.add_argument("--gen-queries", action="store_true", help="Generate query file for an existing graph.")
    parser.add_argument("--graph", type=str, help="Graph file path for generating queries.")
    parser.add_argument("--query-count", type=int, default=1000, help="Number of queries (default: 1000).")
    parser.add_argument("--window-ratio", type=float, default=0.8, help="Query window size ratio (default: 0.8 * tmax).")
    parser.add_argument("--query-output", type=str, default="data/query.txt", help="Output path for query file.")

    if len(sys.argv) == 1:
        parser.print_help()
        print("\n[Default Action] Generating 'paper' and 'small' presets for quick testing...")
        handle_preset("paper")
        handle_preset("small")
        return

    args = parser.parse_args()

    if args.download:
        if args.download == "all":
            for d in ["contact", "mit"]:
                handle_download(d)
        else:
            handle_download(args.download)
    elif args.preset:
        handle_preset(args.preset)
    elif args.synthetic:
        generate_synthetic(args.vertices, args.edges, args.tmax, args.output,
                           args.query_count, args.window_ratio, args.query_output)
    elif args.gen_queries:
        if not args.graph:
            print("Error: --graph must be specified with --gen-queries.")
            return
        generate_queries(args.graph, args.query_output, args.query_count, args.window_ratio)

if __name__ == "__main__":
    main()
