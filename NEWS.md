sapc
====

Version 0.16
------------

 - JSON Schema fixes and hardening
 - Set CMake variables `SAPC_VALIDATE_SCHEMA_TESTS` and optionally `SAPC_PATH_AJV_BIN` to enable schema validation tests (requires ajv-cli from npm)
 - Fix build on libc++-11 and any other future strict C++ standard library implementations

Version 0.15
------------

 - Importing modules includes their custom tags in grammar parsing
 - Fix enums to apply custom tags
 - Custom tags for unions, aliases, and constants
 - Renamed $sapc, $sapc.customtag, and $sapc.typeid to $core, $customtag, $typeid, respectively
 - Renamed 'generic' type kind to 'typeparam', 'generics' array to 'typeParams', and old 'typeParams' to 'typeArgs'
 - Optional static array sizes, e.g. `int[5] values;`
 - Fix error output to show correct end position
 - Add "fake" MSC-style error codes to errors and warnings so VS and VSCode match on them by default
 - Types declared with `using` but no alias are now "simple" types instead of "alias" in the output schema

Version 0.14-beta
-----------------

 - Fix memory access bug in processing of specialized generics

Version 0.13-beta
-----------------

 - Fix attributes in JSON schema output
 - Use qualified name for typename references in JSON output
 - Ensure transitively imported types are exposed in the JSON output
 - Simplify output schema extensively
 - Fix listing of constants in namespace entries

Version 0.12-beta
-----------------

 - Fix include path searching with `-I` option
 - Expanded tests; internal change only

Version 0.11-beta
-----------------

 - JSON schema changes
 - Removed `type` keyword
 - Added pointer support via `name*`
 - Added array-of-pointer via `name*[]`
 - Enum field values must be qualified with `.`, e.g. `my_enum e = my_enum.value;`
 - Attributes can have `typename` fields
 - Module scope constants with `const`
 - List initialization of values, e.g. `int[] vals = { 1, 2, 3 };` or `Vec zero = { 0, 0 };`
 - Rewrote compiler to use an AST
 - Minor tweaks to which entities can have annotations
 - Namespaces
 - Generics, e.g. `struct Map<K, V> { T[] keys; V[] values; }` and `Map<string, int> nameValues;`
 - Custom tags for structs and enums, e.g. `use message : struct` and `message ack { int time; }`

Version 0.10-beta
-----------------

 - Generated depfiles always write paths relative to the working directory

Version 0.9-beta
----------------

 - union support
 - Attribute usages are now called annotations (no change to attribute keyword)
 - JSON output includes source location info
 - Various cleanups, simplifications, and other improvements to JSON output and schema

Version 0.8-alpha
-----------------

 - Added enum enumerations
 - Removed include to simplify grammar
 - Removed the * pointer modifier
 - The module declaration doesn't have to be first in the file anymore
 - Simplified core types to just string, int, float, bool, and byte
 - Allow attributes on module declaration
 - Replace type with struct
 - Use type for simple type declarations with no fields or base type
 - Rewrite parser to hand-written recursive descent

Version 0.7-alpha
-----------------

 - Modules and dependencies
 - Import/include search paths
 - Simplify cli parameters
 - Improved support of using spac via CMake's add_subdirectory
 - C and C++-style commands (// and /*...*/)
 - Improved error reporting in grammar
 - Fields can be marked as pointers or arrays
 - Several core built-in types in the sap schema
    
Version 0.6-alpha
-----------------

- Added JSON schema for output
- Implemented module/include search paths
- Imported/included definitions won't conflict with themselves on duplicate imports/includes

Version 0.5-alpha
-----------------

- Initial Release
