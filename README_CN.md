# ZJSON   &emsp;&emsp;  [English](README.md)  

## 介绍
从node.js转到c++，特别怀念在js中使用json那种畅快感。在c++中也使用过了些库，但提供的接口使用方式，总不是习惯，很烦锁，接口函数太多，不直观。参考了很多库，如：rapidjson, cJson, CJsonObject, drleq-cppjson, json11等。数据结构受cJOSN启发很大，解析部分借鉴了json11，向他们致敬。最后因为数据存储需要不区分型别，又要能知道其型别，所以选择了C++17才支持的std::variant以及std::any，最终，C++版本定格在c++17，本库设计为单头文件，且不依赖c++标准库以外的任何库。

## 项目名称说明
本人姓名拼音第一个字母z加上json，即得本项目名称zjson，没有其它任何意义。我将编写一系列以z开头的相关项目，命名是个很麻烦的事，因此采用了这种简单粗暴的方式。

## 设计思路 
简单的接口函数、简单的使用方法、灵活的数据结构、尽量支持链式操作。使用模板技术，得以完成最简设计，为Json对象子对象的方法只需一个 ———— addSubitem，该方法自动识别是值对象还是子Json对象。采用链表结构（向cJSON致敬）来存储Json对象，请看我下面的数据结构设计，表头与后面的结点，都用使用一致的结构，这使得在索引操作([])时，可以进行链式操作。

## 项目进度
项目目前完成大部分主要功能，具体情况请看任务列表。可以新建Json对象，增加数据，按key(Object类型)或索引(Array类型)提取相应的值或子对象，生成json字符串，并且实现从json字符串构造Json对象。  
已经做过内存泄漏测试，析构函数能正确运行，百万级别生成与销毁未见内存明显增长。
编写了大量的单元测试用例，同时支持windws、linux和mac主流操作系统。  
任务列表：
- [x] 构造函数(Object & Array)
- [x] 构造函数(值)
- [x] JSON字符串反序列化构造函数
- [x] 复制构造函数
- [x] initializer_list构造函数
- [x] 析构函数
- [x] operator=
- [x] operator[]
- [x] contains
- [x] getValueType
- [x] getAndRemove
- [x] getAllKeys
- [x] addSubitem（为Json对象增加子对象，为数组快速增加元素）
- [x] toString(生成json字符串)
- [x] toInt、toDouble、toFalse 等值类型转换
- [x] toVector 数组类型转换
- [x] isError、isNull、isArray 等节点类型判断
- [x] parse, 从json字符串生成Json对象
- [x] Extend Json - 扩展对象
- [x] concat Json - 数组扩展 
- [x] push_front - 数组压入队首
- [x] push_back - 数组压入队尾
- [x] insert - 数组插入
- [x] clear - 清空
- [x] Remove key  - 删除所有键为key的数据（Json对象允许重复的key）
- [x] std::move语义
- [ ] 递归版性能测试与对比
- [ ] 算法非递归化
- [ ] 再次性能测试与对比
  
## 数据结构

### Json 节点类型定义   
（内部使用，数据类型只在Json类内部使用）
```
enum Type {
    Error,                //错误，查找无果，这是一个无效Json对象
    False,                //Json值类型 - false
    True,                 //Json值类型 - true
    Null,                 //Json值类型 - null
    Number,               //Json值类型 - 数字，库中以double类型存储
    String,               //Json值类型 - 字符串
    Object,               //Json类对象类型 - 这是Object嵌套，对象型中只有child需要关注
    Array                 //Json类对象类型 - 这是Array嵌套，对象型中只有child需要关注
};
```
### Json 节点定义
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
公开的对象类型，json只支持Object与Array两种对象，与内部类型对应（公开类型）。
```
enum class JsonType
{
    Object = 6,
    Array = 7
};
```
接口列表
- Json(JsonType type = JsonType::Object)&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;//默认构造函数，生成Object或Array类型的Json对象
- template&lt;typename T&gt; Json(T value, string key="")&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;//值构造函数
- Json(const Json& origin)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;&nbsp;//复制构造函数
- Json(Json&& rhs)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;&nbsp;//移动构造函数
- Json(string jsonStr)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;//反序列化构造函数
- explicit Json(std::initializer_list&lt;std::pair&lt;const std::string, Json&gt;&gt; values)&emsp;&emsp;&emsp;&emsp;&emsp;//initializer_list Object构造函数
- Json& operator = (const Json& origin)&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;//赋值操作
- Json& operator = (Json&& rhs)&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;//移动赋值操作
- Json operator[](const int& index)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//Json数组对象元素查询
- Json operator[](const string& key)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;//Json Object 对象按key查询
- template&lt;typename T&gt; bool addSubitem(T value)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//增加值对象类型，只面向Array
- template&lt;typename T&gt; bool addSubitem(string name, T value)  //增加值对象类型，当this为Array时，name会被忽略
- string toString()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;//Json对象序列化为字符串
- bool isError()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//无效Json对象判定
- bool isNull()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;//null值判定
- bool isObject()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;//Object对象判定
- bool isArray()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//Array对象判定
- bool isNumber()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;//number值判定，Json内使用double型别存储number值
- bool isTrue()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;//true值判定
- bool isFalse()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//false值判定
- int toInt()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;//值对象转为int
- float toFloat()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;//值对象转为float
- double toDouble()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;//值对象转为double
- bool toBool()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;//值对象转为bool
- vector&lt;Json&gt; toVector()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;//数组对象转为vector
- bool extend(Json value)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//对象扩展
- bool concat(Json value)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//数组扩展
- bool push_front(Json value)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//数组压入队首
- bool push_back(Json value)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//数组压入队尾
- bool insert(int index, Json value)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//数组插入元素
- bool clear()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//清空
- void remove(const string &key)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//删除键值
- bool contains(const string& key)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//判断key是否存在
- string getValueType()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//获取值类型字符串表示
- Json getAndRemove(const string& key)&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//获取并删除
- std::vector<std::string> getAllKeys()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;//获取所有key
    
