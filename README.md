# zjson

## 介绍
从node.js转到c++，特别怀念在js中使用json那种畅快感。在c++中也使用过了些库，但提供的接口使用方式，总不是习惯，很烦锁，接口函数太多，不直观。参考了很多库，如：rapidjson, cJson, CJsonObject, drleq-cppjson, json11等，受cJson的数据结构启发很大，决定用C++手撸一个。最后因为数据存储需要不区分型别，又要能知道其型别，所以选择了C++17才支持的std::variant以及std::any，最终，C++版本定格在c++17，本库设计为单头文件，且不依赖c++标准库以外的任何库。

## 项目名称说明
本人姓名拼音第一个字母z加上josn，即得本项目名称zjson，没有其它任何意义。我将编写一系列以z开头的相关项目，命名是个很麻烦的事，因此采用了这种简单粗暴的方式。

## 设计思路 
简单的接口函数、简单的使用方法、灵活的数据结构、尽量支持链式操作。使用模板技术，使用给Json对象增加值的方法只有两个，AddValueBase和AddValueJson。采用链表结构（向cJSON致敬）来存储Json对象，请看我下面的数据结构设计，表头与后面的结点，都用使用一致的结构，这使得在索引操作([])时，可以进行链式操作。

## 项目进度
项目目前完成一半，可以新建Json对象，增加数据，按key(Object类型)或索引(Array类型)提取相应的值或子对象，生成json字符串。  
已经做过内存泄漏测试，析构函数能正确运行，百万级别生成与销毁未见内存明显增长。  
任务列表：
- [x] 构造函数、复制构造函数、析构函数
- [x] AddValueBase（为Json对象增加值类型）、AddValueJson（为Json对象增加对象类型）
- [x] operator=、operator[]
- [x] toString(生成json字符串)
- [x] toInt、toDouble、toFalse 等值类型转换
- [x] isError、isNull、isArray 等节点类型判断
- [ ] parse, 从json字符串生成Json对象;相应的构造函数
- [ ] Extend Json - 扩展对象
- [ ] Remove[All] key  - 删除数据, 因为Json对象允许重复的key
- [ ] findAll  - 查找全部, 因为Json对象允许重复的key
  
## 数据结构

### Json 节点类型定义
```
enum Type {
    Error,                //错误，查找无果，这是一个无效Json对象
    False,                //Json值类型 - false
    True,                 //Json值类型 - true
    Null,                 //Json值类型 - null
    Number,               //Json值类型 - 数字，库中以double类型存储
    String,               //Json值类型 - 字符串
    Object,               //Json对象类型 - 这是Object嵌套，对象型中只有child需要关注
    Array                 //Json对象类型 - 这是Array嵌套，对象型中只有child需要关注
};
```

```
class Json {
    Json* brother;       //与cJSON中的next对应，值类型才有效，指向并列的数据，但有可能是值类型，也有可能是对象类型
    Json* child;         //孩子节点，对象类型才有效
    Type type;           //节点类型
    std::variant <int, bool, double, string> data;   //节点数据
    string name;         //节点的key
}
```
## 接口说明
公开的对象类型，json只支持Object与Array两种对象，与内部类型对应。
```
enum class JsonType
{
    Object = 6,
    Array = 7
};
```
接口列表
- Json(JsonType type = JsonType::Object)
- Json(const Json& origin)
- Json& operator = (const Json& origin)
- Json operator[](const int& index) 
- Json operator[](const string& key)
- bool AddValueJson(Json& obj)
- bool AddValueJson(string name, Json& obj)
- template<typename T> bool AddValueBase(T value)
- template<typename T> bool AddValueBase(string name, T value)
- string toString()
- bool isError()
- bool isNull()
- bool isObject()
- bool isArray()
- bool isNumber()
- bool isTrue()
- bool isFalse()
- int toInt()
- float toFloat()
- double toDouble()
- bool toBool()
    
## 项目地址
```
https://gitee.com/zhoutk/zjson
或
https://github.com/zhoutk/zjson
```

## 编程示例

请参看demo.cpp或tests目录下的测试用例

## 运行方法

```
git clone https://github.com/zhoutk/zjson
cd zjson
cmake -Bbuild ..

---windows
cd build && cmake --build .

---linux & mac
cd build && make

run zjson or ctest
```

## 相关项目

会有一系列项目出炉，网络服务相关，敬请期待...
```

```