/**
 * zjson comparative performance benchmark
 *
 * Measures parse, stringify, and deep-copy throughput on generated JSON data.
 * Third-party libraries are optional: define ZJSON_BENCH_HAS_NLOHMANN,
 * ZJSON_BENCH_HAS_RAPIDJSON, and/or ZJSON_BENCH_HAS_SIMDJSON via CMake when
 * their headers/libraries are available.
 */

#include "../src/zjson.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifndef ZJSON_BENCH_HAS_NLOHMANN
#if __has_include(<nlohmann/json.hpp>)
#define ZJSON_BENCH_HAS_NLOHMANN 1
#else
#define ZJSON_BENCH_HAS_NLOHMANN 0
#endif
#endif

#ifndef ZJSON_BENCH_HAS_RAPIDJSON
#if __has_include(<rapidjson/document.h>) && __has_include(<rapidjson/stringbuffer.h>) && __has_include(<rapidjson/writer.h>)
#define ZJSON_BENCH_HAS_RAPIDJSON 1
#else
#define ZJSON_BENCH_HAS_RAPIDJSON 0
#endif
#endif

#ifndef ZJSON_BENCH_HAS_SIMDJSON
#define ZJSON_BENCH_HAS_SIMDJSON 0
#endif

#if ZJSON_BENCH_HAS_NLOHMANN
#include <nlohmann/json.hpp>
#endif

#if ZJSON_BENCH_HAS_RAPIDJSON
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#endif

#if ZJSON_BENCH_HAS_SIMDJSON
#include <simdjson.h>
#endif

using Clock = std::chrono::high_resolution_clock;

