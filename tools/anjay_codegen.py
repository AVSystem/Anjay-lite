#!/usr/bin/env python3
# Copyright 2023-2025 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay Lite LwM2M SDK
# All rights reserved.
#
# Licensed under AVSystem Anjay Lite LwM2M Client SDK - Non-Commercial License.
# See the attached LICENSE file for details.

import datetime
import argparse
import collections
import textwrap
import operator
import sys
import os
import re
from xml.etree import ElementTree
from xml.etree.ElementTree import Element
from jinja2 import Environment, FileSystemLoader

C_SINGLE_INST_TEMPLATE_FILENAME = "single_instance_template.c.jinja2"
C_MULTIPLE_INSTS_TEMPLATE_FILENAME = "multiple_instances_template.c.jinja2"

NONALPHANUM_REGEX = re.compile(r'[^a-zA-Z0-9]+')

DIGIT_SPELLINGS = {
    '0': 'zero',
    '1': 'one',
    '2': 'two',
    '3': 'three',
    '4': 'four',
    '5': 'five',
    '6': 'six',
    '7': 'seven',
    '8': 'eight',
    '9': 'nine'
}


def _node_text(n: Element) -> str:
    return (n.text if (n is not None and n.text is not None) else '').strip()


def _sanitize_identifier(n: str) -> str:
    identifier = NONALPHANUM_REGEX.sub('_', n).strip('_')
    # Identifiers are not allowed to have a leading digit
    return DIGIT_SPELLINGS.get(identifier[0], identifier[0]) + identifier[1:]


class ResourceDef(collections.namedtuple('ResourceDef', ['rid', 'name', 'operations', 'multiple', 'mandatory', 'type',
                                                         'range_enumeration', 'units', 'description'])):
    @property
    def mandatory_str(self) -> str:
        return 'Mandatory' if self.mandatory else 'Optional'

    @property
    def multiple_str(self):
        return 'Multiple' if self.multiple else 'Single'

    @property
    def name_upper(self) -> str:
        return _sanitize_identifier(self.name.upper())
    
    @property
    def name_snake(self) -> str:
        return _sanitize_identifier(self.name).lower()

    @property
    def kind_enum(self) -> str:
        if self.operations not in {'R', 'W', 'RW', 'E', 'BS_RW'}:
            raise AssertionError('unexpected operations: ' + self.operations)
        result = 'ANJ_DM_RES_' + self.operations
        if self.multiple:
            if 'E' in self.operations:
                raise AssertionError('multiple-instance executable resources are not supported')
            result += 'M'
        return result
    
    @property
    def type_enum(self) -> str:
        types = {
            'string': 'ANJ_DATA_TYPE_STRING',
            'integer': 'ANJ_DATA_TYPE_INT',
            'float': 'ANJ_DATA_TYPE_DOUBLE',
            'boolean': 'ANJ_DATA_TYPE_BOOL',
            'opaque': 'ANJ_DATA_TYPE_BYTES',
            'time': 'ANJ_DATA_TYPE_TIME',
            'objlnk': 'ANJ_DATA_TYPE_OBJLNK',
            'unsigned integer': 'ANJ_DATA_TYPE_UINT',
            'corelnk': 'ANJ_DATA_TYPE_STRING',
        }

        if self.type.lower() not in types and "E" not in self.operations:
            raise AssertionError(f'unknown type: {self.type}')

        return types.get(self.type.lower(), 'ANJ_DATA_TYPE_NULL')

    @property
    def union_res_value_field(self) -> str:
        types = {
            'string': 'bytes_or_string.data',
            'integer': 'int_value',
            'float': 'double_value',
            'boolean': 'bool_value',
            'opaque': 'bytes_or_string.data',
            'time': 'time_value',
            'objlnk': 'objlnk',
            'unsigned integer': 'uint_value',
            'corelnk': 'bytes_or_string.data',
        }

        return types.get(self.type.lower(), 'Error: unknown type')

    @classmethod
    def from_etree(cls, res: Element) -> 'ResourceDef':
        return cls(rid=int(res.get('ID')),
                   name=_node_text(res.find('Name')),
                   operations=_node_text(res.find('Operations')).upper()
                       or 'RW', # no operations = resource modifiable by Bootstrap Server
                   multiple={'Single': False, 'Multiple': True}[_node_text(res.find('MultipleInstances'))],
                   mandatory={'Optional': False, 'Mandatory': True}[_node_text(res.find('Mandatory'))],
                   type=(_node_text(res.find('Type')).lower() or 'N/A'),
                   range_enumeration=(_node_text(res.find('RangeEnumeration')) or 'N/A'),
                   units=(_node_text(res.find('Units')) or 'N/A'),
                   description=textwrap.fill(_node_text(res.find('Description'))).replace('\n', '\n\t * '))


