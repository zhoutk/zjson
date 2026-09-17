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

Documentation map:

- **[`docs/使用指南.md`](docs/使用指南.md)** — the complete API semantics, traps and quick-reference card (**authoritative for the interface**);
- [`docs/从Qt迁移指南.md`](docs/从Qt迁移指南.md) — migration from `QJsonDocument` / `QJsonObject`;
- [`docs/多线程使用指南.md`](docs/多线程使用指南.md) — the threading contract, whether an external lock is enough, and the measured cost of each pattern;
- [`docs/性能测试报告.md`](docs/性能测试报告.md) — benchmark results against nlohmann / RapidJSON / simdjson.

## Introduce
From node.Js back to c++. I especially miss the pleasure of using json in javascript, so try to diy one. I used many libraries, such as: rapidjson, cJson, CJsonObject, drleq cppjson, json11, etc. Zjson's data structure is greatly inspired by cJOSN. The parsing part refers to json11, thanks! Finally, because data storage needs not only to distinguish values, but also to know their types, the storage settled on a **type tag + union** (the three number states share 8 bytes; strings switch between an owned buffer and a borrowed arena view) - no inheritance, no virtual functions. The C++ version is fixed at C++17. This library is designed as a single header file, not relying on any other lib than the C++ standard library.

## Design ideas  
Simple interface functions, simple use methods, flexible data structures, and support chain operations as much as possible. Realizing the simplest design using template technology. Adding a child object of Json only needs one function -- `add`, which automatically identifies whether it is a value or a child Json object. The Json object is stored in a linked list structure (refers to cJSON). Please see my data structure design as follows. The header and the following nodes use the same structure, which enables chained operations during index operations ([]).

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
- [x] take / takes (get + remove; formerly getAndRemove)
- [x] getAllKeys
- [x] add (add members to an object / items to an array rapidly; formerly addSubitem)
- [x] toString(generate josn string)
- [x] toInt、toDouble、toBool
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
- [x] three-state Number (exact int64 / uint64 / double storage)
- [x] thread safety: concurrent reads of one document + cross-thread node lifetime (see [`docs/多线程使用指南.md`](docs/多线程使用指南.md))
- [x] Qt keyword-macro coexistence (`slots`/`signals`/`foreach` no longer collide; see `tests/test_qt_macro_compat.cpp`)
- [x] direct-child & safe-mutation helper block (`directChild`/`hasChild`/`childValueOr`/`ownedKey`/`memberCount`/`isEmptyObject`/`setElement`/`setChild`)
  
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
    Json* brother;       //sibling link (like cJSON's next): the next member/element; the name is meaningful on object members only
    Json* child;         //first child node, valid for object/array types
    Json* lastChild;     //tail of the child chain, so append is O(1)
    atomic<Index*> keymap; //lazy per-object key index (CAS-published by const readers; see the threading guide)
    Type type;           //node type
    NumberKind numberKind;  //which member of the numeric payload below is live
    union { double; int64_t; uint64_t; } number;   //node's numeric data (8 bytes, three states)
    StoredString valueString;  //node's string data (owned, or a borrowed arena view)
    StoredString name;         //node's key (object member name)
}
```
> Note: `valueString` / `name` are the internal `detail::StoredString` (a tagged union of an owned
> `std::string` and a view into the parse arena), not a plain `std::string`; strings are materialized only
> when needed, which keeps `sizeof(Json)` at 128 bytes. Object names are owned while they fit
> `std::string`'s inline buffer and borrow the arena when longer - neither case materializes in `key()`.
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
- template&lt;typename T&gt; Json(const T& value)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//value constructor (arithmetic; ADL `to_json` types take the other overload)
- Json(const float&) / Json(const double&) / Json(const bool&) / Json(const std::nullptr_t&)&emsp;//literal constructors (`nullptr` means null)
- Json(const Json& origin)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;&nbsp;//copy constructor
- Json(Json&& rhs)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;&nbsp;//move constructor
- Json(string jsonStr)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;//deserialized constructor
- explicit Json(std::initializer_list&lt;std::pair&lt;const std::string, Json&gt;&gt; values)&emsp;&emsp;&emsp;&emsp;&emsp;//initializer_list Object constructor
- Json& operator = (const Json& origin)&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;
- Json& operator = (Json&& origin)&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;
- Json operator[](const int& index)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;
- Json operator[](const string& key)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;
- template&lt;typename T&gt; Json& add(T value)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//append an element to an Array (no effect on an Object)
- template&lt;typename T&gt; Json& add(string name, T value)&emsp;&emsp;//add a member to an Object; **append** semantics, a duplicate key leaves two members (use `setChild` to replace)
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
- vector&lt;Json&gt; toVector() const&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;
- Json& extend(Json value)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;
- Json& concat(Json value)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//for array object
- Json& push_front(const Json& value)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//for array object
- Json& push_back(const Json& value)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//for array object
- Json& insert(int index, const Json& value)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//for array object
- Json& clear()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//clear child
- Json& remove(const string &key, Json* self = nullptr, Json* prev = nullptr)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;
- bool contains(const string& key) const&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;
- string getValueType() const&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//return value's type in string
- Json take(const string& key)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;
- Json getAllKeys() const&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;

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

More interface

- const Json* resolvePointerPtr(string_view pointer) const&emsp;//locate by RFC 6901 pointer; nullptr when absent (no allocation)
- Json at(const string& pointer) const&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;//pointer-addressed **copy** (Error when absent)
- static Json ParseJsonStrict(input, err) / ParseJsonStrictUtf8(input, err)&emsp;//strict / strict + UTF-8 validation
- static Json FromFile(path)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//read a file (document-shaped files are move-parsed; Error on failure)
- Json& mergePatch(const Json& patch)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//RFC 7386 Merge Patch (in place)
- Json applyPatch(const Json& operations, string& err) const&emsp;&emsp;//RFC 6902 JSON Patch (returns a new document)
- iterator / const_iterator with begin/end/cbegin/cend&emsp;&emsp;&emsp;&emsp;//structured bindings work; `key()` returns a `string_view`

Direct-child access & safe-mutation helpers (end of `zjson.hpp`, `namespace ZJSON`, added 2026-09-17)

A group of `inline` free functions that centralize "direct members only" and "never break the sibling
chain". They use **public API only** and their behaviour is pinned by `tests/test_util.cpp`; full usage
notes and the ADL caveat are in [`docs/使用指南.md`](docs/使用指南.md) §8.

- const Json* directChild(const Json& object, string_view key)&emsp;//direct members only (no deep search); nullptr when absent / not an object
- bool hasChild(const Json& object, string_view key)&emsp;&emsp;&emsp;//does a direct member exist
- Json childValueOr(const Json& object, string_view key, const Json& default)&emsp;//direct value, or the default
- std::string ownedKey(string_view key)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;//materialize an iterator `key()` (string_view -> std::string)
- int memberCount(const Json& object)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//member count (0 for a non-object)
- bool isEmptyObject(const Json& object)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//object emptiness (the only correct way; do not use `isEmpty()`)
- bool setElement(Json& array, int index, const Json& value)&emsp;//replace an array element, keeping its siblings
- void setChild(Json& object, string_view key, const Json& value)&emsp;//replace/add a direct member (no duplicates, order preserved)

> ⚠ Three traps these helpers exist for: `operator[]` returns a **copy** (so `obj["k"] = v` writes to a
> temporary); `operator=` clears the `brother` link (so `it.value() = v` drops every following element);
> and `size()` / `isEmpty()` report **-1** / **always true** for an object.
>
> ⚠ `directChild` returns a **pointer into the document** - do not let it outlive a critical section when
> other threads may write (use the value-returning `childValueOr`); see
> [`docs/多线程使用指南.md`](docs/多线程使用指南.md) §3.1.

Semantics worth knowing

- `operator[]` returns a **copy** (a member's subtree is deep-copied). Use `findPtr`/`findPtrAt`/`atRef` to read or modify in place.
- The deep-search fallback used when a key is not a direct member returns the **first match in document order** (pre-order).
- Parsing limits nesting to **101 levels**; `cloneChain`/`deleteJson`/pretty printing/comparison are iterative, so a 20000-level document built through the API can be copied, printed, compared and destroyed safely.
- Equality treats members as a **multiset**: duplicate keys must match in multiplicity and value pairing (parsing itself collapses duplicates; the default policy keeps the last one).
- **`key()` returns `std::string_view` (breaking change since 2026-09-16)**: `entry.key()`, `it.key()` and `it->key()` no longer return `const string&`. `std::string`'s converting constructor from a `string_view` is **explicit**, so **only copy-initialization contexts** stop compiling - `=` at a declaration, a by-value argument, a `return` into `std::string`, `push_back`:

  | Form | Result |
  |---|---|
  | `std::string k = e.key();` (copy-init) | ❌ does not compile |
  | `take(e.key())` (by-value parameter) = `return e.key();` = `v.push_back(e.key())` | ❌ does not compile |
  | `std::string k(e.key());` (direct-init) | ✅ |
  | `std::string k; k = e.key();` (assignment, not initialization) | ✅ |
  | `s += e.key();` / `s.append(e.key());` / `v.emplace_back(e.key())` | ✅ |
  | comparison, `.empty()`/`.size()`, structured bindings, using the `string_view` directly | ✅ |

  Note that `std::string k = e.key();` (a declaration - fails) and `k = e.key();` (an assignment - works) behave differently. Migration: add parentheses, `std::string(e.key())`, or use the `string_view` as-is - or use the helper block's `ZJSON::ownedKey(e.key())`, whose name states the intent.
- The view returned by `key()`/`it.key()` is valid while the document is alive and the member is not renamed (it points into the parse arena for long keys); copy it into a `std::string` while it is still valid when it must outlive that.
- Object **names** are owned when they fit `std::string`'s inline buffer and borrow the parse arena when they are longer (so nothing allocates per key, and no read path ever rewrites a node); string **values** always borrow the arena.
- Structured bindings work through the ADL `get` + `std::tuple_size`/`std::tuple_element`; `std::get<N>(entry)` is intentionally not provided (adding overloads to `namespace std` for our own types would be undefined behaviour).
- A JSON **integer literal** (no fraction, no exponent) is stored exactly as `int64`/`uint64` and written back verbatim, so `{"id":9007199254740993}` round-trips; `42.0`, `42e0` and `-0` stay `double` (`isIntegral()` tells them apart). The third numeric state costs no memory - the kind tag lives in the padding that already followed `type`.
- Performance, and the comparison against nlohmann/json, RapidJSON and simdjson (throughput, node-pool cost, access-path cost, stringify hotspots): see [`docs/性能测试报告.md`](docs/性能测试报告.md), raw medians in `docs/benchmark_2026-09-15_clang64_medians.csv`.- The 2026-09-15 performance work in two rounds - wide-object key index (flat-object parse **+23%** overall, **1.75x** on a 100 KB flat document) and the R2 node slimming (`sizeof(Json)` 176 -> 128, copy **+16.6%**, node churn **-12.7%**) - with its A/B evidence, plan-validation measurements and rejected candidates: see [`docs/性能优化实施与评估-2026-09-15.md`](docs/性能优化实施与评估-2026-09-15.md). Independent review of the R2 round: [`docs/复核-2026-09-15-R2与UAF归因.md`](docs/复核-2026-09-15-R2与UAF归因.md).
## Thread safety and memory

1. **Node allocation and deallocation are thread safe.** Every thread owns a slab pool that is never released, so a node allocated on one thread may be freed on another (or after the allocating thread has exited). No locking is involved.
2. **A document that is only read is safe to share for concurrent reads.** The lazy key index is published with an atomic compare-exchange by the const read paths (racing threads share one fully built table), and **no const read path writes to a node** - `entry.key()`/`it.key()` return a `std::string_view`, so nothing is materialized, allocated or thrown. **Concurrent reads and writes of one document are still not safe** (same as `std::string`). An external lock only works when every read goes through a value-returning API and no interior reference or pointer outlives the critical section; the results of `atRef`/`findPtr`/iterators/`key()` are exactly the ones that do. Full recipes, anti-patterns and the measured cost of each pattern: [`docs/多线程使用指南.md`](docs/多线程使用指南.md).
3. **Pool residency is per thread and unbounded by design.** Each thread keeps the slabs it ever used (measured: 64 short-lived threads that each parsed a 40k-node document leave about 440 MB resident for the process lifetime). Reuse worker threads; do not create one thread per request if documents can be large.
4. **Cross-module ownership is not guaranteed.** The header inlines into every module, so a document passed across DLL boundaries and destroyed after the owning module is unloaded is unsafe.
5. Deep documents are safe to copy/print/compare/destroy (iterative traversals), but the parser still refuses nesting beyond 101 levels.

The **full multi-threading guide** (which patterns are safe out of the box, whether adding your own lock is
enough, the measured price of each of the four patterns, and the anti-pattern list):
[`docs/多线程使用指南.md`](docs/多线程使用指南.md). How the two const-read races were found and fixed:
[`docs/线程安全审查与修复-2026-09-16.md`](docs/线程安全审查与修复-2026-09-16.md).
    
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