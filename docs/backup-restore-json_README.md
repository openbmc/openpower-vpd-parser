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
defines a `source` (a hardware EEPROM path), its VPD format `type`, and an array
of `destinations`. Each destination entry represents an independent backup
target (either a D-Bus inventory path or a hardware EEPROM path) with its own
dedicated `backupMap` of records and keywords to synchronize.

```json
{
  "backupRestoreGroups": [
    {
      "source": {
        "hardwarePath": "<SOURCE_HARDWARE_EEPROM_PATH>"
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
  VPD.
  - **hardwarePath** - String (mandatory). The sysfs hardware/EEPROM path for
    the source device (e.g. `/sys/bus/i2c/drivers/at24/8-0050/eeprom`). It must
    contain VPD data in a format whose parsing is supported by VPD-Manager.
- **type** - String (mandatory). Specifies the VPD format. It must always be a
  type whose parsing is supported by VPD-Manager (e.g. `"IPZ"` or `"KEYWORD"`).
  Because the source is always a hardware EEPROM path, this field reflects the
  VPD type of that source EEPROM. If any destination in this group is also
  configured as a hardware EEPROM path, then as per current support, that
  destination EEPROM must also be of the same VPD type as the source.
  - `"IPZ"` - IBM IPZ formatted VPD (Record and Keyword structure).
  - `"KEYWORD"` - Generic Keyword-Value formatted VPD.
- **destinations** - Array (mandatory). List of destination configurations
  associated with this source. Each entry defines a destination endpoint and its
  own independent `backupMap`.

---

## Destination-Level Fields

Each element in the `destinations` array represents a distinct target endpoint
paired with the source and contains the following fields:

- **destination** - Object (mandatory). Specifies the backup destination
  location. It can be configured with either an inventory path or a hardware
  EEPROM path. At least one of `inventoryPath` or `hardwarePath` is mandatory:
  - **inventoryPath** - String (conditionally mandatory; required if
    `hardwarePath` is not provided). D-Bus inventory object path for the
    destination (e.g.
    `/xyz/openbmc_project/inventory/system/chassis/motherboard`). When
    configured with an inventory path, the restored/synchronized data is updated
    only on D-Bus under that inventory path.
  - **hardwarePath** - String (conditionally mandatory; required if
    `inventoryPath` is not provided). Sysfs hardware/EEPROM path for the
    destination device. When configured with a hardware path, the code updates
    the data on the physical EEPROM as well as on D-Bus under the inventory path
    published for that EEPROM (provided the destination EEPROM is a valid entry
    in the main system config JSON).
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
  value (and expected length) of that keyword against which the code matches
  before performing backup or restore. The data copy/sync operation is only
  performed if the target destination keyword value matches this default value
  and exact length as defined in this field.
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
- **isBiosSyncRequired** - Boolean (optional, default `false`). Indicates
  whether updates to this keyword should trigger synchronization with BIOS /
  Host firmware attributes.

---

## Synchronization Decision Logic

During system initialization or concurrent maintenance, the VPD manager compares
the source and destination keyword values against each other and against the
configured `defaultValue`:

| Source Keyword Value        | Destination Keyword Value        | Action Taken                                                                                                                                   |
| :-------------------------- | :------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------- |
| **Valid (non-default)**     | **Default (uninitialized)**      | **Backup:** Source value is written to Destination.                                                                                            |
| **Default (uninitialized)** | **Valid (non-default)**          | **Restore:** Destination value is written to Source.                                                                                           |
| **Valid (Value A)**         | **Valid (Value B)** _(Mismatch)_ | **Conflict:** Destination value is retained on D-Bus; a `VpdMismatch` Warning PEL is logged (if enabled). _(Overridden if `copyAlways: true`)_ |
| **Default (uninitialized)** | **Default (uninitialized)**      | **Unset:** No sync occurs. If `isPelRequired` is `true`, a `VpdMismatch` Warning PEL is logged.                                                |
| **Valid (Same Value)**      | **Valid (Same Value)**           | **In Sync:** No action required.                                                                                                               |

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
