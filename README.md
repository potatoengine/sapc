sapc - IDL compiler
===================

By Sean Middleditch

https://github.com/potatoengine/sapc

Legal
-----

This is free and unencumbered software released into the public domain.

See LICENSE.md for more details.

Usage
-----

```
sapc [-o <input>] [-I<path>]... [-d <depfile>] [-h] [--] <input>
  -I<path>              Add a path to the search list for imports and includes
  -o|--output <ouput>   Output file path, otherwise prints to stdout
  -d|--deps <depfile>   Specify the path that a Make-style deps file will be written to for build system integration
  -h|--help             Print this help information
  <input>               Specify the input IDL (in compile mode) or input JSON (in template mode)
```

Input Schema
------------

Simplified PEG grammar.

```
file <- module pragma* statement*
statement <- attrdef / typedef / enumdef / import / include
module <- 'module' identifier ';'
pragma <- 'pragma' identifier ';'

import <- 'import' identifier ';'
include <- 'include' string EOL

value <- number / string / 'true' / 'false' / 'null'
number <- '-'? [0-9]+
string <- '"' <[^"\n]*> '"'

comment <- linecomment / blockcomment
linecomment <- ( '#' / '//' ) [^\n]*
blockcomment <- '/*' .* '*/'
identifier <- [a-zA-Z_][a-zA-Z0-9_]*

attrdef <- 'attribute' identifier ( '{' attrparam* '}' / ';' )
attrparam <- identifier identifier ( '=' value )? ';'

attributes <- ( '[' attribute ( ',' attribute )* ']' )+
attribute <- identifier ( '(' ( value ( ',' value )* )? ')' )?

typedef <- attributes? 'type' identifier ( ':' identifier )? ( '{' field* '}' / ';' )
field <- attributes? identifier identifier ( '=' value / '=' identifier )? ';'

enumdef <- attributes? 'enum' ( ':' identifier )? '{' enumvalue ( ',' enumvalue )* '}'
enumvalue <- identifier ( '=' number )?
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
type test {
    [cdecl("t_num")]
    int num = 0;
    flags flg = first;
};
```

Output Schema
-------------

See [https://raw.githubusercontent.com/potatoengine/sapc/master/schema/sap-1.schema.json]
