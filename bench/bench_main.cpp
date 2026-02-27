/**
 * zjson performance benchmark
 * 
 * Measures: parse throughput, toString throughput, deep-copy throughput
 * Datasets: ~10KB, ~100KB, ~1MB generated JSON
 * No third-party dependencies — uses std::chrono for timing.
 *
 * Build:
 *   cl /std:c++17 /O2 /EHsc /I../src bench_main.cpp /Fe:bench.exe
 * or via CMake (added as bench_zjson target).
 */

#include "../src/zjson.hpp"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

using namespace ZJSON;
using Clock = std::chrono::high_resolution_clock;

// ---------- dataset generators ----------

static std::string gen_flat_object(int num_keys) {
    // {"k0":0,"k1":"v1","k2":true,"k3":null,...}
    std::string s = "{";
    for (int i = 0; i < num_keys; ++i) {
        if (i > 0) s += ',';
        s += "\"k" + std::to_string(i) + "\":";
        switch (i % 4) {
        case 0: s += std::to_string(i); break;
        case 1: s += "\"value_" + std::to_string(i) + "\""; break;
        case 2: s += (i % 8 == 2) ? "true" : "false"; break;
        case 3: s += "null"; break;
        }
    }
    s += '}';
    return s;
}

static std::string gen_nested_array(int depth, int width) {
    // Generates nested arrays: [[1,2,...],[...],...]
    if (depth <= 0) {
        std::string s = "[";
        for (int i = 0; i < width; ++i) {
            if (i > 0) s += ',';
            s += std::to_string(i);
        }
        s += ']';
        return s;
    }
    std::string s = "[";
    for (int i = 0; i < width; ++i) {
        if (i > 0) s += ',';
        s += gen_nested_array(depth - 1, width);
    }
    s += ']';
    return s;
}

static std::string gen_mixed(int target_kb) {
    // Mix of objects and arrays until target size
    std::string s = "[";
    int idx = 0;
    while ((int)s.size() < target_kb * 1024) {
        if (idx > 0) s += ',';
        s += "{\"id\":" + std::to_string(idx);
        s += ",\"name\":\"item_" + std::to_string(idx) + "\"";
        s += ",\"active\":" + std::string(idx % 2 ? "true" : "false");
        s += ",\"score\":" + std::to_string(idx * 1.5);
        s += ",\"tags\":[\"a\",\"b\",\"c\"]";
        s += ",\"meta\":{\"x\":" + std::to_string(idx) + ",\"y\":" + std::to_string(idx * 2) + "}";
        s += '}';
        idx++;
    }
    s += ']';
    return s;
}

// ---------- benchmark runner ----------

struct BenchResult {
    std::string label;
    size_t data_bytes;
    int iterations;
    double total_ms;
    double throughput_mb_s;
};

template <typename Fn>
BenchResult run_bench(const std::string& label, size_t data_bytes, int iters, Fn&& fn) {
    // warm-up
    for (int i = 0; i < std::min(iters / 10, 3); ++i) fn();

    auto t0 = Clock::now();
    for (int i = 0; i < iters; ++i) fn();
    auto t1 = Clock::now();

    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double mb_s = (double)data_bytes * iters / (ms / 1000.0) / (1024.0 * 1024.0);

    return { label, data_bytes, iters, ms, mb_s };
}

static void print_result(const BenchResult& r) {
    std::cout << std::left << std::setw(40) << r.label
        << "  size=" << std::setw(10) << r.data_bytes
        << "  iters=" << std::setw(6) << r.iterations
        << "  time=" << std::fixed << std::setprecision(1) << std::setw(10) << r.total_ms << " ms"
        << "  throughput=" << std::fixed << std::setprecision(2) << r.throughput_mb_s << " MB/s"
        << std::endl;
}

// ---------- main ----------

int main() {
    std::cout << "=== zjson performance benchmark ===" << std::endl;
    std::cout << std::endl;

    // Generate datasets
    struct Dataset {
        std::string name;
        std::string json;
    };

    std::vector<Dataset> datasets;
    datasets.push_back({ "flat_10KB", gen_flat_object(300) });
    datasets.push_back({ "flat_100KB", gen_flat_object(3000) });
    datasets.push_back({ "mixed_10KB", gen_mixed(10) });
    datasets.push_back({ "mixed_100KB", gen_mixed(100) });
    datasets.push_back({ "mixed_1MB", gen_mixed(1000) });

    for (auto& ds : datasets) {
        std::cout << "Dataset: " << ds.name << " (" << ds.json.size() << " bytes)" << std::endl;
    }
    std::cout << std::endl;

    // Parse benchmarks
    std::cout << "--- Parse ---" << std::endl;
    for (auto& ds : datasets) {
        int iters = ds.json.size() < 50000 ? 1000 : (ds.json.size() < 500000 ? 100 : 10);
        auto r = run_bench("parse:" + ds.name, ds.json.size(), iters, [&] {
            Json j(ds.json);
            (void)j;
        });
        print_result(r);
    }
    std::cout << std::endl;

    // toString benchmarks
    std::cout << "--- ToString ---" << std::endl;
    for (auto& ds : datasets) {
        Json parsed(ds.json);
        int iters = ds.json.size() < 50000 ? 1000 : (ds.json.size() < 500000 ? 100 : 10);
        auto r = run_bench("toString:" + ds.name, ds.json.size(), iters, [&] {
            std::string s = parsed.toString();
            (void)s;
        });
        print_result(r);
    }
    std::cout << std::endl;

    // Deep copy benchmarks
    std::cout << "--- Deep Copy ---" << std::endl;
    for (auto& ds : datasets) {
        Json parsed(ds.json);
        int iters = ds.json.size() < 50000 ? 500 : (ds.json.size() < 500000 ? 50 : 5);
        auto r = run_bench("copy:" + ds.name, ds.json.size(), iters, [&] {
            Json copy = parsed;
            (void)copy;
        });
        print_result(r);
    }
    std::cout << std::endl;

    std::cout << "=== done ===" << std::endl;
    return 0;
}
