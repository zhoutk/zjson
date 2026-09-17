# ZJSON 从 Qt JSON 迁移指南

> 面向把项目中 `QJsonDocument / QJsonObject / QJsonArray / QJsonValue` 全面替换为
> ZJSON 的团队。全部差异条目都来自一个真实项目的完整迁移（geode：ORM 持久层 +
> Qt 桌面应用 + HTTP 服务 + 多进程 worker，约 30 个消费文件、600+ 处引用，含
> 数据文件兼容回归），并有测试佐证：ZJSON 侧 `tests/test_util.cpp` 与
> `test_qt_macro_compat.cpp`，项目侧合约测试。
>
> 基础用法（非 Qt 部分）见[`使用指南.md`](使用指南.md)。

---

## 1. 类型映射总览

| Qt | ZJSON | 说明 |
|---|---|---|
| `QJsonDocument`（解析） | `ZJSON::Json::ParseJsonStrictUtf8(s, err)` | 见第 2 节 |
| `QJsonDocument`（序列化） | `json.toString()` / `toString(4)` | Compact / Indented |
| `QJsonObject` / `QJsonArray` / `QJsonValue` | `ZJSON::Json` 一个类通吃 | 以 `isObject()/isArray()/isString()/isNumber()` 区分 |
| `QJsonObject::value(k)` | `ZJSON::childValueOr(o, k)` | 直系 + 缺失默认 |
| `QJsonObject::contains(k)` | `ZJSON::hasChild(o, k)` 或 `o.contains(k)` | 都只查直系 |
| `QJsonObject::insert(k, v)` | `ZJSON::setChild(o, k, v)` | 替换或新增 |
| `QJsonArray::operator[] = v` | `ZJSON::setElement(a, i, v)` | 数组元素替换 |
| `QJsonArray::append(v)` | `a.push_back(v)` 或 `a.add(v)` | |
| `QJsonArray::size()` / `isEmpty()` | `a.size()` / `a.size() == 0` | 见第 4 节陷阱 |

**一个类 vs 四个类**：ZJSON 用 `Json` 统一表达七种类型
（Null/True/False/Number/String/Array/Object/Error）。判断类型永远先 `isXxx()`，
不存在 `QJsonDocument::isObject()` 那样的"文档级"与"值级"两层区分。

---

## 2. 解析迁移

### 2.1 基本替换

```cpp
// Qt
QJsonParseError parseError;
QJsonDocument doc = QJsonDocument::fromJson(raw, &parseError);
if (doc.isNull()) { ... }

// ZJSON（推荐：StrictUtf8 与 QJsonDocument::fromJson 的接受范围最接近）
std::string err;
ZJSON::Json doc = ZJSON::Json::ParseJsonStrictUtf8(jsonText, err);
if (doc.isError()) { /* err 有原因 */ }
```

注意入参类型：ZJSON 收 `std::string`，不是 `QByteArray`。Qt 侧常见的
`QString::toUtf8()` 得到的是 `QByteArray`，转法：

```cpp
// 从 QString：
ZJSON::Json::ParseJsonStrictUtf8(text.toStdString(), err);
// 从 QByteArray：
ZJSON::Json::ParseJsonStrictUtf8(std::string(raw.constData(), size_t(raw.size())), err);
// 从文件：
ZJSON::Json doc = ZJSON::Json::FromFile(path.toLocal8Bit().constData());  // 失败 = Error
```

### 2.2 接受范围逐项对照（重要）

