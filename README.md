# ZJSON   &emsp;&emsp;  [中文介绍](README_CN.md)  

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
- [x] Remove key
- [x] std::move
- [ ] performance test and comparison for recursive version
- [ ] algorithm non recursion
- [ ] performance test and comparison again
  
## Data structure

### Json node type   
> For internal use, the data type is only used inside the Json class
```
enum Type {
    Error,                //error or a invalid Json
    False,                //Json value type - false
    True,                 //Json value type - true
    Null,                 //Json value type - null
    Number,               //Json value type - numerial
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
    std::variant <int, bool, double, string> data;   //node's data
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
- bool isTrue()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;
- bool isFalse()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;
- int toInt()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;
- float toFloat()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;&nbsp;
- double toDouble()&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&nbsp;
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
    
## Examples
```
    Json subObject{{"math", 99},{"str", "a string."}};   
    Json mulitListObj{{"fkey", false},{"strkey","ffffff"},{"num2", 9.98}, {"okey", subObject}};
    Json subArray(JsonType::Array);                 
    subArray.addSubitem({12,13,14,15});            

    Json ajson(JsonType::Object);                
    std::string data = "kevin";                     
    ajson.addSubitem("fail", false);             
    ajson.addSubitem("name", data);              
    ajson.addSubitem("school-en", "the 85th.");   
    ajson.addSubitem("age", 10);                  
    ajson.addSubitem("scores", 95.98);            
    ajson.addSubitem("nullkey", nullptr);         

    Json sub;                                  
    sub.addSubitem("math", 99);                 
    ajson.addValueJson("subJson", sub);           

    Json subArray(JsonType::Array);              
    subArray.addSubitem("I'm the first one.");   
    subArray.addSubitem("two", 2);               
    
    Json sub2;                            
    sub2.addSubitem("sb2", 222);

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

run zjson or ctest
```

## Associated projects

> [zorm](https://gitee.com/zhoutk/zorm.git) (General Encapsulation of Relational Database)
```
https://gitee.com/zhoutk/zorm
or
https://github.com/zhoutk/zorm
```