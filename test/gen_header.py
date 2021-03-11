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

def cxxname(el): return annotation(el, name='cxxname', argname='name', default=cxx_type_map[el['name']] if el['name'] in cxx_type_map else identifier(el['name']))
def ignored(el): return annotation(el, name='ignore', argname='ignored', default=False)
def namespace(el): return 'sapc_attr' if 'is_attribute' in el and el['is_attribute'] else 'st'

def encode(el):
    if isinstance(el, str):
        return '"' + el + '"'
    elif isinstance(el, bool):
        return 'true' if el else 'false'
    elif isinstance(el, dict) and el['kind'] == 'enum':
        return f'{identifier(el["type"])}::{identifier(el["name"])}'
    elif isinstance(el, dict) and el['kind'] == 'typename':
        return f'typeid({identifier(el["type"])})'
    elif isinstance(el, list):
        return f'{{{",".join(encode(v) for v in el)}}}'
    elif el is None:
        return 'nullptr'
    else:
        return str(+el)

def field_cxxtype(types, type_info):
    if 'kind' in type_info:
        if type_info['kind'] == 'typename':
            return 'std::type_info'
        if type_info['kind'] == 'array':
            field_type = type_info['of']
            return f'std::vector<{field_cxxtype(types, field_type)}>'
        elif type_info['kind'] == 'pointer':
            field_type = type_info['to']
            return f'std::unique_ptr<{field_cxxtype(types, field_type)}>'
    else:
        field_type = types[type_info]
        return cxxname(field_type)

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
        kind = type['kind']

        basetype = types[type['base']] if 'base' in type else None
        basespec = f' : {cxxname(basetype)}' if basetype is not None else ''

        if 'location' in type:
            loc = type['location']
            if 'line' in loc and 'column' in loc:
                print(f'  // {loc["filename"]}({loc["line"]},{loc["column"]})', file=args.output)
            elif 'line' in loc:
                print(f'  // {loc["filename"]}({loc["line"]})', file=args.output)
            else:
                print(f'  // {loc["filename"]}', file=args.output)

        if kind == 'enum':
            print(f'  enum class {name}{basespec} {{', file=args.output)

            for ename in type['names']:
                value = type['values'][ename]
                print(f'    {identifier(ename)} = {value},', file=args.output)
        else:
            print(f'  struct {name}{basespec} {{', file=args.output)

            if kind != 'opaque':
                for fieldname in type['order']:
                    field = type['fields'][fieldname]
                    if ignored(field): continue

                    if 'default' in field:
                        print(f'    {field_cxxtype(types, field["type"])} {cxxname(field)} = {encode(field["default"])};', file=args.output)
                    else:
                        print(f'    {field_cxxtype(types, field["type"])} {cxxname(field)};', file=args.output)

        print(f'  }};', file=args.output)
        print(f'}}', file=args.output)
        
    for constname in doc['constants']:
        constant = doc['constants'][constname]
        if ignored(constant): continue;

        print(f'namespace {namespace(constant)} {{', file=args.output)

        name = cxxname(constant)

        if 'location' in constant:
            loc = constant['location']
            if 'line' in loc and 'column' in loc:
                print(f'  // {loc["filename"]}({loc["line"]},{loc["column"]})', file=args.output)
            elif 'line' in loc:
                print(f'  // {loc["filename"]}({loc["line"]})', file=args.output)
            else:
                print(f'  // {loc["filename"]}', file=args.output)

        print(f'  constexpr {field_cxxtype(types, constant["type"])} {cxxname(constant)} = {encode(constant["value"])};', file=args.output)
        print(f'}}', file=args.output)

    print('#endif', file=args.output)

    args.output.close()

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
