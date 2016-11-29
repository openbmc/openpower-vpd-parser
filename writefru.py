#!/usr/bin/env python
import yaml

if __name__ == '__main__':
    with open('writefru.yaml', 'r') as f:
        ifile = yaml.safe_load(f)

    with open('writefru.hpp', 'w') as ofile:
        ofile.write('/* WARNING: Generated header. Do not edit!!! */\n\n')

        ofile.write('#pragma once\n\n')

        ofile.write('#include <map>\n')
        ofile.write('#include <iostream>\n')
        ofile.write('#include <defines.hpp>\n')
        ofile.write('#include <store.hpp>\n\n')

        ofile.write('namespace openpower\n{\n')
        ofile.write('namespace vpd\n{\n')
        ofile.write('namespace inventory\n{\n\n')

        ofile.write('using inner = std::map<std::string, std::string>;\n')
        ofile.write('using outer = std::map<std::string, inner>;\n\n')

        ofile.write('auto print = [](outer&& object, const std::string& path)')
        ofile.write('\n')
        ofile.write('{\n')
        ofile.write('    std::cout << std::endl;\n')
        ofile.write('    std::cout << path << std::endl;\n')
        ofile.write('    std::cout << std::endl;\n')
        ofile.write('    for(const auto& o : object)\n')
        ofile.write('    {\n')
        ofile.write('        std::cout << o.first << std::endl;\n')
        ofile.write('        for(const auto& i : o.second)\n')
        ofile.write('        {\n')
        ofile.write('            ')
        ofile.write('std::cout << i.first << " : " << i.second <<std::endl;\n')
        ofile.write('        }\n')
        ofile.write('        std::cout << std::endl;\n')
        ofile.write('    }\n')
        ofile.write('};\n\n')

        ofile.write('/** @brief API to write parsed VPD to inventory, ')
        ofile.write('for a specifc FRU\n')
        ofile.write(' *\n')
        ofile.write(' *  @param [in] vpdStore - Store object containing ')
        ofile.write('parsed VPD\n')
        ofile.write(' *  @param [in] path - FRU object path\n')
        ofile.write(' */\n')
        ofile.write('template<Fru F>\n')
        ofile.write('void writeFru(const Store& vpdStore,\n')
        ofile.write('    const std::string& path);\n\n')

        for frus in ifile.iterkeys():
            # Value of this group is a std::set<string, led structure>
            fru = ifile[frus]
            ofile.write('/** @brief Specialization of ' + frus + ' */\n')
            ofile.write('template<>\n')
            ofile.write('void writeFru<Fru::')
            ofile.write(frus)
            ofile.write('>(const Store& vpdStore,\n')
            ofile.write('    const std::string& path)\n')
            ofile.write('{\n')
            ofile.write('    outer object;\n\n')

            ofile.write('    // Inventory manager needs object path, list ')
            ofile.write('of interface names to be\n')
            ofile.write('    // implemented, and property:value pairs ')
            ofile.write('contained in said interfaces.\n')

            for interfaces, properties in fru.iteritems():
                interface = interfaces.split(".")
                intfName = interface[0] + interface[-1]
                ofile.write('    inner ' + intfName + ';\n')
                for name, value in properties.iteritems():
                    if fru and interfaces and name and value:
                        record, keyword = value.split(",")
                        ofile.write('    ' + intfName + '["' + name + '"] =\n')
                        ofile.write('    vpdStore.get<Record::')
                        ofile.write(record)
                        ofile.write(', record::Keyword::' + keyword + '>();\n')
                ofile.write('    object.emplace(\"' + interfaces + '\",\n')
                ofile.write('        ' + intfName + ');\n\n')
            ofile.write('    // TODO: Need integration with inventory ')
            ofile.write('manager, print serialized dbus\n')
            ofile.write('    // object for now.\n')
            ofile.write('    print(std::move(object), path);\n')
            ofile.write('}\n\n')

        ofile.write('} // namespace inventory\n')
        ofile.write('} // namespace vpd\n')
        ofile.write('} // namespace openpower\n')
