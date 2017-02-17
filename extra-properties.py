#!/usr/bin/env python

import os
import yaml
from mako.template import Template
import argparse


def main():
    parser = argparse.ArgumentParser(
        description="OpenPOWER FRU VPD parser and code generator")

    parser.add_argument(
        '-e', '--extra_props_yaml',
        dest='extra_props_yaml',
        default='extra-properties-example.yaml',
        help='input extra properties yaml file to parse')
    args = parser.parse_args()

    with open(os.path.join(script_dir, args.extra_props_yaml), 'r') as fd:
        yamlDict = yaml.safe_load(fd)

        # Render the mako template
        template = os.path.join(script_dir, 'extra-properties.mako.hpp')
        t = Template(filename=template)
        with open('extra-properties-gen.hpp', 'w') as fd:
            fd.write(
                t.render(
                    dict=yamlDict))


if __name__ == '__main__':
    script_dir = os.path.dirname(os.path.realpath(__file__))
    main()
