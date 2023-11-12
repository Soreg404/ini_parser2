# ini_parser2

C++17 (&lt;filesystem&gt;)

## single file lib
```c++
// once in your project
#define INI_IMPLEMENTATION
#include <ini.hpp>

// everywhere else just
#include <ini.hpp>
```

## usage
```c++
// open and parse file
IniFile file("your_file.ini");

// read & write
std::string value = file["section"]["key"];
file["section"]["another key"] = "text";
file["section"]["new key"] = "text";

// possible remove
file.remove("different section");
file["section"].remove("some key");

// save
file.save();
