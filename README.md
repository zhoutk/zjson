# ZJSON   &emsp;&emsp;  [中文介绍](README_CN.md)

[![JSONTestSuite](https://img.shields.io/badge/JSONTestSuite-283%2F283%20(100%25)-brightgreen)](docs/jsontestsuite_results.txt)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)](https://isocpp.org/)
[![header-only](https://img.shields.io/badge/header--only-yes-success)](src/zjson.hpp)
[![license: MIT](https://img.shields.io/badge/license-MIT-green)](LICENSE)

> Conformance verified against the full [`JSONTestSuite`](https://github.com/nst/JSONTestSuite)
> `test_parsing/` corpus — **95/95** `y_` (must-accept) and **188/188** `n_` (must-reject)
> cases pass in strict mode. See [`docs/jsontestsuite_results.txt`](docs/jsontestsuite_results.txt).
>
> The bundled [`JSON_checker`](thirds/JSON-c/README) mini-suite is also covered by
> automated regression tests: **36/36** corpus files under `thirds/JSON-c/test/`
> pass with the original suite semantics (top-level object/array and 19-level nesting limit).

Recent API additions include `toString(indent)` pretty-printing, semantic `==/!=`, `begin/end/cbegin/cend` iteration with structured bindings, duplicate-key `ParseOptions`, JSON Pointer via `at("/a/b/0")`, JSON Merge Patch / JSON Patch via `mergePatch(...)` and `applyPatch(..., err)`, ADL-based `to_json` / `from_json` hooks, plus internal slab allocation and arena-backed parsed string storage.

The current Windows/MSVC Release benchmark report comparing zjson with nlohmann/json, RapidJSON, and simdjson is available in [`docs/性能测试报告.md`](docs/性能测试报告.md).

## Introduce
From node.Js back to c++. I especially miss the pleasure of using json in javascript, so try to diy one. I used many libraries, such as: rapidjson, cJson, CJsonObject, drleq cppjson, json11, etc. Zjson's data structure is greatly inspired by cJOSN. The parsing part refers to json11, thanks! Finally, because data storage needs not only to distinguish values, but also to know their types. I choose std:: variant and std:: any which supported by C++17. Finally, the C++ version is fixed at C++17. This library is designed as a single header file, not relying on any other lib than the C++ standard library.

## Design ideas  
Simple interface functions, simple use methods, flexible data structures, and support chain operations as much as possible. Realizing the simplest design using template technology. Adding a child object of Json only needs one function -- addSubitem, which automatically identifies whether it is a value or a child Json object. The Json object is stored in a linked list structure (refers to cJSON). Please see my data structure design as follows. The header and the following nodes use the same structure, which enables chained operations during index operations ([]).

## Project progress
At present, the project has completed most of functions. Please refer to the task list for details. 

task list：
- [x] constructor(Object & Array)
- [x] constructor(values)
- [x] JSON serializable constructor
- [x] copy constructor
- [x] initializer_list constructor
- [x] destructor
- [x] operator=
- [x] operator[]
- [x] contains
- [x] getValueType
- [x] getAndRemove
- [x] getAllKeys
- [x] addSubitem（add subitems & add items to array rapidly）
- [x] toString(generate josn string)
- [x] toInt、toDouble、toFalse
- [x] toVector
- [x] isError、isNull、isArray
- [x] parse - from Json string to Json object
- [x] Extend - Json
- [x] concat - Json 
- [x] push_front - Json
- [x] push_back - Json
- [x] insert - Json
- [x] clear
- [x] std::move
- [x] Remove key
- [x] Remove intger 
- [x] pop pop_back pop_front
- [x] removeFirst removeLast remove(for array)
- [x] slice
- [x] takes take
- [x] performance test and comparison harness
- [x] algorithm non recursion
- [x] slab allocator and parsed string arena
  
## Data structure

### Json node type   
> For internal use, the data type is only used inside the Json class
```
enum Type {
    Error,                //error or a invalid Json
    False,                //Json value type - false
    True,                 //Json value type - true
    Null,                 //Json value type - null
    Number,               //Json value type - number (double / int64 / uint64)
    String,               //Json value type - string
    Object,               //Json object type
    Array                 //Json object type
};
```
### Json node define
```
class Json {
    Json* brother;       //like cJSON's next
    Json* child;         //chile node, for object type
    Type type;           //node type
    NumberKind numberKind;  //which member of the numeric payload below is live
    union { double; int64_t; uint64_t; } number;   //node's numeric data
    string valueString;  //node's string data
    string name;         //node's key
}
```
## Interface
Object type, only support Object and Array.
```
enum class JsonType
{
    Object = 6,
    Array = 7
};
```
Api list
- Json(JsonType type = JsonType::Object)&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;//constructor default, can generate Object or Array
- template&lt;typename T&gt; Json(T value, string key="")&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;//value constructor
- Json(const Json& origin)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;&nbsp;//move constructor
- Json(Json&& rhs)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;&nbsp;//copy constructor
- Json(string jsonStr)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;//deserialized constructor
- explicit Json(std::initializer_list&lt;std::pair&lt;const std::string, Json&gt;&gt; values)&emsp;&emsp;&emsp;&emsp;&emsp;//initializer_list Object constructor
- Json& operator = (const Json& origin)&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;
- Json& operator = (Json&& origin)&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;
- Json operator[](const int& index)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;
- Json operator[](const string& key)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;
- template&lt;typename T&gt; bool addSubitem(T value)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;
- template&lt;typename T&gt; bool addSubitem(string name, T value)  //add a subitem
- string toString()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;
- bool isError()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;
- bool isNull()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;
- bool isObject()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;
- bool isArray()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;
- bool isNumber()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;
- bool isIntegral()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;//true when the number is stored as int64/uint64 (a JSON integer literal)
- bool isTrue()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;
- bool isFalse()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;
- int toInt()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;
- float toFloat()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;
- double toDouble()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;
- int64_t toInt64()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;//exact for integer nodes (no double round trip)
- uint64_t toUint64()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;//exact for integer nodes (no double round trip)
- bool toBool()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;
- vector&lt;Json&gt; toVector()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;
- bool extend(Json value)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;
- bool concat(Json value)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//for array object
- bool push_front(Json value)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//for array object
- bool push_back(Json value)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//for array object
- bool insert(int index, Json value)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//for array object
- void clear()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//clear child
- void remove(const string &key)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;
- bool contains(const string& key)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;
- string getValueType()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//return value's type in string
- Json getAndRemove(const string& key)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;
- std::vector<std::string> getAllKeys()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;

Additional interface (2026-09-14)

- const Json& atRef(string_view pointer)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//RFC 6901 pointer as a reference; no copy is made (error sentinel on failure)
- Json* findPtr(string_view key)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//mutable pointer to a member (direct first, then the deep fallback)
- Json* findPtrAt(string_view pointer)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//mutable pointer addressed by pointer (nullptr when absent)
- bool setAt(string pointer, const Json& value[, string& err])&emsp;//write through a pointer (RFC 6902 "add" semantics; document unchanged on failure)
- template&lt;typename T&gt; bool try_get(const string& key, T& out)&emsp;//non-throwing accessor; the target is only written on success
- std::optional&lt;int/double/string&gt; try_int/try_double/try_string(const string& key)
- static Json array(std::initializer_list&lt;Json&gt; values)&emsp;&emsp;&emsp;&emsp;//array construction without `Json{...}` ambiguity
- std::ostream& dumpTo(std::ostream& out, int indent = 0)&emsp;&emsp;&emsp;//stream the document instead of building the text first
- static Json ParseJson(std::string&& input, std::string& errMsg)&emsp;//takes ownership of the input buffer (no copy)

Semantics worth knowing

- `operator[]` returns a **copy** (a member's subtree is deep-copied). Use `findPtr`/`findPtrAt`/`atRef` to read or modify in place.
- The deep-search fallback used when a key is not a direct member returns the **first match in document order** (pre-order).
- Parsing limits nesting to **101 levels**; `cloneChain`/`deleteJson`/pretty printing/comparison are iterative, so a 20000-level document built through the API can be copied, printed, compared and destroyed safely.
- Equality treats members as a **multiset**: duplicate keys must match in multiplicity and value pairing (parsing itself collapses duplicates; the default policy keeps the last one).
- Structured bindings work through the ADL `get` + `std::tuple_size`/`std::tuple_element`; `std::get<N>(entry)` is intentionally not provided (adding overloads to `namespace std` for our own types would be undefined behaviour).
- A JSON **integer literal** (no fraction, no exponent) is stored exactly as `int64`/`uint64` and written back verbatim, so `{"id":9007199254740993}` round-trips; `42.0`, `42e0` and `-0` stay `double` (`isIntegral()` tells them apart). The third numeric state costs no memory - the kind tag lives in the padding that already followed `type`.
- Performance, and the comparison against nlohmann/json, RapidJSON and simdjson (throughput, node-pool cost, access-path cost, stringify hotspots): see [`docs/性能测试报告.md`](docs/性能测试报告.md), raw medians in `docs/benchmark_2026-09-15_clang64_medians.csv`.

## Thread safety and memory

1. **Node allocation and deallocation are thread safe.** Every thread owns a slab pool that is never released, so a node allocated on one thread may be freed on another (or after the allocating thread has exited). No locking is involved.
2. **A single document is not safe for concurrent readers and writers.** The lazy key index and lazy string materialisation mutate `mutable` state, so share a document across threads only while nobody is modifying it (prefer handing out copies or moving ownership).
3. **Pool residency is per thread and unbounded by design.** Each thread keeps the slabs it ever used (measured: 64 short-lived threads that each parsed a 40k-node document leave about 440 MB resident for the process lifetime). Reuse worker threads; do not create one thread per request if documents can be large.
4. **Cross-module ownership is not guaranteed.** The header inlines into every module, so a document passed across DLL boundaries and destroyed after the owning module is unloaded is unsafe.
5. Deep documents are safe to copy/print/compare/destroy (iterative traversals), but the parser still refuses nesting beyond 101 levels.
    
## Examples
```
    Json subObject{{"math", 99},{"str", "a string."}};   
    Json mulitListObj{{"fkey", false},{"strkey","ffffff"},{"num2", 9.98}, {"okey", subObject}};
    Json subArray(JsonType::Array);                 
    subArray.add({12,13,14,15});            

    Json ajson(JsonType::Object);                
    std::string data = "kevin";                     
    ajson.add("fail", false);             
    ajson.add("name", data);              
    ajson.add("school-en", "the 85th.");   
    ajson.add("age", 10);                  
    ajson.add("scores", 95.98);            
    ajson.add("nullkey", nullptr);         

    Json sub;                                  
    sub.add("math", 99);                 
    ajson.addValueJson("subJson", sub);           

    Json subArray(JsonType::Array);              
    subArray.add("I'm the first one.");   
    subArray.add("two", 2);               
    
    Json sub2;                            
    sub2.add("sb2", 222);

    subArray.addValueJson("subObj", sub2);         
    
    ajson.addValueJson("array", subArray);         

    std::cout << "ajson's string is : " << ajson.toString() << std::endl;   

    string name = ajson["name"].toString();        
    int oper = ajson["sb2"].toInt();               
    Json operArr = ajson["array"];                 
    string first = ajson["array"][0].toString();   
```
result of mulitListObj：
```
{
    "fkey": false,
    "strkey": "ffffff",
    "num2": 9.98,
    "okey": {
        "math": 99,
        "str": "a string."
    }
}
```
result of  ajson：
```
{
    "fail": false,
    "name": "kevin",
    "school-en": "the 85th.",
    "age": 10,
    "scores": 95.98,
    "nullkey": null,
    "subJson": {
        "math": 99
    },
    "array": [
        "I'm the first one.",
        2,
        {
            "sb2": 222
        }
    ]
}
```
Detailed description, please move to demo.cpp or unit test in tests catalogue.

## Implementation-Defined Behavior

The following table documents zjson's behavior on inputs where the JSON specification (RFC 8259) does not mandate a particular outcome, or where common implementations differ. These correspond to the `i_*` (implementation-defined) category in the [JSONTestSuite](https://github.com/nst/JSONTestSuite).

| Behavior | zjson | Notes |
|---|---|---|
| **Duplicate object keys** | Configurable; default keep-last | `ParseOptions::DuplicateKeyPolicy` supports `KeepFirst`, `KeepLast`, and `Reject` |
| **Number precision** | IEEE 754 `double`, plus exact `int64` / `uint64` | An integer literal with no fraction or exponent is stored exactly as `int64`, or as `uint64` when it only fits the unsigned range (`18446744073709551615` round-trips); everything else is a `double`. Anything wider than both falls back to `double` exactly as before |
| **Very large numbers** | `±Infinity` | `from_chars`/`strtod` result; no error |
| **Very small numbers** | `0.0` or denormal | `from_chars`/`strtod` result; no error |
| **Maximum nesting depth** | 100 levels | Configurable via `max_depth`; deeper input is rejected |
| **UTF-8 BOM (U+FEFF)** | Not consumed | BOM bytes cause a parse error (not treated as whitespace) |
| **Comments (`//` and `/* */`)** | Accepted in extension mode | `ParseJson()` allows comments; `ParseJsonStrict()` rejects them |
| **Trailing commas** | Rejected | `[1,]` and `{"a":1,}` produce parse errors |
| **Leading zeros** | Rejected | `012`, `-01` produce parse errors |
| **`NaN` / `Infinity` literals** | Rejected | Not valid JSON values |
| **Single-quoted strings** | Rejected | Only double-quoted strings are accepted |
| **Unquoted object keys** | Rejected | Keys must be double-quoted strings |
| **Lone surrogates in `\uXXXX`** | Encoded as-is into UTF-8 | Not rejected in extension mode; use `ParseJsonStrictUtf8()` for byte-level validation |
| **UTF-8 byte validation** | Off by default | Enable via `ParseJsonStrictUtf8()` to reject invalid byte sequences |
| **Maximum string length** | Limited by `std::string` / memory | No explicit limit |
| **Null bytes in strings** | Accepted via `\u0000` | Raw `0x00` bytes in the input stream cause string termination issues on C-string APIs |

### Parsing Modes

| API | Comments | UTF-8 validation | Use case |
|---|---|---|---|
| `ParseJson(input, err)` | Allowed | Off | General use with extensions |
| `ParseJsonStrict(input, err)` | Rejected | Off | Strict RFC 8259 structure |
| `ParseJsonStrictUtf8(input, err)` | Rejected | On | Full RFC 8259 + UTF-8 compliance |

## Project site
```
https://gitee.com/zhoutk/zjson
or
https://github.com/zhoutk/zjson
```

## run guidance
The project is built in vs2019, gcc7.5, clang12.0 success.  
```
git clone https://github.com/zhoutk/zjson
cd zjson
cmake -Bbuild .

---windows
cd build && cmake --build .

---linux & mac
cd build && make

run ctest --test-dir out/build/x64-release --output-on-failure
```

## Associated projects

> [zorm](https://gitee.com/zhoutk/zorm.git) (General Encapsulation of Relational Database)
```
https://gitee.com/zhoutk/zorm
or
https://github.com/zhoutk/zorm
```