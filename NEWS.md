sapc
====

Version 0.11-beta (WIP)
-----------------------

 - Removed `type` keyword
 - Added pointer support via `name*`
 - Added array-of-pointer via `name*[]`

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
