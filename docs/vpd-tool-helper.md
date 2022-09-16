## This page details usage of the vpd-tool.

`vpd-tool` is a command line interface to read and write FRU's Vital Product Data (VPD).

**Where is the FRU VPD present?**

* The FRU's VPD is embedded in the EEPROM hardware. The EEPROM path of any FRU can be found from the VPD JSON present in /var/lib/vpd/vpd_inventory.json.
* The FRU VPD is populated on D-Bus by the ibm-read-vpd application.

# Operations allowed by the vpd-tool:
1. Dump critical data of all inventory objects
2. Dump critical data of a specific object
3. Read a VPD keyword from D-Bus
4. Read a VPD keyword from hardware
5. Write a VPD keyword on D-Bus(implicit write on hardware also)
6. Write a VPD keyword only on hardware(currently it writes on D-Bus)
7. Force recollect all VPD

**1. Dump critical data of all inventory objects**

vpd-tool provides an option to dump critical data of **all inventory objects**.

Command:
```
vpd-tool -i  (or)
vpd-tool --dumpInventory
```

_The critical data includes CC, SN, FN, PN, DR, Location code, Pretty name, Present property, Type of the FRU and any other FRU specific properties (like Address, Bus number, Slot number for PCIe cable cards; Model, Spare Part number for fan FRU, etc.,)._

**2. Dump critical data of a specific object**

The vpd-tool provides an option to dump critical data **for a specific object** provided the object path.
The user can find the object path by executing `vpd-tool -i` command.

Command:
```
vpd-tool -o -O <object_path> (or)

vpd-tool --dumpObject --object <object_path>

Eg: vpd-tool --dumpObject --object /system/chassis/motherboard
```

**3. Read a VPD keyword from D-Bus**

The vpd-tool provides an option to **read** a particular VPD keyword **from D-Bus**, provided the object path, record name and keyword name.

Command:
```
vpd-tool -r -O <object_path> -R <record_name> -K <keyword_name>  (or)

vpd-tool --readKeyword --object <object_path> --record <record_name> --keyword <keyword_name>

Eg: vpd-tool --readKeyword --object /system/chassis/motherboard --record VINI --keyword SN
```

**4. Read a VPD keyword from hardware**

The vpd-tool provides an option to **read** a particular VPD keyword directly **from hardware** **(-H or --Hardware)**, provided the **EEPROM** **path**, record name and keyword name.

Command:

```
vpd-tool -r -H -O <EEPROM_path> -R <record_name> -K <keyword_name>. (or)

vpd-tool --readKeyword --Hardware --object <EEPROM_path> --record <record_name> --keyword <keyword_name>

Eg: vpd-tool --readKeyword --Hardware --object /sys/bus/i2c/drivers/at24/7-0051/eeprom --record VINI --keyword SN
```

**5. Write a VPD keyword on D-Bus (implicitly writes on hardware)**

The vpd-tool provides an option to **write** a particular VPD keyword **on D-Bus** and implicitly the value will be written on hardware as well. _There is no option to write keyword only on D-Bus, as it creates data mismatch._

Input arguments needed are **object path**, record name, keyword and value to be written.

`Note: Value should be given either in ASCII (or) in HEX prefixed with 0x. Value is accepted as 2 digit hex numbers only.(0x1 should be given as 0x01).`

Command:
```
vpd-tool -w -O <object_path> -R <record_name> -K <keyword_name> -V <value>  (or)

vpd-tool -u  -O <object_path> -R <record_name> -K <keyword_name> -V <value>  (or)

vpd-tool --writeKeyword --object <object_path> --record <record_name> --keyword <keyword_name> --value <value in ASCII/HEX>  (or)

vpd-tool --updateKeyword --object <object_path> --record <record_name> --keyword <keyword_name> --value <value in ASCII/HEX>

Eg: vpd-tool --writeKeyword --object /system/chassis/motherboard/tpm_wilson --record VINI --keyword SN --value 0x626364
    vpd-tool -w -O /system/chassis/motherboard/tpm_wilson -R VINI -K SN -V abcABC
```

**6. Write a VPD keyword on hardware**

The vpd-tool provides an option to **write** a particular VPD keyword directly on hardware.

Input arguments needed are **EEPROM path**, record name, keyword and value to be written.

`Note: Value should be given either in ASCII (or) in HEX prefixed with 0x.`

Command:
```
vpd-tool -w -H -O <EEPROM_path> -R <record_name> -K <keyword_name> -V <value>  (or)

vpd-tool -u -H -O <EEPROM_path> -R <record_name> -K <keyword_name> -V <value>  (or)

vpd-tool --writeKeyword -H --object <EEPROM_path> --record <record_name> --keyword <keyword_name> --value <value in ASCII/HEX>  (or)

vpd-tool --updateKeyword -H --object <EEPROM_path> --record <record_name> --keyword <keyword_name> --value <value in ASCII/HEX>

Eg: vpd-tool --writeKeyword -H --object /sys/bus/i2c/drivers/at24/0-0051/eeprom --record VINI --keyword SN --value 0x626364
    vpd-tool -w -H -O /sys/bus/i2c/drivers/at24/0-0051/eeprom -R VINI -K SN -V abcABC
```

**7. Force recollect all VPD on Hardware ( -f / -F / --forceReset )**

_**WARNING: This option is not meant to be used outside the LAB.**_

The vpd-tool has an option to do force recollection on all the VPD present on hardware. When this option is executed, the persisted VPD cache data is deleted and the pim service and system vpd service are restarted and retrigger all the udev events.

_**This option cannot be executed if the chassis power is ON or transitioning to ON,**_ as restarting the system vpd service will fail if the CEC is powered ON and thereby brings the system to failed state.

Command:
```
vpd-tool -f (or)
vpd-tool --forceReset (or)
vpd-tool -F
```