| 输入情形 | QJsonDocument::fromJson | ZJSON 推荐 | 结论 |
|---|---|---|---|
| 非法 UTF-8 序列 | ✗ 拒绝（"invalid UTF8 string"） | `ParseJsonStrictUtf8` ✗ 拒绝 | **一致**（实测验证；不要用不带 Utf8 的版本，它默认不校验） |
| `//`、`/* */` 注释 | ✗ 拒绝 | `ParseJsonStrict*` ✗ 拒绝 / `ParseJson` ✓ 容忍 | 用 Strict 系列才一致 |
| 重复键 | 保留**最后**一个 | 默认 `KeepLast` | 一致（要改用 `ParseOptions.duplicateKey`） |
| 末尾垃圾字符 | ✗ 拒绝 | ✗ 拒绝 | 一致 |
| 嵌套深度 | 有限制 | 限深 **101 层** | 均为防御性限制 |

### 2.3 精度差异（ZJSON 的增强，注意数据兼容）

QJsonValue 把**所有数字存成 double**；ZJSON 把整数字面量按 `int64/uint64`
精确存储。因此：

```
{"id": 9007199254740993}   // > 2^53
```

- Qt：读回 `9007199254740992`（精度丢失，且可能静默）；
- ZJSON：解析、存储、序列化全程精确往返。

迁移读取旧数据时这是纯收益；但如果有代码依赖"Qt 丢精度后的值"，需要自查。

---

## 3. 读取迁移

### 3.1 QJsonValue 的"默认值"语义是最大的语义差异

`QJsonValue::toString(def)/toInt(def)/toDouble(def)/toBool(def)` 的契约是
**"类型不符或缺失 → 返回默认值"**。ZJSON 的 `toInt()/toDouble()` 是宽松转换
（字符串会做前缀数字解析、缺失返回 0），**直接替换会悄悄改变行为**：

| 表达式（缺失该键时） | Qt | ZJSON 裸用 |
|---|---|---|
| `obj["port"].toInt()` | 0（缺键 → Undefined → 默认 0） | 0 | 
| `obj["name"].toInt(3)` | 3 | **编译不过**（无默认值重载） |
| `obj["count"].toDouble()`（值是字符串 `"12"`） | 0（类型不符） | **12**（前缀解析！行为变了） |
| `obj["ok"].toBool()`（值是数字 1） | false | false |

推荐做法：在项目里放一个薄封装头，把 QJsonValue 语义一次性补齐（本项目的
实现是 `maple/src/common/zjson_qt.h`，可直接参考），核心就是这六个函数：

```cpp
// zjson_qt.h —— Qt 侧的 ZJSON 取值辅助（全部只看直系子节点）
namespace zjsonqt {

inline QString childString(const ZJSON::Json& o, std::string_view k, const QString& def = {}) {
    const ZJSON::Json* v = ZJSON::directChild(o, k);
    return (v && v->isString()) ? QString::fromStdString(v->toString()) : def;
}

inline double childDouble(const ZJSON::Json& o, std::string_view k, double def = 0.0) {
    const ZJSON::Json* v = ZJSON::directChild(o, k);
    return (v && v->isNumber()) ? v->toDouble() : def;
}

inline int childInt(const ZJSON::Json& o, std::string_view k, int def = 0) {
    const ZJSON::Json* v = ZJSON::directChild(o, k);
    if (!v || !v->isNumber()) return def;
    const double n = v->toDouble();
    return (n != static_cast<double>(static_cast<int>(n))) ? def : static_cast<int>(n);
}

inline bool childBool(const ZJSON::Json& o, std::string_view k, bool def = false) {
    const ZJSON::Json* v = ZJSON::directChild(o, k);
    if (!v) return def;
    if (v->isTrue())  return true;
    if (v->isFalse()) return false;
    return def;
}

inline ZJSON::Json childArray(const ZJSON::Json& o, std::string_view k) {   // toArray()
    const ZJSON::Json* v = ZJSON::directChild(o, k);
    return (v && v->isArray()) ? *v : ZJSON::Json(ZJSON::JsonType::Array);
}

inline ZJSON::Json childObject(const ZJSON::Json& o, std::string_view k) {  // toObject()
    const ZJSON::Json* v = ZJSON::directChild(o, k);
    return (v && v->isObject()) ? *v : ZJSON::Json(ZJSON::JsonType::Object);
}

} // namespace zjsonqt
```

