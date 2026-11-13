# Backup and Restore JSON File Documentation

This README documents the purpose, structure, and usage of the Backup and
Restore JSON configuration files used by the VPD manager. It includes
descriptions of top-level structures, group definitions, keyword mappings,
operational logic, and configuration examples.

---

## Overview

The Backup and Restore functionality enables synchronizing data between
different hardware FRUs, or between a hardware FRU and a D-Bus inventory object.
It ensures data redundancy, persistence, and consistency across configured
source and destination endpoints.

This is an optional feature and is self-triggered based on the presence of the
`backupRestoreConfigPath` tag in the main system configuration JSON. If a valid
path to a Backup and Restore JSON is provided in the main system configuration
JSON, the feature is automatically triggered. If the tag is omitted, Backup and
Restore processing is skipped.

---

## Structure

The top-level JSON contains an array of `backupRestoreGroups`. Each group
defines a `source` (a hardware EEPROM path or a D-Bus inventory path), its VPD
format `type`, and an array of `destinations`. Each destination entry represents
an independent backup target (either a D-Bus inventory path or a hardware EEPROM
path) with its own dedicated `backupMap` of records and keywords to synchronize.

```json
{
  "backupRestoreGroups": [
    {
      "source": {
        "hardwarePath": "<SOURCE_HARDWARE_EEPROM_PATH>",
        "inventoryPath": "<SOURCE_INVENTORY_PATH> (at least one of hardwarePath or inventoryPath is mandatory)"
      },
      "type": "<VPD_TYPE (e.g. IPZ, KEYWORD)>",
      "destinations": [
        {
          "destination": {
            "inventoryPath": "<DESTINATION_INVENTORY_PATH>",
            "hardwarePath": "<DESTINATION_HARDWARE_EEPROM_PATH> (at least one of inventoryPath or hardwarePath is mandatory)"
          },
          "backupMap": [
            {
              "sourceRecord": "<SOURCE_RECORD_NAME>",
              "sourceKeyword": "<SOURCE_KEYWORD_NAME>",
              "destinationRecord": "<DESTINATION_RECORD_NAME>",
              "destinationKeyword": "<DESTINATION_KEYWORD_NAME>",
              "defaultValue": [
                "<array of default byte values defining expected length>"
              ],
              "copyAlways": "<boolean: optional, default false>",
              "isPelRequired": "<boolean: optional, default false>",
              "isManufactureResetRequired": "<boolean: optional, default false>",
              "isBiosSyncRequired": "<boolean: optional, default false>"
            }
          ]
        }
      ]
    }
  ]
}
```

---

## Group-Level Fields

Each element in `backupRestoreGroups` represents a source FRU grouping and
contains the following fields:

- **source** - Object (mandatory). Specifies the primary source location for the
  VPD. At least one of `hardwarePath` or `inventoryPath` is mandatory. As for the
  destination, the source must correspond to a FRU entry in the main system
  config JSON, and the source value is read from the EEPROM when `hardwarePath`
  is configured, or from D-Bus when `inventoryPath` is configured.
  - **hardwarePath** - String (conditionally mandatory; required if
    `inventoryPath` is not provided). The sysfs hardware/EEPROM path for the
    source device (e.g. `/sys/bus/i2c/drivers/at24/8-0050/eeprom`). It must
    contain VPD data in a format whose parsing is supported by VPD-Manager.
    Note: `vpd-tool --mfgClean` requires the source to be configured with a
    `hardwarePath`.
  - **inventoryPath** - String (conditionally mandatory; required if
    `hardwarePath` is not provided). D-Bus inventory object path for the source.
- **type** - String (mandatory). Specifies the VPD format. It must always be a
  type whose parsing is supported by VPD-Manager (e.g. `"IPZ"` or `"KEYWORD"`).
  This field reflects the VPD type of the source FRU. If any destination in this
  group is configured as a hardware EEPROM path, then as per current support,
  that destination EEPROM must also be of the same VPD type as the source (e.g.
  `"IPZ"` or `"KEYWORD"`).
- **destinations** - Array (mandatory). List of destination configurations
  associated with this source. Each entry defines a destination endpoint and its
  own independent `backupMap`.

---

## Destination-Level Fields

Each element in the `destinations` array represents a distinct target endpoint
paired with the source and contains the following fields:

- **destination** - Object (mandatory). Specifies the backup destination
  location. It can be configured with either an inventory path or a hardware
  EEPROM path. At least one of `inventoryPath` or `hardwarePath` is mandatory.
  In both cases, the destination must correspond to a FRU entry in the main
  system config JSON, because the code resolves an inventory path to its FRU
  (hardware) path and a hardware path to its inventory path. The two options
  differ in where the destination's current keyword value is read from for
  comparison with the source:
  - **inventoryPath** - String (conditionally mandatory; required if
    `hardwarePath` is not provided). D-Bus inventory object path for the
    destination (e.g.
    `/xyz/openbmc_project/inventory/system/chassis/motherboard`). When
    configured with an inventory path, the destination EEPROM is not parsed; the
    source value is compared with the value already published on D-Bus under
    that inventory path. If the destination value needs to be restored/backed
    up, the data is still written to the FRU's EEPROM, as resolved from the main
    system config JSON.
  - **hardwarePath** - String (conditionally mandatory; required if
    `inventoryPath` is not provided). Sysfs hardware/EEPROM path for the
    destination device. When configured with a hardware path, the destination
    EEPROM is parsed and the source value is compared with the value stored in
    that EEPROM. If the destination value needs to be restored/backed up, the
    data is written to the physical EEPROM, and the updated data is published on
    D-Bus under the inventory path of that EEPROM.
