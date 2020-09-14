# sapc - by Sean Middleditch
# This is free and unencumbered software released into the public domain.
# See LICENSE.md for more details.
import sys
import json
import argparse
import platform
import re
from os import path
from datetime import datetime

def annotation(el, name, argname, default=None):
    if el is not None and 'annotations' in el and name in el['annotations']:
        return el['annotations'][name][argname]
    else:
        return default

cxx_keywords = ['void', 'nullptr',
    'char', 'int', 'short', 'long', 'signed', 'unsigned', 'bool',
    'float', 'double',
    'if', 'do', 'while', 'switch', 'case', 'for',
    'struct', 'class', 'using', 'template', 'typename', 'typedef', 'const',
    'default', 'auto', 'namespace', 'sizeof', 'alignof', 'constexpr', 'constinit', 'consteval']
def identifier(name):
    clean = re.sub(r'[^a-zA-Z0-9_]', '_', name)
    id = clean + '_' if clean == '' or re.match('^[0-9]', clean) else clean
    legal = id + '_' if id in cxx_keywords else id
    return legal
cxx_type_map = {'string': 'std::string',
                'bool': 'bool', 'byte': 'unsigned char',
                'int': 'int', 'float': 'float'}

def cxxname(el):
    if el['name'] in cxx_type_map:
        return cxx_type_map[el['name']]
    else:
        return annotation(el, name='cxxname', argname='name', default=identifier(el['name']))
def ignored(el): return annotation(el, name='ignore', argname='ignored', default=False)
def namespace(el): return 'sapc_attr' if 'is_attribute' in el and el['is_attribute'] else 'sapc_type'

def encode(el):
    if isinstance(el, str):
        return '"' + el + '"'
    elif isinstance(el, bool):
        return 'true' if el else 'false'
    elif el is None:
        return 'nullptr'
    else:
        return +el

def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument('-i', '--input', type=argparse.FileType(mode='r', encoding='utf8'), required=True)
    parser.add_argument('-o', '--output', type=argparse.FileType(mode='w', encoding='utf8'))
    args = parser.parse_args(argv[1:])

    doc = json.load(args.input)
    args.input.close()

    if not args.output:
        args.output = sys.stdout

    print(f'// Generated file ** DO NOT EDIT **', file=args.output)
    print(f'//  from: {path.basename(args.input.name)}', file=args.output)
    print(f'//  with: {path.basename(argv[0])}', file=args.output)
    print(f'//  time: {datetime.utcnow()} UTC', file=args.output)
    print(f'//  node: {platform.node()}', file=args.output)

    print(f'#if !defined(INCLUDE_GUARD_SAPC_{identifier(doc["module"]).upper()})', file=args.output)
    print(f'#define INCLUDE_GUARD_SAPC_{identifier(doc["module"]).upper()} 1', file=args.output)
    print('#pragma once', file=args.output)
    print('#include <string>', file=args.output)
    print('#include <memory>', file=args.output)
    print('#include <vector>', file=args.output)
    print('#include <any>', file=args.output)

    types = doc['types']
    exports = doc['exports']

    for annotation, annotation_args in doc['annotations'].items():
        print(f'// annotation: {annotation}({",".join([k+":"+encode(v) for k,v in annotation_args.items()])})', file=args.output)
        
    if 'imports' in doc:
        for module in doc['imports']:
            print(f'#include "{module}.h"', file=args.output)

    for typename in exports:
        type = types[typename]
        if ignored(type): continue

        print(f'namespace {namespace(type)} {{', file=args.output)

        name = cxxname(type)

        basetype = types[type['base']] if 'base' in type else None

        if 'location' in type:
            loc = type['location']
            if 'line' in loc and 'column' in loc:
                print(f'  // {loc["filename"]}({loc["line"]},{loc["column"]})', file=args.output)
            elif 'line' in loc:
                print(f'  // {loc["filename"]}({loc["line"]})', file=args.output)
            else:
                print(f'  // {loc["filename"]}', file=args.output)

        if type['category'] == 'enum':
            if basetype:
                print(f'  enum class {name} : {cxxname(basetype)} {{', file=args.output)
            else:
                print(f'  enum class {name} {{', file=args.output)

            for value in type['values']:
                print(f'    {identifier(value["name"])} = {encode(value["value"])},', file=args.output)
        else:
            if basetype:
                print(f'  struct {name} : {cxxname(basetype)} {{', file=args.output)
            else:
                print(f'  struct {name} {{', file=args.output)

            if type['category'] != 'opaque':
                for fieldname in type['order']:
                    field = type['fields'][fieldname]
                    if ignored(field): continue

                    field_type = types[field["type"]]
                    field_cxxtype = cxxname(field_type)
                    if "is_pointer" in field and field["is_pointer"]:
                        field_cxxtype = f'std::unique_ptr<{field_cxxtype}>'
                    if "is_array" in field and field["is_array"]:
                        field_cxxtype = f'std::vector<{field_cxxtype}>'

                    if ('default' in field):
                        if field_type['category'] == 'enum':
                            print(f'    {field_cxxtype} {cxxname(field)} = ({field_cxxtype}){encode(field["default"])};', file=args.output)
                        else:
                            print(f'    {field_cxxtype} {cxxname(field)} = {encode(field["default"])};', file=args.output)
                    else:
                        print(f'    {field_cxxtype} {cxxname(field)};', file=args.output)

        print(f'  }};', file=args.output)
        print(f'}}', file=args.output)

    print(f'inline void sapc_test_{identifier(doc["module"])}() {{', file=args.output)
    if 'imports' in doc:
        for module in doc['imports']:
            print(f'  sapc_test_{identifier(module)}();', file=args.output)
    for typename in exports:
        type = types[typename]
        if ignored(type): continue

        name = cxxname(type)
        print(f'  [[maybe_unused]] {namespace(type)}::{name} val_{identifier(name)} = {{}};', file=args.output)
    print('}', file=args.output)

    print('#endif', file=args.output)

    args.output.close()

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
