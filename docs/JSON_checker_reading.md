# JSON_checker 代码解读与借鉴说明

## 1. 先给结论

`thirds/JSON-c/` 里的这几份代码不是 zjson 的实现依赖，也不只是“教用户怎么测一下”的演示脚本。

它们分成两层：

1. `JSON_checker.c` + `JSON_checker.h`
   这是一个独立的小型 JSON **语法检查器**。
2. `main.c`
   这是一个最小命令行示例，演示如何把标准输入逐字符送进 `JSON_checker` 做校验。

所以更准确地说：

- `JSON_checker.c/.h` 是核心库代码。
- `main.c` 才是示例程序。
- `test/pass*.json` 与 `test/fail*.json` 是配套语料，用来验证这个检查器是否按它自己的规则工作。

它的定位不是“完整 JSON 解析器”，而是“快速判断一段文本在语法上像不像 JSON”。

---

## 2. 这套代码到底是做什么的

从 `thirds/JSON-c/README` 的原始说明可以直接看出作者意图：

- 它把 `JSON_checker` 定义为一个 **Pushdown Automaton**，也就是带栈的自动机。
- 目标是“非常快地判断 JSON 文本在语法上是否正确”。
- 它可以用于：
  - 过滤输入
  - 校验输出
  - 作为更快解析器的基础

这说明它关心的是：

- 括号是否匹配
- 对象/数组上下文是否正确
- 字符串、数字、`true/false/null` 这些 token 是否符合 JSON 词法
- 嵌套是否超过允许深度

它**不负责**：

- 构建 DOM 树
- 保存对象键值
- 提供随机访问 API
- 做 pretty print
- 记录丰富的错误上下文
- 直接处理完整 UTF-8 语义校验

所以它本质上是一个“语法接受器”，不是一个“数据结构解析器”。

---

## 3. 各文件分别扮演什么角色

### 3.1 `JSON_checker.h`

这是公开接口头文件，暴露了一个非常小的 API 面：

- `new_JSON_checker(int depth)`
- `JSON_checker_char(JSON_checker jc, int next_char)`
- `JSON_checker_done(JSON_checker jc)`

同时定义了内部状态结构：

- `valid`: 当前对象是否还有效
- `state`: 当前自动机状态
- `depth`: 最大允许嵌套深度
- `top`: 栈顶位置
- `stack`: 模式栈

这说明它的工作方式是**增量式**的：

1. 创建一个 checker，并指定最大嵌套深度。
2. 每读到一个字符，就喂给 `JSON_checker_char(...)`。
3. 全部喂完之后，再用 `JSON_checker_done(...)` 做最终收尾判断。

这种 API 很适合流式输入、管道输入、网络输入或文件扫描。

### 3.2 `JSON_checker.c`

这是核心实现，主要由 5 个部分组成。

#### A. 字符分类表 `ascii_class`

作者没有直接对每个字符写一堆 `if/else`，而是先把 ASCII 字符映射到有限个“字符类别”：

- 空格
- 其它空白符
- `{` `}` `[` `]`
- `:` `,`
- `"` `\\` `/`
- `+ - . 0 1-9`
- `a b c d e f l n r s t u`
- 其它字符

这样做的好处是：

- 状态转移表更小
- 自动机逻辑更规整
- 运行时分支更少

#### B. 状态枚举 `enum states`

这里定义了自动机所处的状态，比如：

- `GO`: 起始状态
- `OK`: 当前已经形成一个合法值
- `OB`: 正在对象上下文
- `KE`: 期待 key
- `CO`: 期待冒号
- `VA`: 期待 value
- `AR`: 正在数组上下文
- `ST`: 在字符串内部
- `ES`: 在转义序列内部
- `U1~U4`: Unicode 转义 `\uXXXX` 的 4 个十六进制位
- `MI/ZE/IN/FR/FS/E1/E2/E3`: 数字词法相关状态
- `T1~T3/F1~F4/N1~N3`: `true/false/null` 识别状态

这部分体现的是：

- JSON 的“词法”和“结构”被合并进一个统一的有限状态机里处理。
- 它不是先 tokenize 再 parse，而是边读边识别。