替换对照：

```cpp
// Qt                                          // ZJSON
obj["type"].toString()                    →  zjsonqt::childString(obj, "type")
obj["lat"].toDouble(vpLat_)               →  zjsonqt::childDouble(obj, "lat", vpLat_)
obj.contains("zoom") ? obj["zoom"].toInt(vpZoom_) : vpZoom_
                                          →  ZJSON::hasChild(obj, "zoom")
                                                 ? zjsonqt::childInt(obj, "zoom", vpZoom_) : vpZoom_
item.value("dots").toArray()              →  zjsonqt::childArray(item, "dots")
```

`QJsonValue::toVariant().toString()`（属性表显示常用）的等价物：

```cpp
inline QString jsonValueDisplayText(const ZJSON::Json& value) {
    if (value.isString()) return QString::fromStdString(value.toString());
    if (value.isNumber()) return QString::number(value.toDouble());
    if (value.isTrue())   return QStringLiteral("true");
    if (value.isFalse())  return QStringLiteral("false");
    return QString();                       // 数组/对象/null 与 Qt 一致输出空串
}
```

### 3.2 `QJsonObject::constBegin()` 迭代

```cpp
// Qt：key() 是 QString
for (auto it = objectInfo.constBegin(); it != objectInfo.constEnd(); ++it)
    keys.push_back(it.key());

// ZJSON：key() 是 string_view（不物化），需持有副本时物化
for (auto it = objectInfo.cbegin(); it != objectInfo.cend(); ++it)
    keys.push_back(QString::fromStdString(ZJSON::ownedKey(it->key())));
```

### 3.3 数组遍历

```cpp
// Qt：for (const QJsonValue& v : array) / v.toObject()
// ZJSON：下标循环 + isObject 守卫（toObject 的"非对象→空对象"语义用守卫还原）
for (int index = 0; index < objects.size(); ++index) {
    if (objects[index].isObject()) {
        const ZJSON::Json& item = objects[index];
        ...
    }
}
```

---

## 4. 写入迁移

### 4.1 构造对象

```cpp
// Qt
QJsonObject body;
body["message"] = params.message;                      // operator[] 可赋值
body["metadata"] = QJsonObject{{"app", "maple"}};

// ZJSON：operator[] 返回副本、赋值无效！用初始化列表 + add()
ZJSON::Json body{
    {"message", params.message.toStdString()},
    {"metadata", ZJSON::Json{{"app", "maple"}}},
};
body.add("session_id", sid.toStdString());             // 追加新键（key 不存在时）
```

⚠ `add()` 是**追加**不是替换：对已存在的键 add 会留下重复成员。条件性覆盖用
`ZJSON::setChild(body, "key", value)`（QJsonObject::insert 语义，保持成员顺序）。

⚠ **不要**写 `obj["k"] = v`：ZJSON 的 `operator[]` 返回**副本**，赋值被丢弃
（Qt 的 `QJsonObject::operator[]` 返回可赋值的 `QJsonValueRef`，这是迁移中最常见
的静默 bug——编译器通常不报错）。写一律走初始化列表 / `add()` / `setChild()`。

### 4.2 数组

```cpp
// Qt
QJsonArray pts; for (double v : points) pts << v;
QJsonArray arr; arr.append(obj);
pts[i] = value;                        // QJsonValueRef 可赋值

// ZJSON
ZJSON::Json pts(ZJSON::JsonType::Array);
for (double v : points) pts.push_back(v);
ZJSON::Json arr(ZJSON::JsonType::Array);
arr.push_back(obj);
ZJSON::setElement(pts, i, ZJSON::Json(value));   // 数组元素替换
```

### 4.3 序列化

```cpp
QJsonDocument(body).toJson(QJsonDocument::Compact)   →  body.toString()
QJsonDocument(body).toJson(QJsonDocument::Indented)  →  body.toString(4)
doc.toJson() → QByteArray（网络发送）                →  QByteArray::fromStdString(body.toString())
```

