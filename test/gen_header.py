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
    if el is not None and 'annotations' in el:
        annotations = el['annotations']
        for anno in annotations:
            if anno["type"] == name:
                return anno["args"][argname]
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

def namespace(el):
    cxxns = annotation(el, name='cxxnamespace', argname='ns', default=None)
    if cxxns is not None:
        return cxxns
    elif 'kind' in el and el['kind'] == 'generic':
        return None
    elif el['name'] in cxx_type_map:
        return None
    elif annotation(el, name='cxxname', argname='name') is not None:
        return None
    elif 'namespace' in el:
        return 'st::' + '::'.join(identifier(c) for c in el['namespace'].split('.'))
    elif 'kind' in el and el['kind'] == 'attribute':
        return 'st_attr'
    else:
        return 'st'

def qualified(el):
    ns = namespace(el)
    name = cxxname(el)
    return name if ns is None else (ns + '::' + name)

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

def field_cxxtype(types, name):
    field_type = types[name]
    if field_type['kind'] == 'typename':
        return 'std::type_index'
    if field_type['kind'] == 'array':
        field_type = field_cxxtype(types, field_type['refType'])
        return f'std::vector<{field_type}>'
    elif field_type['kind'] == 'pointer':
        field_type = field_cxxtype(types, field_type['refType'])
        return f'std::unique_ptr<{field_type}>'
    elif field_type['kind'] == 'specialized':
        ref_type = field_cxxtype(types, field_type['refType'])
        param_types = [field_cxxtype(types, param) for param in field_type['typeParams']]
        return f'{ref_type}<{", ".join(param_types)}>'
    else:
        ns = namespace(field_type)
        name = cxxname(field_type)
        return name if ns is None else (ns + '::' + name)

