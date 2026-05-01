# NanoDB — Mini Database Engine

> MS-CS Applied Programming — Spring 2026  
> FAST-NUCES Islamabad

## GitHub Repository
https://github.com/YOUR_USERNAME/NanoDB  ← replace with your actual link

---

## Architecture

| Layer | Component | Data Structure |
|-------|-----------|----------------|
| Memory | Buffer Pool + Pager | Fixed array + Doubly Linked List (LRU) |
| Schema | Type Engine | Polymorphism + Operator Overloading |
| Metadata | System Catalog | Custom Hash Map (chaining) |
| Parser | SQL Compiler | Custom Stack (infix→postfix) + Priority Queue |
| Index | Query Optimizer | AVL Tree (self-balancing) |
| Joins | Join Optimizer | Graph + Kruskal's MST |

**No STL containers used anywhere.**

---

## How to Build

### Option A — Visual Studio (Windows)

1. Open Visual Studio Community
2. File → Open → CMake → select `CMakeLists.txt`
3. Build → Build All (or press F7)
4. Executables appear in `build/` or `out/build/`

### Option B — Command Line (Windows)

```bat
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Option C — Linux / macOS

```bash
mkdir build && cd build
cmake ..
make
```

---

## How to Run

### Run all 7 demo test cases (A–G):
```
./build/nanodb
```

### Run the automated test runner with queries.txt:
```
./build/test_runner tests/queries.txt
```

Both executables write output to:
- **Console** — real-time execution log
- **nanodb_execution.log** — full structured log file for demo evaluation

---

## Key Log Lines (Demo Evidence)

The log file will contain entries like:
```
[LOG] Page 42 evicted via LRU, written to disk
[LOG] Infix "c_acctbal > 5000" converted to Postfix "c_acctbal 5000 >"
[LOG] Multi-table join routed via MST: customer -> orders -> lineitem
[LOG] AVL: found key=42 in 7 steps (O(log N))
[BENCHMARK] Sequential scan: 200 rows in 0.412 ms
[BENCHMARK] AVL index search: found=YES in 0.003 ms
[BENCHMARK] Speedup: 137.33x
[LOG] PQ: dequeued query (table='customer' priority=10) — executing next
[LOG] Table 'customer' persisted (205 rows)
```

---

## File Structure

```
NanoDB/
├── include/
│   ├── schema.h        ← Field types, Row, TableSchema (polymorphism)
│   ├── containers.h    ← Stack<T>, Array<T>, FixedStr (no STL)
│   ├── pager.h         ← Doubly Linked List LRU + Buffer Pool
│   ├── catalog.h       ← Hash Map System Catalog
│   ├── parser.h        ← Tokenizer, Infix→Postfix, Priority Queue
│   ├── avl_tree.h      ← AVL Tree index (O log N)
│   ├── graph.h         ← Graph + Kruskal's MST join optimizer
│   └── engine.h        ← Query execution engine (ties everything together)
├── src/
│   └── main.cpp        ← Entry point + all 7 demo test cases
├── tests/
│   ├── test_runner.cpp ← Automated test runner
│   └── queries.txt     ← 50 TPC-H workload queries
├── CMakeLists.txt
├── .gitignore
└── README.md
```