#### C. 状态转移表 `state_transition_table`

这是整个实现的核心。

它把：

- 当前状态
- 当前字符类别

映射到：

- 下一个状态，或
- 一个负数动作码

这些负数动作码代表结构行为，例如：

- `-6`: 读到 `{`，进入对象并压栈
- `-5`: 读到 `[`，进入数组并压栈
- `-4`: 字符串结束，根据当前栈顶模式决定接下来是否合法
- `-3`: 读到 `,`，根据栈顶模式决定进入下一个 key 还是下一个 value
- `-2`: 读到 `:`，把对象从“等 key”切到“等 value”
- `-7/-8/-9`: `]` 或 `}` 出栈

这就是典型的“表驱动 PDA”写法。

#### D. 栈模式 `MODE_ARRAY / MODE_DONE / MODE_KEY / MODE_OBJECT`

状态机只靠“当前状态”还不够，因为 JSON 的结构嵌套需要记住上下文。为此作者用了一个显式栈，栈里保存的是结构模式：

- `MODE_ARRAY`: 当前在数组内部
- `MODE_OBJECT`: 当前在对象内部，最近读完 key，期待 value 或下一个分隔行为
- `MODE_KEY`: 当前在对象内部，期待 key 或 `}`
- `MODE_DONE`: 顶层结束哨兵

这就是 README 里所说的“Pushdown Automaton”的来源。

#### E. 生命周期与失败策略

实现还定义了几个辅助函数：

- `destroy(...)`
- `reject(...)`
- `push(...)`
- `pop(...)`

特点很明显：

- 一旦遇到错误，立刻 `reject`，并销毁 checker
- API 设计成 fail-fast
- 使用者不需要手动在失败路径再清理一次对象

这个设计在 C 里很直接，但也意味着：

- 调用者不能在失败后继续复用同一个 checker
- 错误信息只有真假，没有位置、类型、上下文

### 3.3 `main.c`

`main.c` 的作用非常简单，就是一个示例程序：

1. `new_JSON_checker(20)` 创建检查器，最大嵌套深度设为 20。
2. 从 `stdin` 逐字符 `getchar()`。
3. 每个字符调用一次 `JSON_checker_char(...)`。
4. 输入结束后调用 `JSON_checker_done(...)`。
5. 若失败，打印 `syntax error`。

所以你的判断有一半是对的：

- **`main.c` 的确只是“给用户一个怎么调用检查器”的示例。**

但另一半要纠正：

- **真正有价值的不是 `main.c`，而是它背后的 `JSON_checker.c/.h` 设计。**

---

## 4. 这套实现的核心思想是什么

如果把它抽象一下，它用了 4 个非常经典的工程思路。

### 4.1 词法与结构合并，边读边判

传统解析器常见做法是：

1. 先分词
2. 再做语法分析
3. 再构建 AST/DOM

`JSON_checker` 不是这样。它把“是否为合法 JSON”这个目标压缩成一个更小的问题：

> 只要知道当前在什么状态、当前字符属于什么类别、当前结构栈是什么，就能决定下一步是否合法。

因此它可以做到：

- 不生成 token 数组
- 不分配节点对象
- 不保存结果树
- 一边输入一边判断

### 4.2 显式栈代替递归

JSON 的对象/数组嵌套天然需要上下文。

这套代码没有依赖 C 函数调用栈做递归，而是显式维护一个 `stack`：

- 好处是深度受控
- 不容易栈溢出
- 结构状态更容易做边界控制

### 4.3 表驱动而不是分支驱动

大部分逻辑都被搬到了：

- 字符分类表
- 状态转移表

这意味着：

- 代码更像“数据驱动”而不是“流程硬编码”
- 性能通常比较稳定
- 适合做形式化检查和差异对照

### 4.4 API 设计成流式验证器

它的 API 不是：

- `parse(string)`

而是：

- `new`
- `char`
- `done`

这个接口说明作者把它定位成：

- 流式输入的验证器
- 过滤器
- 管道前置检查器

而不是一个用户直接操作的 JSON 容器库。

---

## 5. 它不是什么，以及它的局限在哪里

