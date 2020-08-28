sapc Frequently Anticipated Questions
=====================================

Why another IDL specification?
------------------------------

All of the existing IDLs of which the author is aware are purpose-built and inextensible.

Microsoft's IDL primarily supports COM definitions. WebIDL is only for defining APIs. protoc
and its ilk are only for serialization and only in a specific format.

sapc is intended to be generic. Via attributes and user-defined types it can be extended to
define almost any structure or API, and by allowing users to write their own codegen
backends it can be extended to support any almost any format, language, framework, or purpose.

Why does it only generate JSON?
-------------------------------

Some IDL compilers have built-in codegen for various purpose-built solutions, such as
protoc being able to generate serialization code in various languages. However, adding a new
language to protoc requires modifying the protoc compiler itself; it is inextensible.

Wven with codegen, there are use cases were it is handy to be able to introspect the IDL
definition itself without generating any particular code.

A strong JSON-driven templating tool would certainly be handy for developing sapc-powered
code generators, but such a tool is not the domain of sapc itself.

With the sapc approach, projects can develop their own codegen solutions in Python or C#
or JavaScript or even C++ without needing to handle the complexity of parsing, validating,
and compiling sap modules.

Why not develop sapc as a library to a language with decent template engines like Python?
-----------------------------------------------------------------------------------------

Languages like Python do offer some top-notch template engines that make codegen particularly
easy. However, not every project would necessary want to add Python as a build dependency.

Some projects might want to use C# or JavaScript or Rust or so on as their primary build
dependencies and tools. Developing sapc as a library for a specific language would limit
sapc's applicability, and without good reason.

Any language with solid templating capability, like Python, also has solid JSON ingestion
support as well. Reading in sapc's generated JSON files is possibly the easiest part of
writing a sapc-based code generator. Binding sapc in directly as an API wouldn't really
make anything particularly easier.

Why not develop sapc as a C library API that can be bound into other languages?
-------------------------------------------------------------------------------

The "API" of sapc is the JSON schema it produces. Other than in C itself and perhaps C++,
it isn't any easier to consume and use a C API than it is to consume and use a JSON file.

Why isn't there syntax to support porting of protobuffers or flatbuffers IDL?
-----------------------------------------------------------------------------

sapc isn't meant to occupy the exact same space as those IDLs. Many of the author's own
uses for sapc have absolutely no need for protobuffer-like tags. Attributes allow that
data to be entered where necessary, but consuming limited syntax space for bespoke
features is not within the design goals of sapc.
