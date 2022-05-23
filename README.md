# zjson

## 介绍
从node.js转到c++，特别怀念在js中使用json那种畅快感。在c++中也使用过了些库，但提供的接口使用方式，总不是习惯，很烦锁，接口函数太多，不直观。参考了很多库，如：rapidjson, cJson, CJsonObject, drleq-cppjson, json11等，受cJson的数据结构启发很大，决定用C++手撸一个。最后因为数据存储需要不区分型别，又要能知道其型别，所以选择了C++17才支持的std::variant以及std::any，最终，C++版本定格在c++17，本库设计为单头文件，且不依赖c++标准库以外的任何库。

## 项目名称说明
本人姓名拼音第一个字母z加上josn，即得本项目名称zjson，没有其它任何意义。我将编写一系列以z开头的相关项目，命名是个很麻烦的事，因此采用了这种简单粗暴的方式。

## 设计思路 
简单的接口函数、简单的使用方法、灵活的数据结构、尽量支持链式操作。使用模板技术，得以完成最简设计，为Json对象增加值的方法只有两个，AddValueBase和AddValueJson。采用链表结构（向cJSON致敬）来存储Json对象，请看我下面的数据结构设计，表头与后面的结点，都用使用一致的结构，这使得在索引操作([])时，可以进行链式操作。

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
- [ ] std::move语义
  
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
- Json(JsonType type = JsonType::Object)             //默认构造函数，生成Object或Array类型的Json对象
- Json(const Json& origin)                           //复制构造函数
- Json& operator = (const Json& origin)              //赋值操作
- Json operator[](const int& index)                  //Json数组对象元素查询
- Json operator[](const string& key)                 //Json Object 对象按key查询
- bool AddValueJson(Json& obj)                       //增加子Json类对象类型, 只面向Array
- bool AddValueJson(string name, Json& obj)          //增加子Json类对象类型，当obj为Array时，name会被忽略
- template<typename T> bool AddValueBase(T value)    //增加值对象类型，只面向Array
- template<typename T> bool AddValueBase(string name, T value)  //增加值对象类型，当this为Array时，name会被忽略
- string toString()                                  //Json对象序列化为字符串
- bool isError()                                     //无效Json对象判定
- bool isNull()                                      //null值判定
- bool isObject()                                    //Object对象判定
- bool isArray()                                     //Array对象判定
- bool isNumber()                                    //number值判定，Json内使用double型别存储number值
- bool isTrue()                                      //true值判定
- bool isFalse()                                     //false值判定
- int toInt()                                        //值对象转为int
- float toFloat()                                    //值对象转为float
- double toDouble()                                  //值对象转为double
- bool toBool()                                      //值对象转为bool
    
## 编程示例
简单使用示例
```
    Json ajson(JsonType::Object);                   //新建Object对象，输入参数可以省略
    std::string data = "kevin";                     
    ajson.AddValueBase("fail", false);              //增加false值对象
    ajson.AddValueBase("name", data);               //增加字符串值对象
    ajson.AddValueBase("school-en", "the 85th.");   
    ajson.AddValueBase("age", 10);                  //增加number值对象，此处为整数
    ajson.AddValueBase("scores", 95.98);            //增加number值对象，此处为浮点数，还支持long,long long
    ajson.AddValueBase("nullkey", nullptr);         //增加null值对象，需要送入nullptr， NULL会被认为是整数0

    Json sub;                                       //新建Object对象
    sub.AddValueBase("math", 99);                 
    ajson.AddValueJson("subJson", sub);             //为ajson增加子Json类型对象，完成嵌套需要

    Json subArray(JsonType::Array);                 //新建Array对象，输入参数不可省略
    subArray.AddValueBase("I'm the first one.");    //增加Array对象的字符串值子对象
    subArray.AddValueBase("two", 2);                //增加Array对象的number值子对象，第一个参数会被忽略
    
    Json sub2;                            
    sub2.AddValueBase("sb2", 222);

    subArray.AddValueJson("subObj", sub2);          //为Array对象增加Object类子对象，完成嵌套需求
    
    ajson.AddValueJson("array", subArray);          //为ajson增加Array对象，且这个Array对象本身就是一个嵌套结构

    std::cout << "ajson's string is : " << ajson.toString() << std::endl;    //输出ajson对象序列化后的字符串， 结果见下方

    string name = ajson["name"].toString();         //提取key为name的字符串值，结果为：kevin
    int oper = ajson["sb2"].toInt();                //提取嵌套深层结构中的key为sb2的整数值，结果为：222
    Json operArr = ajson["array"];                  //提取key为array的数组对象
    string first = ajson["array"][0].toString();    //提取key为array的数组对象的序号为0的值，结果为：I'm the first one.
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
```

```