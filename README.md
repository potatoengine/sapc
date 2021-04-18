sapc - IDL compiler
===================

By Sean Middleditch

[github.com/potatoengine/sapc](https://github.com/potatoengine/sapc)

Legal
-----

This is free and unencumbered software released into the public domain.

See [LICENSE.md](./LICENSE.md) for more details.

Usage
-----

```
sapc [-o <input>] [-I<path>]... [-d <depfile>] [-h] [--] <input>
  -I<path>              Add a path to the search list for imports
  -o|--output <ouput>   Output file path, otherwise prints to stdout
  -d|--deps <depfile>   Specify the path that a Make-style deps file will be written to for build system integration
  -h|--help             Print this help information
  <input>               Specify the input IDL file
```

Input Schema
------------

Example:

```
module example;

using string;
using int;

attribute cdecl { string name; }

enum flags {
    none,
    first = 10,
    last = 20
}

[cdecl("test_t")]
struct test {
    [cdecl("t_num")]
    int num = 0;
    flags flg = first;
};
```

Output Schema
-------------

See [sap-1.schema.json](https://potatoengine.github.io/sapc/schema/sap-1.schema.json)