**键序差异**：`QJsonObject` 迭代按**键排序**输出；ZJSON 按**插入序**。
- 对 HTTP 响应 / 配置文件的可读性无影响；
- 对字节级比对、内容哈希、diff 稳定性有影响——持久化文件迁移后键序会变，
  语义等价但字节不同。本项目 `data.json` 迁移即属此类（纯外观差异）。

**数字格式差异**：Qt 与 ZJSON 的 double 输出格式不同（ZJSON 为最短往返、整数
字面量原样输出且不经 double）。`{"id":9007199254740993}` 这类大整数在 ZJSON 下
反而更准。做文件哈希比对时注意。

---

## 5. Qt 机制相关的硬陷阱（本项目实测踩过的两个编译期/运行期问题）

### 5.1 `ZJSON::Json` 不能放进 QVector / Q_DECLARE_METATYPE

`Json` 声明了**类级 `operator new`**（节点走 slab 池）。类级 `operator new` 会
**隐藏全部全局 `operator new` 重载——包括 placement new**：

- `QVector<ZJSON::Json>` ❌ 编译失败（Qt 容器内部用未限定的 placement new）
- `Q_DECLARE_METATYPE(ZJSON::Json)` ❌ 编译失败（metatype 系统同样无法构造）

```cpp
// ✗ QVector<ZJSON::Json> pickedObjects_;
std::vector<ZJSON::Json> pickedObjects_;    // ✓（std 容器用 ::operator new + 限定 placement new）
```

### 5.2 信号槽

- **直连**（同线程，AutoConnection 默认路径）传 `const ZJSON::Json&`：✅ 直接可用，
  无需任何注册；
- **队列连接**（跨线程）：需要 metatype 注册，而 `Q_DECLARE_METATYPE` 不可用
  （5.1）→ 不要跨线程传 `Json`。替代方案：跨线程只传序列化文本
  （`QString::fromStdString(json.toString())`）或拆好的标量字段，在接收线程再解析。

### 5.3 ADL 二义

`ZJSON::hasChild/setChild/...` 带 `Json` 参数，ADL 会把它们带入重载决议。如果你
项目里已有**同名且带 Json 参数**的自由函数（比如本地 `hasChild`），不加限定会
编译报 "call to 'hasChild' is ambiguous"。处理：删除本地重复实现（推荐，语义
本来就一样），或全部加 `ZJSON::` 限定。注意：**不带 Json 参数**的同名函数
（如 `ownedKey(std::string_view)`）不受 ADL 影响，不会冲突。

### 5.4 Qt 关键字宏

Qt 把 `slots`/`signals`/`emit`/`foreach` 定义成空宏。任何要在 Qt TU 里使用的
头文件不得用这些词做标识符。ZJSON 已在上游修复（成员改名 `slotTable`）并由
`test_qt_macro_compat` 用例钉死——你的 Qt 代码可以放心 `#include "zjson.hpp"`，
无需任何包裹。

---

## 6. 行为差异速查表（迁移自查清单）

