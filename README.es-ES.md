

# ZJSON   &emsp;&emsp;  [中文介绍](README_CN.md)

[![JSONTestSuite](https://img.shields.io/badge/JSONTestSuite-283%2F283%20(100%25)-brightgreen)](docs/jsontestsuite_results.txt)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)](https://isocpp.org/)
[![header-only](https://img.shields.io/badge/header--only-yes-success)](src/zjson.hpp)
[![license: MIT](https://img.shields.io/badge/license-MIT-green)](LICENSE)

> Conformidad verificada contra el corpus completo de [`JSONTestSuite`](https://github.com/nst/JSONTestSuite)
> `test_parsing/` — **95/95** casos `y_` (debe aceptar) y **188/188** casos `n_` (debe rechazar)
> pasan en modo estricto. Consulte [`docs/jsontestsuite_results.txt`](docs/jsontestsuite_results.txt).
>
> La mini-suite [`JSON_checker`](thirds/JSON-c/README) incluida también está cubierta por
> pruebas de regresión automatizadas: **36/36** archivos del corpus bajo `thirds/JSON-c/test/`
> pasan con la semántica original de la suite (objeto/matriz de nivel superior y límite de anidamiento de 19 niveles).

Las recientes adiciones a la API incluyen formato bonito con indentación `toString(indent)`, igualdad/Desigualdad semánticas `==/!=`, iteración `begin/end/cbegin/cend` con emparejamiento estructurado, `ParseOptions` para claves duplicadas, JSON Pointer `at("/a/b/0")`, JSON Merge Patch / JSON Patch con `mergePatch(...)` y `applyPatch(..., err)`, ganchos `to_json` / `from_json` basados en ADL, y asignación de slab y almacenamiento de strings parseados respaldado por arena.

El informe de benchmark actual de Windows/MSVC Release comparando zjson con nlohmann/json, RapidJSON y simdjson está disponible en [`docs/性能测试报告.md`](docs/性能测试报告.md).

## Introducción
De node.js a C++. Extrañaba especialmente la comodidad de usar JSON en JavaScript, así que intenté crear la mía. He utilizado muchas bibliotecas, como: rapidjson, cJson, CJsonObject, drleq cppjson, json11, etc. La estructura de datos de zjson está fuertemente inspirada por cJOSN. La parte de análisis sintáctico (parsing) se basa en json11, ¡gracias! Finalmente, debido a que el almacenamiento de datos no solo necesita distinguir valores, sino también conocer sus tipos, elegí `std::variant` y `std::any`, soportados por C++17. La versión de C++ se fijó en C++17. Esta biblioteca está diseñada como un solo archivo de cabecera, sin depender de ninguna otra librería que no sea la biblioteca estándar de C++.

## Ideas de diseño  
Funciones de interfaz simples, métodos de uso sencillos, estructuras de datos flexibles y soporte para operaciones encadenadas tanto como sea posible. Implementación del diseño más simple utilizando tecnología de plantillas. Agregar un objeto hijo a Json solo requiere una función: `addSubitem`, que identifica automáticamente si es un valor o un objeto Json hijo. El objeto Json se almacena en una estructura de lista enlazada (参照 cJSON). Consulte el diseño de mi estructura de datos a continuación. El encabezado y los siguientes nodos utilizan la misma estructura, lo que permite operaciones encadenadas durante las operaciones de índice ([]).

## Progreso del proyecto
Actualmente, el proyecto ha completado la mayoría de las funciones. Consulte la lista de tareas para más detalles. 

lista de tareas：
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
  
## Estructura de datos

### Tipo de nodo Json   
> Para uso interno, el tipo de dato se utiliza únicamente dentro de la clase Json
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
### Definición de nodo Json
```
class Json {
    Json* brother;       //like cJSON's next
    Json* child;         //chile node, for object type
    Type type;           //node type
    std::variant <int, bool, double, string> data;   //node's data
    string name;         //node's key
}
```
## Interfaz
Tipo Object, solo soporta Object y Array.
```
enum class JsonType
{
    Object = 6,
    Array = 7
};
```
Lista de Api
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
    
## Ejemplos
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
Resultado de mulitListObj：
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
Resultado de ajson：
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
Para una descripción detallada, consulte `demo.cpp` o las pruebas unitarias en el directorio `tests`.

## Comportamiento definido por la implementación

La siguiente tabla documenta el comportamiento de zjson en entradas donde la especificación JSON (RFC 8259) no exige un resultado particular, o donde las implementaciones comunes difieren. Estos corresponden a la categoría `i_*` (definida por la implementación) en el [JSONTestSuite](https://github.com/nst/JSONTestSuite).

| Comportamiento | zjson | Notas |
|---|---|---|
| **Claves duplicadas en objetos** | Configur`Configurable`; por defecto, mantener último | `ParseOptions::DuplicateKeyPolicy` admite `KeepFirst`, `KeepLast`, y `Reject` |
| **Precisión numérica** | IEEE 754 `double` | Se parsea mediante `strtod`; los enteros que caben en `int` usan la ruta rápida `atoi` |
| **Números muy grandes** | `±Infinity` | Resultado de `strtod`; sin error |
| **Números muy pequeños** | `0.0` o subnormal | Resultado de `strtod`; sin error |
| **Profundidad máxima de anidamiento** | 100 niveles | Configurables mediante `max_depth`; input más profundo es rechazado |
| **BOM UTF-8 (U+FEFF)** | No se consume | Los bytes BOM causan un error de análisis (no se tratan como espacio en blanco) |
| **Comentarios (`//` y `/* */`)** | Aceptados en modo extensión | `ParseJson()` permite comentarios; `ParseJsonStrict()` los rechaza |
| **Comas finales** | Rechazadas | `[1,]` y `{"a":1,}` producen errores de análisis |
| **Ceros a la izquierda** | Rechazados | `012`, `-01` producen errores de análisis |
| **Literales `NaN` / `Infinity`** | Rechazados | No son valores JSON válidos |
| **Strings entre comillas simples** | Rechazados | Solo se aceptan strings entre comillas dobles |
| **Claves de objetos sin comillas** | Rechazados | Las claves deben ser strings entre comillas dobles |
| **Surogados sueltos en `\uXXXX`** | Se codifican tal cual en UTF-8 | No se rechazan en modo extensión; use `ParseJsonStrictUtf8()` para validación a nivel de byte |
| **Validación de bytes UTF-8** | Desactivada por defecto | Habilite mediante `ParseJsonStrictUtf8()` para rechazar secuencias de bytes no válidas |
| **Longitud máxima de string** | Limitada por `std::string` / memoria | Sin límite explícito |
| **Bytes nulos en strings** | Aceptados mediante `\u0000` | Los bytes `0x00` raw en el flujo de entrada causan problemas de terminación de string en APIs de strings C |

### Modos de análisis

| API | Comentarios | Validación UTF-8 | Caso de uso |
|---|---|---|---|
| `ParseJson(input, err)` | Permitidos | Desactivada | Uso general con extensiones |
| `ParseJsonStrict(input, err)` | Rechazados | Desactivada | Estructura estricta RFC 8259 |
| `ParseJsonStrictUtf8(input, err)` | Rechazados | Activada | Cumplimiento total RFC 8259 + UTF-8 |

## Sitio`Sitios web del proyecto`
```
https://gitee.com/zhoutk/zjson
or
https://github.com/zhoutk/zjson
```

## Instrucciones de ejecución
El proyecto se compila con éxito en vs2019, gcc7.5, clang12.0.  
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

## Proyectos asociados

> [zorm](https://gitee.com/zhoutk/zorm.git) (General Encapsulation of Relational Database)
```
https://gitee.com/zhoutk/zorm
or
https://github.com/zhoutk/zorm
```
