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
sapc [-c] -i <input> [-o <input>] [-d <depfile>] [-h]
  -c|--compile          Compile a sapc IDL file to JSON (default option)
  -i|--input <input>    Specify the input IDL (in compile mode) or input JSON (in template mode)
  -o|--output <ouput>   Output file path, otherwise prints to stdout
  -d|--deps <depfile>   Specify the path that a Make-style deps file will be written to for build system integration
  -h|--help             Print this help information
```

Input Schema
------------

```
file <- statement*
statement <- attrdef / typedef / import / include

import <- 'import' identifier ';'
include <- 'include' string EOL

value <- number / string / 'true' / 'false' / 'null'
number <- '-'? [0-9]+
string <- '"' <[^"\n]*> '"'

comment <- '#' [^\n]*
identifier <- [a-zA-Z_][a-zA-Z0-9_]*

attrdef <- 'attribute' identifier ( '{' attrparam* '}' / ';' )
attrparam <- identifier identifier ( '=' value )? ';'

attributes <- ( '[' attribute ( ',' attribute )* ']' )+
attribute <- identifier ( '(' ( value ( ',' value )* )? ')' )?

typedef <- attributes? 'type' identifier ( ':' identifier )? ( '{' field* '}' / ';' )
field <- attributes? identifier identifier ( '=' value )? ';'
```

Example:

```
type string;
type int;

attribute cdecl { string name; }

[cdecl("test_t")]
type test {
	[cdecl("t_num")]
	int num = 0;
};
```

Output Schema
-------------

Output is a JSON file (schema still in flux).