class ObjectDef(collections.namedtuple('ObjectDef',
                                       ['oid', 'name', 'version', 'description', 'urn', 'multiple', 'mandatory', 'resources'])):
    @property
    def name_snake(self) -> str:
        return _sanitize_identifier(self.name).lower()

    @property
    def name_pascal(self) -> str:
        return ''.join(word.capitalize() for word in _sanitize_identifier(self.name).split('_'))
    
    @property
    def name_upper(self) -> str:
        return _sanitize_identifier(self.name_snake.upper())

    @property
    def mandatory_str(self) -> str:
        return 'Mandatory' if self.mandatory else 'Optional'

    @property
    def multiple_str(self):
        return 'Multiple' if self.multiple else 'Single'

    @property
    def has_any_readable_resources(self) -> bool:
        return any('R' in res.operations for res in self.resources)

    @property
    def has_any_writable_resources(self) -> bool:
        return any('W' in res.operations for res in self.resources)

    @property
    def has_any_executable_resources(self) -> bool:
        return any('E' in res.operations for res in self.resources)

    @property
    def has_any_multiple_resources(self) -> bool:
        return any(res.multiple for res in self.resources)

    @property
    def has_any_multiple_writable_resources(self) -> bool:
        return any((res.multiple and 'W' in res.operations) for res in self.resources)

    @property
    def needs_instance_reset_handler(self) -> bool:
        return self.multiple or self.has_any_writable_resources

    @staticmethod
    def parse_resources(obj: ElementTree):
        return sorted([ResourceDef.from_etree(item) for item in obj.find('Resources').findall('Item')],
                       key=operator.attrgetter('rid'))

    @classmethod
    def from_etree(cls, obj: ElementTree, resources_subset: set) -> 'ObjectDef':
        resources = ObjectDef.parse_resources(obj)
        if resources_subset is not None:
            resources = [ r for r in resources if r.rid in resources_subset ]

        return cls(oid=int(_node_text(obj.find('ObjectID'))),
                   name=_node_text(obj.find('Name')),
                   version=_node_text(obj.find('ObjectVersion')),
                   description=textwrap.fill(_node_text(obj.find('Description1'))).replace('\n', '\n * '),
                   urn=_node_text(obj.find('ObjectURN')),
                   multiple={'Single': False, 'Multiple': True}[_node_text(obj.find('MultipleInstances'))],
                   mandatory={'Optional': False, 'Mandatory': True}[_node_text(obj.find('Mandatory'))],
                   resources=resources)


def generate_object_boilerplate(
        obj: ObjectDef, 
        instances_number: int, 
        transactional: bool = False, 
        resource_instances: list[list[int]] = None,
        dynamic_resources: bool = False,
        dynamic_instances: bool = False
        ) -> str:

    template_loader = FileSystemLoader(searchpath=os.path.join(os.path.dirname(__file__), './templates'))
    jinja_env = Environment(loader=template_loader, trim_blocks=True)

    resource_instances_dict = {}
    for rid, count in resource_instances or []:
        rid = int(rid)
        count = int(count)
        if rid not in resource_instances_dict:
            resource_instances_dict[rid] = count
        else:
            raise ValueError(f'Resource ID {rid} specified multiple times with different instance counts.')
    
    for resource in obj.resources:
        if resource.multiple and resource.rid not in resource_instances_dict:
            resource_instances_dict[resource.rid] = 2

    template_args = dict(
        obj=obj,
        transactional=transactional,
        resource_instances=resource_instances_dict,
        dynamic_resources_instances=dynamic_resources,
        instances_number=instances_number,
        dynamic_object_instances=dynamic_instances,
        date_time=datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
    )

    if obj.multiple == False or (instances_number and instances_number == 1):
        return jinja_env.get_template(C_SINGLE_INST_TEMPLATE_FILENAME).render(**template_args)
    elif instances_number and instances_number > 1:
        return jinja_env.get_template(C_MULTIPLE_INSTS_TEMPLATE_FILENAME).render(**template_args)

