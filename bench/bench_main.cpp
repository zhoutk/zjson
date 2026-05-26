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
#include <iomanip>
#include <iostream>
#include <string>
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
    return { library, operation, dataset.name, dataset.json.size(), iters, ms, mb_s };
}

void print_result(const BenchResult& r) {
    std::cout << std::left << std::setw(11) << r.library
              << std::setw(10) << r.operation
              << std::setw(12) << r.dataset
              << " size=" << std::right << std::setw(8) << r.data_bytes
              << " iters=" << std::setw(5) << r.iterations
              << " time=" << std::fixed << std::setprecision(1) << std::setw(9) << r.total_ms << " ms"
              << " throughput=" << std::fixed << std::setprecision(2) << std::setw(9) << r.throughput_mb_s << " MB/s"
              << std::endl;
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

int main() {
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
              << std::setw(10) << "operation"
              << std::setw(12) << "dataset"
              << " size      iters time          throughput" << std::endl;
    for (const auto& result : results) {
        print_result(result);
    }

    std::cout << "sink=" << g_sink << std::endl;
    return 0;
}