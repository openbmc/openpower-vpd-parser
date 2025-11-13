# Overview

As future systems undergo development, hardware may arrive gradually over time. This includes FRUs whose VPD needs to be collected. This may also possibly include System VPD. In order to de-couple this dependency from development and validation, File Mode VPD feature has been introduced. This feature is intended to be used in lab only.

## Enabling File Mode VPD

File Mode VPD depends on the following two u-boot variables:

### fieldmode

For File Mode VPD, `fieldmode` has to be set to false.
`fieldmode` variable value can be checked from the BMC CLI:

    root@bmc:~# /sbin/fw_printenv fieldmode

fieldmode can be disabled from BMC CLI:

    root@bmc:~# /sbin/fw_setenv fieldmode=false

### vpdmode
The u-boot variable `vpdmode` controls the way in which VPD is collected.
`vpdmode` variable value can be checked from the BMC CLI:

    root@bmc:~# /sbin/fw_printenv vpdmode
    
`vpdmode` can be set from the BMC CLI:
    
    root@bmc:~# /sbin/fw_setenv vpdmode=hardware


#### Modes
##### Hardware Mode
In this mode, System and FRU VPD is collected from the EEPROM paths mentioned in the system configuration JSON. This is the default mode.

    root@bmc:~# /sbin/fw_setenv vpdmode=hardware

##### Mixed Mode
In this mode, for each FRU in the system configuration JSON, vpd-manager checks if a file is present in the file system. If it is present, then all VPD reads and writes would happen to the file rather than to hardware. If the file is absent, all VPD reads and writes would happen on the hardware. The file path is determined by prepending `/var/lib/vpd/filemode` to the respective EEPROM path entry in the system configuration JSON.

Example:
For the FRU with EEPROM path `/sys/bus/i2c/drivers/at24/8-0050/eeprom`, vpd-manager will look for the file `/var/lib/vpd/filemode/sys/bus/i2c/drivers/at24/8-0050/eeprom`

    root@bmc:~# /sbin/fw_setenv vpdmode=mixed

##### File Mode
In this mode, for each FRU in the system configuration JSON, vpd-manager checks if a file is present in the file system. If it is present, then all VPD reads and writes would happen to the file rather than to hardware. If the file is not found, the FRU is not processed, and no reads and writes are allowed for this FRU. The file path is determined by prepending `/var/lib/vpd/filemode` to the respective EEPROM path entry in the system configuration JSON.

Example:
For the FRU with EEPROM path `/sys/bus/i2c/drivers/at24/8-0050/eeprom`, vpd-manager will look for the file `/var/lib/vpd/filemode/sys/bus/i2c/drivers/at24/8-0050/eeprom`

    root@bmc:~# /sbin/fw_setenv vpdmode=file

## FRU EEPROM file setup

In order to setup vpd-manager to read a FRU VPD from file, the VPD image (the binary file) needs to be copied to the BMC filesystem.

Example:
For the FRU with EEPROM path `/sys/bus/i2c/drivers/at24/9-0050/eeprom`, the VPD image needs to be copied to `/var/lib/vpd/filemode/sys/bus/i2c/drivers/at24/9-0050/eeprom`


