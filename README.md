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

Simplified PEG grammar.

```
file <- statement*
statement <- module / import / definition
definition <- attribute / type / struct / enum / union

import <- 'import' identifier ';'

value <- number / string / 'true' / 'false' / 'null'
number <- '-'? [0-9]+
string <- '"' <[^"\n]*> '"'

comment <- linecomment / blockcomment
linecomment <- ( '#' / '//' ) [^\n]*
blockcomment <- '/*' .* '*/'
identifier <- [a-zA-Z_][a-zA-Z0-9_]*
type_info <- identifier ( '[' ']' )?

annotations <- ( '[' annotation ( ',' annotation )* ']' )+
annotation <- identifier ( '(' ( value ( ',' value )* )? ')' )?

attribute <- 'attribute' identifier ( '{' attribute_param* '}' / ';' )
attribute_param <- type_info identifier ( '=' value )? ';'

module <- annotations? 'module' identifier ';'

type <- annotations? 'type' identifier ';'

struct <- annotations? 'struct' identifier ( ':' identifier )? '{' struct_field* '}'
struct_field <- annotations? type_info identifier ( '=' value / '=' identifier )? ';'

union <- annotations? 'union' identifier '{' union_element+ '}'
union_element <- type_info identifier ';'

enum <- annotations? 'enum' ( ':' identifier )? '{' enum_value ( ',' enum_value )* '}'
enum_value <- identifier ( '=' number )?
```

Example:

```
module example;

type string;
type int;

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
