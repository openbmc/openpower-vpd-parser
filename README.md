# Overview

This repository hosts code for parsing and management of VPD. Currently it has
support for OpenPower, IPZ and keyword VPD formats. OpenPower and IPZ are
structured binaries that consist of records and keywords. A record is a
collection of multiple keywords.

The support for other VPD formats can be enabled by extending the interface
implementation and adding custom logic with respect to the structure of that
VPD.

The respository provides support for multiple modes to manage VPD.

1. JSON based processing of EEPROM(s).
   - In this mode, VPD will be processed based on JSON provided and will be
     published under phosphor-inventory-manager service.
2. Single file mode.
   - In this mode, a file can be passed to the exe which parse the data based on
     its format and returns the parsed VPD back. In this mode, further handling
     of the parsed data needs to be added.
3. Dbus mode.
   - The service hosted by this repository will expose API over Dbus, which can
     be called by passing user space file path of VPD EEPROM. The API returns
     parsed VPD for supported formats.

The repository consists of two distinct applications, which are:

## VPD Manager

## VPD-parser-app

## TODOs and Future Improvements