## 编程示例
简单使用示例
```
    Json subObject{{"math", 99},{"str", "a string."}};   //initializer_list方式构造Json对象

    //initializer_list方式构造Json对象, 并且可以嵌套
	Json mulitListObj{{"fkey", false},{"strkey","ffffff"},{"num2", 9.98}, {"okey", subObject}};

    Json subArray(JsonType::Array);                 //数组对象以initializer_list方式增加元素
	subArray.addSubitem({12,13,14,15});             //快速生成 [12,13,14,15] array json

    Json ajson(JsonType::Object);                   //新建Object对象，输入参数可以省略
    std::string data = "kevin";                     
    ajson.addSubitem("fail", false);              //增加false值对象
    ajson.addSubitem("name", data);               //增加字符串值对象
    ajson.addSubitem("school-en", "the 85th.");   
    ajson.addSubitem("age", 10);                  //增加number值对象，此处为整数
    ajson.addSubitem("scores", 95.98);            //增加number值对象，此处为浮点数，还支持long,long long
    ajson.addSubitem("nullkey", nullptr);         //增加null值对象，需要送入nullptr， NULL会被认为是整数0

    Json sub;                                       //新建Object对象
    sub.addSubitem("math", 99);                 
    ajson.addValueJson("subJson", sub);             //为ajson增加子Json类型对象，完成嵌套需要

    Json subArray(JsonType::Array);                 //新建Array对象，输入参数不可省略
    subArray.addSubitem("I'm the first one.");    //增加Array对象的字符串值子对象
    subArray.addSubitem("two", 2);                //增加Array对象的number值子对象，第一个参数会被忽略
    
    Json sub2;                            
    sub2.addSubitem("sb2", 222);

    subArray.addValueJson("subObj", sub2);          //为Array对象增加Object类子对象，完成嵌套需求
    
    ajson.addValueJson("array", subArray);          //为ajson增加Array对象，且这个Array对象本身就是一个嵌套结构

    std::cout << "ajson's string is : " << ajson.toString() << std::endl;    //输出ajson对象序列化后的字符串， 结果见下方

    string name = ajson["name"].toString();         //提取key为name的字符串值，结果为：kevin
    int oper = ajson["sb2"].toInt();                //提取嵌套深层结构中的key为sb2的整数值，结果为：222
    Json operArr = ajson["array"];                  //提取key为array的数组对象
    string first = ajson["array"][0].toString();    //提取key为array的数组对象的序号为0的值，结果为：I'm the first one.
```
mulitListObj序列化后结果为：
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
ajson序列化后结果为：
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
详情请参看demo.cpp或tests目录下的测试用例

## 项目地址
```
https://gitee.com/zhoutk/zjson
或
https://github.com/zhoutk/zjson
```

## 运行方法
该项目在vs2019, gcc7.5, clang12.0下均编译运行正常。
```
git clone https://github.com/zhoutk/zjson
cd zjson
cmake -Bbuild .

---windows
cd build && cmake --build .

---linux & mac
cd build && make

run zjson or ctest
```

## 相关项目

会有一系列项目出炉，网络服务相关，敬请期待...
> [zorm](https://gitee.com/zhoutk/zorm.git) (关系数据库的通用封装)
```
https://gitee.com/zhoutk/zorm
或
https://github.com/zhoutk/zorm
```