这个部分很重要，否则很容易高估它。

### 5.1 它不是完整 JSON 库

它没有：

- DOM
- mutation API
- 序列化
- 查询
- 指针访问
- patch/merge patch

所以它不能替代 zjson。

### 5.2 它的语义带有历史背景

这套 `JSON_checker` 小套件的一个重要特点是：

- 顶层 JSON 值被视为必须是 **object 或 array**

这和后来的 RFC 8259 常见理解并不完全一致；现代 JSON 通常允许顶层标量值，例如：

- `"hello"`
- `123`
- `true`
- `null`

但在 `JSON_checker` 的测试里，顶层字符串属于失败用例。这就是为什么我们在 [tests/test_json_checker.cpp](d:/codes/zjson/tests/test_json_checker.cpp) 里专门兼容了它的原始语义。

### 5.3 UTF-8 处理不是它自己完整解决的

在 `JSON_checker.c` 里：

- ASCII `< 128` 通过查表分类
- 非 ASCII 统一归到 `C_ETC`

这意味着：

- 它本身不负责完整 UTF-8 解码与校验
- UTF 相关能力依赖附带的 `utf8_decode.*` / `utf8_to_utf16.*`
- 如果直接拿它做现代严格 UTF-8 校验，能力是不够的

### 5.4 错误报告很弱

它只能告诉你：

- 通过
- 不通过

它不能直接告诉你：

- 第几行第几列错了
- 错的是哪一类 token
- 为什么错

这对调试用户输入并不友好。

### 5.5 直接复用源码有许可风险

这点必须明确写出来。

`JSON_checker.c` 头部许可除了 MIT 风格段落外，还有一句：

> The Software shall be used for Good, not Evil.

这不是标准 MIT 许可条款，也会让该许可在很多组织的合规体系下变得敏感甚至不可接受。

所以建议是：

- **学习其设计思想可以。**
- **测试其行为可以。**
- **直接拷贝实现到主库中要非常谨慎。**

对 zjson 来说，正确路线不是“抄它的 C 代码”，而是“借鉴它的自动机设计思想，用你自己的 C++ 实现表达”。

---

## 6. 对 zjson 而言，我们已经借鉴了什么

从当前仓库状态看，我们实际上已经吸收了它最有价值的那部分思想。

### 6.1 显式栈 PDA 解析思路

zjson 当前主解析器已经是显式栈 PDA 路线，而不是早期的递归下降。这个方向和 `JSON_checker` 的核心思想是一致的：

- 显式维护结构上下文
- 控制嵌套深度
- 用状态与栈驱动解析流程

### 6.2 用官方小套件做专项回归

当前仓库中的 [tests/test_json_checker.cpp](d:/codes/zjson/tests/test_json_checker.cpp) 已经把 `JSON_checker` 的 36 个样例纳入回归测试。

这件事的价值不是“证明我们用了它的代码”，而是：

- 证明我们对它那套历史语义是清楚的
- 把它当作一个外部行为基线
- 防止未来解析器修改时无意偏离这组已知边界条件

### 6.3 把“结构语法验证”和“DOM 构建”视为可拆分问题

`JSON_checker` 最大的启发不是某段 C 代码，而是这个建模方式：

- JSON 语法合法性检查
- JSON 数据结构构建

这两个问题虽然相关，但不必强绑定在同一份实现里。

这对后续 zjson 做性能优化很有帮助。

---

## 7. 我们还能继续借鉴什么

这里分“值得借鉴”和“不建议照搬”两部分说。

### 7.1 值得借鉴的点

#### A. 做一个纯语法扫描 fast path

可以考虑在 zjson 里增加一个轻量接口，例如：

```cpp
bool IsValidJsonSyntax(std::string_view input, SyntaxOptions opts = {});
```

特点：

- 只判断语法是否合法
- 不构建节点树
- 不分配 DOM 节点
- 适合在高吞吐预过滤场景使用

这对以下场景尤其有价值：

- 接口网关预校验
- 大量日志/消息过滤
- benchmark 中拆分“语法扫描成本”和“DOM 构建成本”

#### B. 把状态/动作进一步数据化