namespace {

volatile size_t g_sink = 0;

void consume(size_t value) {
    g_sink += value;
}

std::string gen_flat_object(int num_keys) {
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

std::string gen_mixed(int target_kb) {
    std::string s = "[";
    int idx = 0;
    while (static_cast<int>(s.size()) < target_kb * 1024) {
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

struct Dataset {
    std::string name;
    std::string json;
};

struct BenchResult {
    std::string category;
    std::string library;
    std::string operation;
    std::string dataset;
    size_t data_bytes = 0;
    int iterations = 0;
    double total_ms = 0.0;
    double throughput_mb_s = 0.0;
};

template <typename Fn>
BenchResult run_bench(const std::string& library, const std::string& operation, const Dataset& dataset, int iters, Fn&& fn) {
    const int warmups = std::min(std::max(iters / 10, 1), 5);
    for (int i = 0; i < warmups; ++i) fn();

    const auto t0 = Clock::now();
    for (int i = 0; i < iters; ++i) fn();
    const auto t1 = Clock::now();

    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    const double mb_s = static_cast<double>(dataset.json.size()) * iters / (ms / 1000.0) / (1024.0 * 1024.0);
    return { "comparative", library, operation, dataset.name, dataset.json.size(), iters, ms, mb_s };
}

template <typename Fn>
BenchResult run_hotspot_bench(const std::string& operation, const Dataset& dataset, int iters, Fn&& fn) {
    const int warmups = std::min(std::max(iters / 100, 1), 10);
    for (int i = 0; i < warmups; ++i) fn();

    const auto t0 = Clock::now();
    for (int i = 0; i < iters; ++i) fn();
    const auto t1 = Clock::now();

    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    const double mb_s = static_cast<double>(dataset.json.size()) * iters / (ms / 1000.0) / (1024.0 * 1024.0);
    return { "zjson_hotspot", "zjson", operation, dataset.name, dataset.json.size(), iters, ms, mb_s };
}

void print_result(const BenchResult& r) {
    std::cout << std::left << std::setw(11) << r.library
              << std::setw(16) << r.operation
              << std::setw(18) << r.dataset
              << " size=" << std::right << std::setw(8) << r.data_bytes
              << " iters=" << std::setw(5) << r.iterations
              << " time=" << std::fixed << std::setprecision(1) << std::setw(9) << r.total_ms << " ms"
              << " throughput=" << std::fixed << std::setprecision(2) << std::setw(9) << r.throughput_mb_s << " MB/s"
              << std::endl;
}

std::string csv_escape(const std::string& value) {
    bool quote = false;
    for (char ch : value) {
        if (ch == ',' || ch == '"' || ch == '\n' || ch == '\r') {
            quote = true;
            break;
        }
    }
    if (!quote)
        return value;

    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');
    for (char ch : value) {
        if (ch == '"')
            escaped.push_back('"');
        escaped.push_back(ch);
    }
    escaped.push_back('"');
    return escaped;
}

void write_csv(const std::string& path, const std::vector<BenchResult>& results) {
    if (path.empty())
        return;

    std::ofstream output(path, std::ios::binary);
    output << "category,library,operation,dataset,data_bytes,iterations,total_ms,throughput_mb_s\n";
    output << std::fixed << std::setprecision(6);
    for (const auto& result : results) {
        output << csv_escape(result.category) << ','
               << csv_escape(result.library) << ','
               << csv_escape(result.operation) << ','
               << csv_escape(result.dataset) << ','
               << result.data_bytes << ','
               << result.iterations << ','
               << result.total_ms << ','
               << result.throughput_mb_s << '\n';
    }
}

int parse_iters(size_t bytes) {
    return bytes < 50000 ? 1000 : (bytes < 500000 ? 100 : 10);
}

int copy_iters(size_t bytes) {
    return bytes < 50000 ? 500 : (bytes < 500000 ? 50 : 5);
}

void bench_zjson(const std::vector<Dataset>& datasets, std::vector<BenchResult>& results) {
    for (const auto& ds : datasets) {
        results.push_back(run_bench("zjson", "parse", ds, parse_iters(ds.json.size()), [&] {
            std::string err;
            ZJSON::Json parsed = ZJSON::Json::ParseJsonStrict(ds.json, err);
            consume(parsed.isError() ? 1 : parsed.getValueType().size());
        }));

        std::string err;
        ZJSON::Json parsed = ZJSON::Json::ParseJsonStrict(ds.json, err);
        results.push_back(run_bench("zjson", "stringify", ds, parse_iters(ds.json.size()), [&] {
            std::string out = parsed.toString();
            consume(out.size());
        }));

        results.push_back(run_bench("zjson", "copy", ds, copy_iters(ds.json.size()), [&] {
            ZJSON::Json copy = parsed;
            consume(copy.getValueType().size());
        }));
    }
}

void bench_zjson_stringify_hotspots(std::vector<BenchResult>& results) {
    const Dataset cleanString{ "escape_clean_128B", std::string(128, 'a') };
    const Dataset escapedString{ "escape_mixed_128B", "plain_text_with_quotes_\"_slashes_\\_and_controls_\n_\r_\t_0123456789_plain_text_with_quotes_\"_slashes_\\_and_controls_\n_\r_\t" };
    const Dataset integerNumber{ "number_integer", std::string(16, '0') };
    const Dataset floatNumber{ "number_float", std::string(16, '0') };

    results.push_back(run_hotspot_bench("escape_clean", cleanString, 200000, [&] {
        std::string out;
        out.reserve(cleanString.json.size() + 8);
        ZJSON::appendEscapedString(cleanString.json, out);
        consume(out.size());
    }));

    results.push_back(run_hotspot_bench("escape_mixed", escapedString, 200000, [&] {
        std::string out;
        out.reserve(escapedString.json.size() * 2);
        ZJSON::appendEscapedString(escapedString.json, out);
        consume(out.size());
    }));

    results.push_back(run_hotspot_bench("format_integer", integerNumber, 200000, [&] {
        std::string out;
        out.reserve(32);
        ZJSON::appendNumber(123456789.0, out);
        consume(out.size());
    }));

    results.push_back(run_hotspot_bench("format_float", floatNumber, 200000, [&] {
        std::string out;
        out.reserve(32);
        ZJSON::appendNumber(123456.789123, out);
        consume(out.size());
    }));
}

// ---------------------------------------------------------------------------
// Node pool cost diagnostics.
//
// Every Json node create/destroy goes through the shared pool, so this section
// isolates (a) the raw cost of one create+destroy pair in a tight single-threaded
// churn and (b) the same parse+stringify workload run by one thread vs several,
// which exposes contention on the pool. These are allocator diagnostics rather
// than library-vs-library comparisons, hence the separate category.
// ---------------------------------------------------------------------------
double run_concurrent_workload(const Dataset& ds, int threads, int itersPerThread) {
    auto worker = [&ds, itersPerThread]() {
        for (int i = 0; i < itersPerThread; ++i) {
            std::string err;
            ZJSON::Json parsed = ZJSON::Json::ParseJsonStrict(ds.json, err);
            std::string out = parsed.toString();
            consume(out.size() + parsed.getValueType().size());
        }
    };

    std::vector<std::thread> pool;
    pool.reserve(static_cast<size_t>(threads));
    const auto t0 = Clock::now();
    for (int t = 0; t < threads; ++t)
        pool.emplace_back(worker);
    for (auto& w : pool)
        w.join();
    const auto t1 = Clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

void bench_zjson_pool_cost(const std::vector<Dataset>& datasets, std::vector<BenchResult>& results) {
    std::cout << std::endl << "=== zjson node pool cost (allocator diagnostics) ===" << std::endl;

    // (a) tight single-threaded create/destroy churn: pure pool traffic.
    const int nodesPerRound = 20000;
    const int rounds = 20;
    auto churn = [&]() {
        ZJSON::Json doc;
        for (int i = 0; i < nodesPerRound; ++i)
            doc.add("k", i);
        consume(doc.size() + 1);
        doc.clear();
    };
    churn();                                            // warm the pool up
    const auto c0 = Clock::now();
    for (int r = 0; r < rounds; ++r)
        churn();
    const auto c1 = Clock::now();
    const double churn_ms = std::chrono::duration<double, std::milli>(c1 - c0).count();
    const double nodes = static_cast<double>(nodesPerRound) * rounds;
    std::cout << "churn            : " << std::fixed << std::setprecision(2)
              << (churn_ms * 1e6 / nodes) << " ns per node create+destroy ("
              << static_cast<long long>(nodes) << " nodes in " << std::setprecision(1)
              << churn_ms << " ms)" << std::endl;

    // (b) parse+stringify, 1 thread vs several: the ratio shows pool contention.
    const Dataset& ds = datasets.back();
    const int itersPerThread = 3;
    const unsigned hw = std::thread::hardware_concurrency();
    const int threads = static_cast<int>(hw == 0 ? 2u : std::min(4u, hw));

    const double st_ms = run_concurrent_workload(ds, 1, itersPerThread);
    const double mt_ms = run_concurrent_workload(ds, threads, itersPerThread);
    const double per_thread_mb = static_cast<double>(ds.json.size()) / (1024.0 * 1024.0);
    const double st_mb_s = per_thread_mb * itersPerThread / (st_ms / 1000.0);
    const double mt_mb_s = per_thread_mb * itersPerThread * threads / (mt_ms / 1000.0);
    const double scaling = (mt_ms > 0.0) ? (st_ms * threads / mt_ms) : 0.0;

    std::cout << "parse+stringify  : 1 thread " << std::setprecision(2) << std::setw(7) << st_mb_s
              << " MB/s | " << threads << " threads " << std::setw(7) << mt_mb_s
              << " MB/s aggregate (scaling x" << std::setprecision(2) << scaling << ")" << std::endl;

    results.push_back({ "zjson_pool", "zjson", "parse+stringify_st", ds.name,
                        ds.json.size() * static_cast<size_t>(itersPerThread), itersPerThread, st_ms, st_mb_s });
    results.push_back({ "zjson_pool", "zjson", "parse+stringify_mt", ds.name,
                        ds.json.size() * static_cast<size_t>(itersPerThread) * static_cast<size_t>(threads),
                        itersPerThread * threads, mt_ms, mt_mb_s });
}

#if ZJSON_BENCH_HAS_NLOHMANN
void bench_nlohmann(const std::vector<Dataset>& datasets, std::vector<BenchResult>& results) {
    for (const auto& ds : datasets) {
        results.push_back(run_bench("nlohmann", "parse", ds, parse_iters(ds.json.size()), [&] {
            auto parsed = nlohmann::json::parse(ds.json);
            consume(parsed.size());
        }));

        auto parsed = nlohmann::json::parse(ds.json);
        results.push_back(run_bench("nlohmann", "stringify", ds, parse_iters(ds.json.size()), [&] {
            std::string out = parsed.dump();
            consume(out.size());
        }));

        results.push_back(run_bench("nlohmann", "copy", ds, copy_iters(ds.json.size()), [&] {
            nlohmann::json copy = parsed;
            consume(copy.size());
        }));
    }
}
#endif

#if ZJSON_BENCH_HAS_RAPIDJSON
void bench_rapidjson(const std::vector<Dataset>& datasets, std::vector<BenchResult>& results) {
    for (const auto& ds : datasets) {
        results.push_back(run_bench("RapidJSON", "parse", ds, parse_iters(ds.json.size()), [&] {
            rapidjson::Document parsed;
            parsed.Parse(ds.json.data(), ds.json.size());
            consume(parsed.HasParseError() ? 1 : (parsed.IsArray() ? parsed.Size() : (parsed.IsObject() ? parsed.MemberCount() : 0)));
        }));

        rapidjson::Document parsed;
        parsed.Parse(ds.json.data(), ds.json.size());
        results.push_back(run_bench("RapidJSON", "stringify", ds, parse_iters(ds.json.size()), [&] {
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            parsed.Accept(writer);
            consume(buffer.GetSize());
        }));

        results.push_back(run_bench("RapidJSON", "copy", ds, copy_iters(ds.json.size()), [&] {
            rapidjson::Document copy;
            copy.CopyFrom(parsed, copy.GetAllocator());
            consume(copy.IsArray() ? copy.Size() : copy.MemberCount());
        }));
    }
}
#endif

#if ZJSON_BENCH_HAS_SIMDJSON
void bench_simdjson(const std::vector<Dataset>& datasets, std::vector<BenchResult>& results) {
    for (const auto& ds : datasets) {
        simdjson::padded_string padded(ds.json);
        simdjson::dom::parser parser;
        results.push_back(run_bench("simdjson", "parse", ds, parse_iters(ds.json.size()), [&] {
            simdjson::dom::element doc;
            auto error = parser.parse(padded).get(doc);
            consume(error ? 1 : doc.is_array() ? 2 : 3);
        }));
    }
}
#endif

} // namespace

int main(int argc, char* argv[]) {
    std::string csvPath;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--csv") == 0 && i + 1 < argc) {
            csvPath = argv[++i];
        }
    }

    std::vector<Dataset> datasets;
    datasets.push_back({ "flat_10KB", gen_flat_object(300) });
    datasets.push_back({ "flat_100KB", gen_flat_object(3000) });
    datasets.push_back({ "mixed_10KB", gen_mixed(10) });
    datasets.push_back({ "mixed_100KB", gen_mixed(100) });
    datasets.push_back({ "mixed_1MB", gen_mixed(1000) });

    std::cout << "=== zjson comparative benchmark ===" << std::endl;
    std::cout << "libraries: zjson=on"
              << " nlohmann=" << (ZJSON_BENCH_HAS_NLOHMANN ? "on" : "off")
              << " RapidJSON=" << (ZJSON_BENCH_HAS_RAPIDJSON ? "on" : "off")
              << " simdjson=" << (ZJSON_BENCH_HAS_SIMDJSON ? "on" : "off")
              << std::endl;

    for (const auto& ds : datasets) {
        std::cout << "dataset " << ds.name << ": " << ds.json.size() << " bytes" << std::endl;
    }
    std::cout << std::endl;

    std::vector<BenchResult> results;
    bench_zjson(datasets, results);
#if ZJSON_BENCH_HAS_NLOHMANN
    bench_nlohmann(datasets, results);
#endif
#if ZJSON_BENCH_HAS_RAPIDJSON
    bench_rapidjson(datasets, results);
#endif
#if ZJSON_BENCH_HAS_SIMDJSON
    bench_simdjson(datasets, results);
#endif

    std::cout << std::left << std::setw(11) << "library"
              << std::setw(16) << "operation"
              << std::setw(18) << "dataset"
              << " size      iters time          throughput" << std::endl;
    for (const auto& result : results) {
        print_result(result);
    }

    std::vector<BenchResult> hotspotResults;
    bench_zjson_stringify_hotspots(hotspotResults);
    std::cout << std::endl << "=== zjson stringify hotspot microbench ===" << std::endl;
    std::cout << std::left << std::setw(11) << "library"
              << std::setw(16) << "operation"
              << std::setw(18) << "dataset"
              << " size      iters time          throughput" << std::endl;
    for (const auto& result : hotspotResults) {
        print_result(result);
    }

    results.insert(results.end(), hotspotResults.begin(), hotspotResults.end());

    // Node pool cost (single-thread churn + concurrency scaling); pushes rows under
    // the "zjson_pool" category and prints its own diagnostics table.
    bench_zjson_pool_cost(datasets, results);

    write_csv(csvPath, results);
    if (!csvPath.empty())
        std::cout << "csv=" << csvPath << std::endl;

    std::cout << "sink=" << g_sink << std::endl;
    return 0;
}