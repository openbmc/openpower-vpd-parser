# Backup and Restore Config JSON File Documentation

This document describes the structure and usage of the JSON configuration file
used for backing up and restoring VPD (Vital Product Data). It explains required
fields, value formats, and provides guidance for extending VPD handling for
system use.

---

## source

Specifies the location of the primary VPD. This section is mandatory for both
backup and restore operations.

It must contain either:

- **hardwarePath** — Physical EEPROM path, or

- **inventoryPath** — D-Bus inventory object path.

## destination

Specifies where the VPD backup is stored. This section is mandatory for both
backup and restore operations..

It must contain either:

- **hardwarePath** - The backup is stored on another FRU’s EEPROM.

- **inventoryPath** - The backup is stored in the BMC cache.

## type

Specifies the VPD format (e.g., IPZ or keyword-based VPD).

> **Note:** Currently only IPZ format is supported for backup/restore.

## backupMap

Defines the list of VPD records and keywords that must be backed up or restored.
Each entry describes the source and destination mapping.

### sourceRecord

Record name in the source VPD.

### sourceKeyword

Keyword name in the source record.

### destinationRecord

Record name in the destination VPD.

### destinationKeyword

Keyword name in the destination record.

### defaultValue

Specifies the value that should be used when comparing the source and
destination keyword values. This value is used only for comparison, not for
updating VPD.

If both the source and destination VPD keywords match this defaultValue, and
isPelRequired is set to true for this keyword, then a PEL entry will be
generated.

### isPelRequired

This optional tag allows to log a PEL when both source and destination contains
the default value.

### isManufactureResetRequired

This optional tag used by vpd-tool mfgClean to reset the keyword value to its
default during manufacturing cleanup.

## Sample JSON

```json
{
  "source": {
    "hardwarePath": "/sys/bus/i2c/drivers/at24/8-0050/eeprom"
  },
  "destination": {
    "inventoryPath": "/xyz/openbmc_project/inventory/system/chassis/motherboard"
  },
  "type": "IPZ",
  "backupMap": [
    {
      "sourceRecord": "VSYS",
      "sourceKeyword": "BR",
      "destinationRecord": "VSYS",
      "destinationKeyword": "BR",
      "defaultValue": [32, 32],
      "isPelRequired": true,
      "isManufactureResetRequired": true
    },
    {
      "sourceRecord": "UTIL",
      "sourceKeyword": "F5",
      "destinationRecord": "UTIL",
      "destinationKeyword": "F5",
      "defaultValue": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
      "isPelRequired": false,
      "isManufactureResetRequired": true
    }
  ]
}
```
