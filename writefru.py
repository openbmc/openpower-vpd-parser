#!/usr/bin/env python

import os
import yaml
from mako.template import Template
import argparse

def main():
    parser = argparse.ArgumentParser(
        description="OpenPOWER FRU VPD parser and code generator")

    parser.add_argument(
        '-i', '--inventory_yaml', dest='inventory_yaml',
        default='writefru.yaml', help='input inventory yaml file to parse')
    args = parser.parse_args()

    with open(os.path.join(script_dir, args.inventory_yaml), 'r') as fd:
        yamlDict = yaml.safe_load(fd)

        # Render the mako template
        template = os.path.join(script_dir, 'writefru.mako.hpp')
        t = Template(filename=template)
        with open('writefru.hpp', 'w') as fd:
            fd.write(
                t.render(
                    fruDict=yamlDict))


if __name__ == '__main__':
    script_dir = os.path.dirname(os.path.realpath(__file__))
    main()
