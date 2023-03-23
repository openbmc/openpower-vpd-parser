# Overview

This repository hosts code for OpenPower and IBM IPZ format VPD parsers. Both
OpenPower VPD and IPZ VPD formats are structured binaries that consist of
records and keywords. A record is a collection of multiple keywords. More
information about the format can be found
[here](https://www-355.ibm.com/systems/power/openpower/posting.xhtml?postingId=1D060729AC96891885257E1B0053BC95).

The repository consists of two distinct applications, which are:

## OpenPower VPD Parser

This is a build-time YAML driven application that parses the OpenPower VPD
format and uses the YAML configuration (see extra-properties-example.yaml and
writefru.yaml) to determine:

- The supported records and keywords.
- How VPD data is translated into D-Bus interfaces and properties.

The application instance must be passed in the file path to the VPD (this can,
for example, be a sysfs path exposed by the EEPROM device driver) and also the
D-Bus object path(s) that EEPROM data needs to be published under.

## IBM VPD Parser

This parser is can be built by passing in the `--enable-ibm-parser` configure
option. This parser differs from the OpenPower VPD parser in the following ways:

- It parses all the records and keywords from the VPD, including large keywords
  (Keywords that begin with a `#` and are > 255 bytes in length).
- It relies on a runtime JSON configuration (see examples/inventory.json) to
  determine the D-Bus object path(s) that hold interfaces and properties
  representing the VPD for a given VPD file path.

Making the application runtime JSON driven allows us to support multiple systems
(with different FRU configurations) to be supported in a single code image as
well as making the application more flexible for future improvements.

## TODOs and Future Improvements

1.  The long-term goal is to completely do away with the build time YAML driven
    configurations and instead reconcile the OpenPower VPD parser and the IBM
    VPD parser applications into a single runtime JSON driven application.
2.  Add details to the README on how to configure and build the application.
3.  More JSON documentation.
4.  Support for more IBM VPD formats.
5.  VPD Write and tool documentation.


