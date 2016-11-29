#!/usr/bin/env python

import yaml
from mako.template import Template

if __name__ == '__main__':
    with open('writefru.yaml', 'r') as fd:
        yamlDict = yaml.safe_load(fd)

        # Render the mako template
        template = 'writefru.mako.hpp'
        t = Template(filename=template)
        with open('writefru.hpp', 'w') as fd:
            fd.write(
                t.render(
                    fruDict=yamlDict))
