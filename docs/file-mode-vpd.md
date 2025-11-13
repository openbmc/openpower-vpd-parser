# Overview

As future systems undergo development, hardware may arrive gradually over time.
System VPD on such systems may not be available on hardware. In order to
de-couple this dependency from development and validation, File Mode VPD feature
has been introduced. This feature is intended to be used in **lab only**, and
only applies to **System VPD**.

## Enabling File Mode VPD

File Mode VPD depends on the following two u-boot variables:

### 1. fieldmode

The u-boot variable `fieldmode` value decides whether a system is in field mode
or in lab mode. For File Mode VPD, `fieldmode` has to be set to false.

`fieldmode` variable value can be checked from the BMC CLI:

    root@bmc:~# /sbin/fw_printenv fieldmode

`fieldmode` can be disabled from BMC CLI:

    root@bmc:~# /sbin/fw_setenv fieldmode false

### 2. vpdmode

The u-boot variable `vpdmode` controls the way in which VPD is collected.

`vpdmode` variable value can be checked from the BMC CLI:

    root@bmc:~# /sbin/fw_printenv vpdmode

`vpdmode` can be set from the BMC CLI:

    root@bmc:~# /sbin/fw_setenv vpdmode hardware

#### Modes of `vpdmode`

##### 1. Hardware Mode

In this mode, System VPD is collected from the EEPROM path mentioned in the
system configuration JSON. This is the default mode.

    root@bmc:~# /sbin/fw_setenv vpdmode hardware

##### 2. Mixed Mode

In this mode, vpd-manager checks if a file containing System VPD is present in
the file system. If it is present, then all System VPD is collected from the
file, and all reads and writes would happen to the file rather than to hardware.
If the file is absent, all System VPD reads and writes would happen on the
hardware. The file path is determined by prepending `/var/lib/vpd/filemode` to
the System VPD EEPROM path entry in the system configuration JSON.

Example: If the system configuration JSON specifies System VPD EEPROM path as
`/sys/bus/i2c/drivers/at24/8-0050/eeprom`, vpd-manager will look for the file
`/var/lib/vpd/filemode/sys/bus/i2c/drivers/at24/8-0050/eeprom`

    root@bmc:~# /sbin/fw_setenv vpdmode mixed

##### 3. File Mode

In this mode, vpd-manager checks if a file containing System VPD is present in
the file system. If it is present, then all VPD reads and writes would happen to
the file rather than to hardware. If the file is not found, `vpd-manager` treats
this scenario as System VPD parsing failure. The file path is determined by
prepending `/var/lib/vpd/filemode` to the System VPD EEPROM path entry in the
system configuration JSON.

Example: If the system configuration JSON specifies System VPD EEPROM path as
`/sys/bus/i2c/drivers/at24/8-0050/eeprom`, vpd-manager will look for the file
`/var/lib/vpd/filemode/sys/bus/i2c/drivers/at24/8-0050/eeprom`

    root@bmc:~# /sbin/fw_setenv vpdmode file

## System VPD EEPROM file setup

In order to setup `vpd-manager` to read a System VPD from file, the VPD image
(the binary file) needs to be copied to specific path in the BMC filesystem.

In order to determine the System VPD EEPROM path for a given system, one needs
to refer to the respective system workbook or the system configuration JSON.

Example: If System VPD is on EEPROM path
`/sys/bus/i2c/drivers/at24/1-0051/eeprom`, the VPD image needs to be copied to
`/var/lib/vpd/filemode/sys/bus/i2c/drivers/at24/1-0051/eeprom`

## Steps to enable File Mode VPD

1. Set `fieldmode` u-boot variable to `false`. See [fieldmode](#1-fieldmode)
2. Set `vpdmode` u-boot variable to desired value. See [vpdmode](#2-vpdmode)
3. Setup VPD image of FRU(s) in the BMC filesystem as described in
   [System VPD EEPROM file setup](#system-vpd-eeprom-file-setup)
4. Reboot BMC
5. After BMC boots up, `vpd-manager` will operate in the desired mode.

## Notes

1. All `vpd-manager` D-Bus APIs will continue to work as usual in File Mode VPD.
2. All `vpd-tool` options will work as usual in File Mode VPD.