- **backupMap** - Array (mandatory). List of individual keyword mapping objects
  that define which records and keywords to synchronize between the source and
  this specific destination.

---

## `backupMap` Entry Fields

Each entry within the `backupMap` array defines the synchronization properties
for a specific keyword:

- **sourceRecord** - String. The record name in the source VPD (e.g. `"VSYS"`,
  `"VCEN"`).
- **sourceKeyword** - String. The keyword name in the source VPD (e.g. `"BR"`,
  `"TM"`, `"SE"`).
- **destinationRecord** - String. The record name in the destination VPD.
  Typically the same as `sourceRecord`, but can map to a different record (e.g.
  `"VSBK"`).
- **destinationKeyword** - String. The keyword name in the destination VPD.
  Typically the same as `sourceKeyword`.
- **defaultValue** - Array of integers (bytes). This field contains the default
  (uninitialized) value, and the expected length, of that keyword. The code
  compares both the source and the destination keyword values against it before
  performing backup or restore. A value is treated as default only if both its
  bytes and its exact length match this field. The same `defaultValue` applies
  to both sides, so the source and destination keywords must share the same
  default value and length.
  - If the destination holds the default value and the source does not, the
    source value is backed up to the destination.
  - If the source holds the default value and the destination does not, the
    destination value is restored to the source.
- **copyAlways** - Boolean (optional, default `false`). If `true`, default value
  matching is skipped and the source keyword data is unconditionally copied to
  the destination. If there is a length mismatch between the source and the
  destination:
  - The source length determines the size of data to be read/copied.
  - If the destination length is smaller than the source length, any extra
    trailing data from the source is ignored.
  - If the destination length is larger than the source length, the copied data
    populates the leading bytes and any remaining extra destination data is
    retained.
- **isPelRequired** - Boolean (optional, default `false`). If `true`, a Platform
  Event Log (PEL) with warning severity is generated if a mismatch or
  double-uninitialized condition is detected.
- **isManufactureResetRequired** - Boolean (optional, default `false`).
  Indicates whether this keyword should be reset to default values during a
  manufacturing reset flow.
- **isBiosSyncRequired** - Boolean (optional, default `false`). Used by the
  `vpd-tool --mfgClean` flow, for entries that also have
  `isManufactureResetRequired` set to `true`. If `true` and `--mfgClean` is run
  with `--syncBiosAttributes`, the keyword is set from the corresponding BIOS
  attribute value (read from BIOS Config Manager) instead of being reset to
  `defaultValue`. Otherwise, this field is ignored.

---

## Synchronization Decision Logic

During system initialization or concurrent maintenance, the VPD manager compares
the source and destination keyword values against each other and against the
configured `defaultValue`:

| Source Keyword Value        | Destination Keyword Value        | Action Taken                                                                                                                                                                                                                                       |
| :-------------------------- | :------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Valid (non-default)**     | **Default (uninitialized)**      | **Backup:** Source value is written to Destination.                                                                                                                                                                                                |
| **Default (uninitialized)** | **Valid (non-default)**          | **Restore:** Destination value is written to Source.                                                                                                                                                                                               |
| **Valid (Value A)**         | **Valid (Value B)** _(Mismatch)_ | **Conflict:** No data is written on either side; a `VpdMismatch` Warning PEL is logged. If the destination is an `inventoryPath`, its existing D-Bus value is retained and the source EEPROM is not modified. _(Overridden if `copyAlways: true`)_ |
| **Default (uninitialized)** | **Default (uninitialized)**      | **Unset:** No sync occurs. If `isPelRequired` is `true`, a `VpdMismatch` Warning PEL is logged.                                                                                                                                                    |
| **Valid (Same Value)**      | **Valid (Same Value)**           | **In Sync:** No action required.                                                                                                                                                                                                                   |

> **Note:** If `copyAlways` is set to `true` for an entry, the decision logic
> and mismatch checks above are bypassed:
>
> - The Source keyword value is always copied to the Destination.
> - The Destination value is not retained.
> - No `VpdMismatch` PEL is logged, even if the values mismatch.

---

## Multi-Group and Multi-Destination Example

```json
{
  "backupRestoreGroups": [
    {
      "source": {
        "hardwarePath": "/sys/bus/i2c/drivers/at24/8-0050/eeprom"
      },
      "type": "IPZ",
      "destinations": [
        {
          "destination": {
            "inventoryPath": "/xyz/openbmc_project/inventory/system/chassis/motherboard"
          },
          "backupMap": [
            {
              "sourceRecord": "VSYS",
              "sourceKeyword": "TM",
              "destinationRecord": "VSYS",
              "destinationKeyword": "TM",
              "defaultValue": [32, 32, 32, 32, 32, 32, 32, 32],
              "isPelRequired": true,
              "isManufactureResetRequired": true
            },
            {
              "sourceRecord": "VSYS",
              "sourceKeyword": "SE",
              "destinationRecord": "VSYS",
              "destinationKeyword": "SE",
              "defaultValue": [32, 32, 32, 32, 32, 32, 32],
              "isPelRequired": true,
              "isManufactureResetRequired": true
            }
          ]
        },
        {
          "destination": {
            "hardwarePath": "/sys/bus/i2c/drivers/at24/7-0050/eeprom"
          },
          "backupMap": [
            {
              "sourceRecord": "VCEN",
              "sourceKeyword": "FC",
              "destinationRecord": "VCEN",
              "destinationKeyword": "FC",
              "defaultValue": [32, 32, 32, 32, 32, 32, 32, 32],
              "isPelRequired": true,
              "isManufactureResetRequired": false
            }
          ]
        }
      ]
    }
  ]
}
```