| # | 行为 | Qt JSON | ZJSON | 迁移动作 |
|---|---|---|---|---|
| 1 | 缺键取值 | `toInt(def)` 等默认值 | 无默认值重载；裸取返回 0/空 | 用 zjsonqt 封装（3.1） |
| 2 | 字符串转数字 | `toDouble()` 不转换 | `toDouble()` 前缀解析 `"12x"`→12 | 封装里先 `isNumber()` 判断 |
| 3 | 大整数（>2^53） | double，丢精度 | int64/uint64 精确 | 纯收益；排查依赖丢精度的代码 |
| 4 | `operator[]` 赋值 | 有效（QJsonValueRef） | 无效（副本） | 写路径全部改 add/setChild |
| 5 | 数组元素赋值 | 有效 | 需 `setElement`（splice） | 同上 |
| 6 | `add` 重复键 | `insert` 覆盖 | `add` 追加留重复 | 覆盖语义用 `setChild` |
| 7 | `remove(key)` | 直系 | **深搜递归删除** | 只要直系删除时重建对象 |
| 8 | 对象判空 | `isEmpty()` 正常 | `isEmpty()` 对对象恒 true | 一律 `ZJSON::isEmptyObject()` |
| 9 | 成员计数 | `size()` 正常 | `size()` 对对象返回 -1 | 一律 `ZJSON::memberCount()` |
| 10 | 迭代 key 类型 | QString | string_view（explicit 转换） | 物化用 `ZJSON::ownedKey()` |
| 11 | 键序 | 排序输出 | 插入序 | 字节级比对场景需注意 |
| 12 | 数字输出格式 | Qt 格式 | 最短往返 + 整数原样 | 内容哈希场景需注意 |
| 13 | 注释 | 拒绝 | `ParseJson` 容忍 / Strict 拒绝 | 用 Strict 系列对齐 Qt |
| 14 | 非法 UTF-8 | 拒绝 | 仅 `*Utf8` 系列拒绝 | 用 `ParseJsonStrictUtf8` |
| 15 | 解析失败表达 | `doc.isNull()` | `doc.isError()` + err 出参 | 机械替换 |
| 16 | QVector/metatype | — | 不可用（operator new） | `std::vector`；跨线程不传 Json |
| 17 | 文档四类 → 一类 | Document/Object/Array/Value | `Json` + `isXxx()` | 心智模型切换 |

---

## 7. 建议的迁移步骤（本项目实际执行顺序）

1. **先落封装头**：抄第 3.1 节的 `zjson_qt.h`（或按需裁剪），让后续替换有统一
   语义出口；
2. **自底向上替换**：先替换无 UI 的基础层（持久化/服务/工具类），再替换 UI 层。
   每替换一个模块就编译 + 跑该模块测试；
3. **机械替换模式**（sed 级别）：
   - `#include <QJsonDocument>/<QJsonObject>/<QJsonArray>` → `#include "zjson.hpp"`
   - `QJsonDocument::fromJson(x.toUtf8())` → `ParseJsonStrictUtf8(x.toStdString(), err)`
   - `obj["k"].toString()` → `zjsonqt::childString(obj, "k")`（按 3.1 对照表）
   - `QJsonObject obj; obj["k"]=v;` → 初始化列表 + `add()`
   - 循环改下标 + `isObject()` 守卫；
4. **重点回归四类行为**（差异最大的地方）：
   - 无效 UTF-8 输入是否同样被拒（数据兼容）；
   - 缺键默认值路径（3.1 的六函数逐一对拍）；
   - 大整数与数字输出格式（持久化文件字节兼容性）；
   - `operator[]` 赋值遗漏（编译期抓不到的静默 bug，grep `json.*\[.*\]\s*=` 复查）；
5. **测试兜底**：把迁移前的行为固化成合约测试（本项目在 ORM 层有 11 项合约
   测试覆盖读写/锁/损坏文件/编码回归，迁移期间全程绿灯作为安全网）。

## 8. 实战参考

- **ORM 持久层去 Qt 化**（最严格的数据兼容场景：损坏文件、跨进程锁、原子写、
  旧格式读取）——`geode` 仓库 `orm/src/JsonFileDb.cpp`，其本地直接子节点辅助
  （childOf/childValue/takeChild）展示了 ZJSON 之上的语义封装与深搜规避；
- **Qt 侧语义封装**——`geode` 仓库 `maple/src/common/zjson_qt.h`（第 3.1 节的
  完整版，含逐条注释）；
- **zjson 侧行为锁定**——`refer/zjson/tests/test_util.cpp`（直系辅助与安全变更
  的全部边界）、`test_qt_macro_compat.cpp`（Qt 宏共存）。
