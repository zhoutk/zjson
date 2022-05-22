# zjson

## 介绍
从node.js转到c++，特别怀念在js中使用json那种畅快感。在c++中也使用过了些库，但提供的接口使用方式，总不是习惯，很烦锁，接口函数太多，不直观。参考了很多库，如：rapidjson, cJson, CJsonObject, drleq-cppjson, json11等，受cJson的数据结构启发很大，决定用C++手撸一个。最后因为数据存储需要不区分型别，又要能知道其型别，所以选择了C++17才支持的std::variant以及std::any，最终，C++版本定格在c++17，本库设计为单头文件，且不依赖c++标准库以外的任何库。

## 项目名称说明
本人姓名拼音第一个字母z加上josn，即得本项目名称zjson，没有其它任何意义。我将编写一系列以z开头的相关项目，命名是个很麻烦的事，因此采用了这种简单粗暴的方式。

## 设计思路 
简单的接口函数、简单的使用方法、灵活的数据结构、尽量支持链式操作。使用模板技术，使用给Json对象增加值的方法只有两个，AddValueBase和AddValueJson。采用链表结构（向cJSON致敬）来存储Json对象，请看我下面的数据结构设计，表头与后面的结点，都用使用一致的结构，这使得在索引操作([])时，可以进行链式操作。

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

    
## 项目地址
```
https://gitee.com/zhoutk/zjson
或
https://github.com/zhoutk/zjson
```

## 运行方法

```

```

## 相关项目

会有一系列项目出炉，网络服务相关，敬请期待...
```

```