def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument('-i', '--input', type=argparse.FileType(mode='r', encoding='utf8'), required=True)
    parser.add_argument('-o', '--output', type=argparse.FileType(mode='w', encoding='utf8'))
    args = parser.parse_args(argv[1:])

    doc = json.load(args.input)
    args.input.close()

    if not args.output:
        args.output = sys.stdout

        
    def banner(text):
        print(f'// --{re.sub(".", "-", text)}--', file=args.output)
        print(f'//  {text}', file=args.output)
        print(f'// --{re.sub(".", "-", text)}--\n', file=args.output)

    banner('Generated file ** DO NOT EDIT **')

    print(f'// from: {path.basename(args.input.name)}', file=args.output)
    print(f'// with: {path.basename(argv[0])}', file=args.output)
    print(f'// time: {datetime.utcnow()} UTC', file=args.output)
    print(f'// node: {platform.node()}', file=args.output)

    print(f'\n#if !defined(INCLUDE_GUARD_SAPC_{identifier(doc["module"]["name"]).upper()})', file=args.output)
    print(f'#define INCLUDE_GUARD_SAPC_{identifier(doc["module"]["name"]).upper()} 1', file=args.output)
    print('#pragma once', file=args.output)
    print('#include <any>', file=args.output)
    print('#include <memory>', file=args.output)
    print('#include <string>', file=args.output)
    print('#include <typeindex>', file=args.output)
    print('#include <vector>', file=args.output)
    print('', file=args.output)

    types = doc['types']
    exports = doc['exports']

    banner(f"Module - {doc['module']['name']}")
    for annotation in doc['module']['annotations']:
        annotation_args = annotation['args']
        print(f'// module annotation: {annotation["type"]}({",".join([k+":"+encode(v) for k,v in annotation_args.items()])})', file=args.output)
    print('', file=args.output)
    
    banner('Imports')
    for module in doc['module']['imports']:
        print(f'#include "{module}.h"', file=args.output)
    print('', file=args.output)

    banner('Types')

    state = {'current_ns': None}

    def enter_namespace(ns):
        if state['current_ns'] != ns:
            if state['current_ns'] is not None:
                print(f'}} // namespace {ns}\n\n', file=args.output)
            state['current_ns'] = ns
            if state['current_ns'] is not None:
                print(f'namespace {ns} {{', file=args.output)

    def print_annotations(prefix, annotations):
        for annotation in annotations:
            print(f'{prefix}// annotation: {annotation["type"]}({",".join([k+":"+encode(v) for k,v in annotation["args"].items()])})', file=args.output)

    def type_banner(type):
        if 'location' in type:
            loc = type['location']
            if 'line' in loc and 'column' in loc:
                print(f'  // {loc["filename"]}({loc["line"]},{loc["column"]})', file=args.output)
            elif 'line' in loc:
                print(f'  // {loc["filename"]}({loc["line"]})', file=args.output)
            else:
                print(f'  // {loc["filename"]}', file=args.output)

        print_annotations('  ', type['annotations'])

    def annotation_getter(type):
        print('    template <int N>', file=args.output)
        print('    static decltype(auto) get_annotation() {', file=args.output)
        for index,anno in enumerate(type['annotations']):
            anno_type = types[anno['type']]
            if anno_type['name'] == '$sapc.customtag': continue

            print(f'      if constexpr(N == {index}) {{', file=args.output)
            print(f'        static auto const anno = {qualified(anno_type)}{{', file=args.output)
            for key,value in anno['args'].items():
                arg_type = anno_type['fields'][key]
                print(f'          {encode(value)}, // {arg_type["name"]}', file=args.output)
            print('          };', file=args.output)
            print('          return anno;', file=args.output)
            print('      }', file=args.output)
        print('    }', file=args.output)

    for typename in exports['types']:
        type = types[typename]
        if ignored(type): continue

        type_ns = namespace(type)
        name = cxxname(type)
        kind = type['kind']

        basetype = types[type['base']] if 'base' in type else None
        basespec = f' : {qualified(basetype)}' if basetype is not None else ''

        if kind == 'enum':
            enter_namespace(namespace(type))
            type_banner(type)

            print(f'  enum class {name}{basespec} {{', file=args.output)

            for ename in type['names']:
                value = type['values'][ename]
                print(f'    {identifier(ename)} = {value},', file=args.output)

            print(f'  }};\n', file=args.output)
        elif kind == 'alias':
            if 'refType' in type:
                enter_namespace(namespace(type))
                type_banner(type)

                print(f'  using {name} = {field_cxxtype(types, type["refType"])};\n', file=args.output)
        elif kind == 'union':
            enter_namespace(namespace(type))
            type_banner(type)

            print(f'  union {name}{basespec} {{', file=args.output)

            if 'order' in type:
                for fieldname in type['order']:
                    field = type['fields'][fieldname]
                    if ignored(field): continue

                    print_annotations('    ', field['annotations'])

            print(f'  }};\n', file=args.output)
        else:
            enter_namespace(namespace(type))
            type_banner(type)

            if 'generics' in type:
                print(f'  template <typename {", typename ".join(type["generics"])}>', file=args.output)

            print(f'  struct {name}{basespec} {{', file=args.output)

            annotation_getter(type)

            if 'order' in type:
                for fieldname in type['order']:
                    field = type['fields'][fieldname]
                    if ignored(field): continue

                    print_annotations('    ', field['annotations'])

                    if 'default' in field:
                        print(f'    {field_cxxtype(types, field["type"])} {cxxname(field)} = {encode(field["default"])};', file=args.output)
                    else:
                        print(f'    {field_cxxtype(types, field["type"])} {cxxname(field)};', file=args.output)

            print(f'  }};\n', file=args.output)
    
    enter_namespace(None)

    banner('Constants')
        
    for constname in exports['constants']:
        constant = doc['constants'][constname]
        if ignored(constant): continue;

        enter_namespace(namespace(constant))

        name = cxxname(constant)

        if 'location' in constant:
            loc = constant['location']
            if 'line' in loc and 'column' in loc:
                print(f'  // {loc["filename"]}({loc["line"]},{loc["column"]})', file=args.output)
            elif 'line' in loc:
                print(f'  // {loc["filename"]}({loc["line"]})', file=args.output)
            else:
                print(f'  // {loc["filename"]}', file=args.output)

        print(f'  static const {field_cxxtype(types, constant["type"])} {cxxname(constant)} = {encode(constant["value"])};\n', file=args.output)

    enter_namespace(None)

    print('#endif', file=args.output)

    args.output.close()

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