虽然 zjson 当前 PDA 已经工作良好，但如果未来要继续做：

- 更细粒度的错误分类
- 更好做 fuzzing/状态覆盖
- 更强的可验证性

可以借鉴 `JSON_checker` 这种：

- 字符类别表
- 状态转移表
- 动作码

不过不一定需要完全照抄成二维静态数组，可以用更现代的 C++ 方式表达。

#### C. 强化流式输入接口

`JSON_checker` 的一个优势是天然支持增量输入。

如果 zjson 后续要做：

- `istream` 解析
- chunked 网络流解析
- 长连接消息流校验

可以单独设计一个 streaming syntax checker 或 streaming parser front-end。

#### D. 用固定深度策略做 DoS 防护

`JSON_checker` 从一开始就把深度限制放进 API：

```c
new_JSON_checker(int depth)
```

这是一种很直接的安全边界。

zjson 当前也有深度限制，但这个思路值得继续明确化：

- 对外显式暴露策略配置
- 在文档中明确默认值和失败行为
- 在测试中固定边界语义

### 7.2 不建议照搬的点

#### A. 不建议直接搬它的 C 实现

原因有三类：

1. 许可条款敏感
2. 语义带历史局限
3. 与 zjson 当前 C++ 代码风格不一致

#### B. 不建议退回到“只有真假没有诊断”的接口

zjson 作为用户侧库，不能只告诉调用方“错了”。

更合理的方向是：

- 快速语法扫描路径可以保持轻量
- 主解析路径仍保留详细错误信息

#### C. 不建议无条件继承它的顶层 object/array 语义

这个规则只适合“兼容 JSON_checker 历史测试集”这一个上下文。

对于 zjson 的严格模式，是否允许顶层标量，应以项目自身文档和标准定位为准，而不应被这个外部小项目绑死。

---

## 8. 如果把它映射到现代 C++ 设计，应怎么改写

如果要把 `JSON_checker` 的思想用现代 C++ 重新表达，一个更合理的形态大概是：

```cpp
struct SyntaxError {
    size_t offset;
    size_t line;
    size_t column;
    const char* reason;
};

class JsonSyntaxChecker {
public:
    explicit JsonSyntaxChecker(size_t maxDepth = 100);
    bool consume(char ch);
    bool finish();
    const SyntaxError& error() const;
private:
    // state, stack, counters...
};
```

或者更直接一些：

```cpp
struct SyntaxOptions {
    size_t maxDepth = 100;
    bool allowComments = false;
    bool allowTopLevelScalars = true;
    bool validateUtf8 = true;
};

Expected<void, SyntaxError> ValidateJson(std::string_view input,
                                        const SyntaxOptions& options = {});
```

这样的好处是：

- 保留自动机/显式栈/流式校验的优点
- 融入现代错误模型
- 不受旧许可限制
- 能和 zjson 现有严格模式、UTF-8 策略、深度策略统一起来

---

## 9. 对这份第三方代码的最终判断

可以把结论压缩成 4 句话：

1. `JSON_checker.c/.h` 是一个真正可用的、很小的 JSON 语法检查器，不只是 demo。
2. `main.c` 只是演示如何按字符流方式调用它。
3. 它最值得借鉴的是“表驱动 + 显式栈 + 增量校验”的设计思想，而不是它的具体 C 源码。
4. 对 zjson，正确的使用方式是“拿它做对照和启发”，而不是“把它直接并入主实现”。

---

## 10. 对本仓库的建议

如果后续你想把这份第三方代码的价值进一步吃干榨尽，我建议优先级按下面排：

1. 保持 [tests/test_json_checker.cpp](d:/codes/zjson/tests/test_json_checker.cpp) 这组专项回归，继续作为外部语义基线。
2. 如有性能需求，再单独实现一个“纯语法扫描”接口，不构建 DOM。
3. 继续沿用你自己的 C++ PDA 解析实现，不直接复用 `JSON_checker.c`。
4. 如果将来要引入流式解析，再把 `new/char/done` 这种接口形式吸收进来。

这条路线既能继承它的工程价值，也不会把历史语义和许可问题带进主库。