if __name__ == '__main__':
    example_usage_text = '''
    Example usage:
        ./tools/anjay_codegen.py -i some_obj.xml -o some_obj.c 
        ./tools/anjay_codegen.py -i some_obj.xml -o some_obj.c -r 1 2 3 -ni 5
        ./tools/anjay_codegen.py -i some_obj.xml -o some_obj.c -nri 4 3
    '''
    parser = argparse.ArgumentParser(
        description='Parses an LwM2M object definition XML and generates Anjay Lite object skeleton', 
        epilog=example_usage_text, 
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    parser.add_argument('-i', '--input', help='Input filename or - to read from stdin')
    parser.add_argument('-o', '--output', default='/dev/stdout', help='Output filename (default: stdout)')
    parser.add_argument('-l', '--list', help='List resources and their names only', action='store_true')
    parser.add_argument('-r', '--resources', nargs='+', type=int, help='Generate code only for a specific list of resources. If the resource does not exist it is silently ignored.')
    parser.add_argument('-t', '--transactional', action='store_true', help='Generate empty transaction handlers.')
    parser.add_argument('-ni', '--instances-number', metavar='{1,2,...,65534}', dest='instances_number', type=int,
                        help='Number of object instances to generate. If not specified, 2 instances will be generated for multiple instance objects.' \
                        'If used together with -di, it will mean maximum number of object instances.', default=1)
    parser.add_argument('-di', '--dynamic-instances', action='store_true', help='Handle multiple instance objects dynamically') 
    parser.add_argument('-nri', '--resources-instances-number', type=int, metavar='{1,2,...,65534}', 
                        help='Number of resource instances for multiple instance resources per resource. ' \
                        'If used together with -dri, it will mean maximum number of resource instances.' \
                        'List arguments in form -nri rid1 count1 -nri rid2 count2 where rid is resource ID and count is number of resource instances. ' \
                        'If omitted for resource, it will be defaulted to 2 instances. ',
                        action='append', nargs=2)
    parser.add_argument('-dri', '--dynamic-resources-instances', action='store_true', help='Handle multiple instance resources dynamically')

    if len(sys.argv) == 1:
        parser.print_help()
        sys.exit(1)

    args = parser.parse_args()
    if args.input is None or (args.instances_number is not None and args.instances_number not in range(1, 65535)):
        parser.print_usage()
        sys.exit(1)

    input_source = None
    if args.input == '-':
        input_source = sys.stdin.read()
    else:
        with open(args.input) as f:
            input_source = f.read()

    tree = ElementTree.fromstring(input_source)
    obj = ObjectDef.from_etree(tree.find('Object'), args.resources)

    if args.list:
        for r in obj.resources:
            print(r.rid, r.name, '(mandatory)' if r.mandatory else '')
        sys.exit(0)

    if args.instances_number > 1 and not obj.multiple:
        print(f'NOTE: Object {obj.name} is not multiple instance, but instances number ({args.instances_number}) specified. '
              'Generating code for single instance object instead.')
        args.instances_number = 1

    boilerplate = generate_object_boilerplate(
        obj,
        args.instances_number, 
        args.transactional, 
        args.resources_instances_number, 
        args.dynamic_resources_instances,
        args.dynamic_instances
    )

    if args.output == '-':
        print(boilerplate)
    else:
        with open(args.output, 'w') as f:
            print(boilerplate, file=f)
