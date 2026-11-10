# System JSON File Documentation

This README documents the purpose, structure, and usage of system JSON files
used for configuration. It includes descriptions of mandatory fields, expected
formats, and examples to understand and extend the system.

---

## Overview

The smallest valid system JSON has a `frus` section with one EEPROM path and
one inventory object for it. Each inventory object needs only two fields; every
other field is optional.

- **inventoryPath** - The D-Bus object path published for this FRU.
- **serviceName** - The D-Bus service that hosts `inventoryPath`. Currently
  only Phosphor Inventory Manager (PIM), `xyz.openbmc_project.Inventory.Manager`,
  is supported.

```json
{
  "frus": {
    "<EEPROM_PATH_OF_FRU>": [
      {
        "inventoryPath": "<string>",
        "serviceName": "<string>"
      }
    ]
  }
}
```

One EEPROM can back more than one inventory object. The first entry in the
array for an `<EEPROM_PATH_OF_FRU>` is the **base FRU**. Every entry after it is
a **child FRU**, which may or may not inherit VPD from the base FRU's EEPROM,
depending on the flags set on that entry (see
[Record inheritance flags](#record-inheritance-flags-inherit-copyrecords-skiprecords-inheritci)).

```json
{
  "frus": {
    "<EEPROM_PATH_OF_FRU>": [
      {
        "inventoryPath": "<string: base FRU>",
        "serviceName": "<string>"
      },
      {
        "inventoryPath": "<string: child FRU>",
        "serviceName": "<string>"
      }
    ]
  }
}
```

---

## Other top-level fields

Apart from `frus`, the following top-level fields are supported. They fall into
two groups:

- **Common fields** are handled by the generic code and work on any platform.
- **OEM-specific fields** are handled only by an OEM handler.

### Common fields

#### commonInterfaces

This optional section allows defining DBus interfaces and properties that are
common to all FRUs in the system configuration JSON.It serves as a place to
specify shared interfaces so they do not need to be repeated for each FRU.

These interfaces are added to every base FRU and child FRU in the `frus`
section, unless an entry's flags say otherwise (see
[Record inheritance flags](#record-inheritance-flags-inherit-copyrecords-skiprecords-inheritci)).

> **Example:** > interfaces such as
> `xyz.openbmc_project.Inventory.Decorator.Asset` may be included here when
> applicable.

Each of these fields maps to:

- **keywordName** – The keyword used in VPD to locate the value.
- **recordName** – The VPD record from which the value is extracted.

```json
{
  "commonInterfaces": {
    "xyz.openbmc_project.Inventory.Decorator.Asset": {
      "PartNumber": {
        "recordName": "VINI",
        "keywordName": "PN"
      },
      "SerialNumber": {
        "recordName": "VINI",
        "keywordName": "SN"
      }
    }
  }
}
```

#### backupRestoreConfigPath

It is path to a JSON file which will contain details required to perform backup
/restoration of EEPROM(s).For more details refer documentation of back up and
restore JSON.

### OEM-specific fields

OEM support lives in the `oem` folder of openpower-vpd-parser. Currently the
only OEM handler implemented there is the IBM handler (`ibm_handler.*pp`), and
the fields below are supported only by that handler.

An OEM that needs any other top-level field can add it to its system JSON and
implement the handling in its own OEM handler.

#### devTree

This field specifies the .dtb filename. The OEM handler uses it to apply the
correct configuration for the system. Currently supported by the IBM handler,
where it is mandatory: a missing field may result in critical system failure.

#### muxes

This field provides configuration for I2C multiplexer (mux) devices
present in the system.The system uses this information to identify and enable
the required muxes. Each mux must be defined with the below fields.

- **i2bus** - The I2C bus number where the mux is connected.
- **deviceaddress** - The 7-bit I2C address of the mux device.
- **holdidlepath** - The absolute sysfs path to the mux driver's hold_idle
  control node.

```json
{
  "muxes": [
    {
      "i2bus": "<string>",
      "deviceaddress": "<string>",
      "holdidlepath": "<string>"
    }
  ]
}
```

---

## FRU entry fields

The fields below are set on an entry of the `frus.<EEPROM_PATH_OF_FRU>` array.
Base and child FRUs use the same fields.

> **Note:** All fields by default are optional except the field which are marked
> as mandatory.

### Mandatory fields

- **inventoryPath** - String. Inventory object path. It is mandatory field.
- **serviceName** - String. Name of the service hosting the inventory object.
  It is mandatory field.

### Record inheritance flags (`inherit`, `copyRecords`, `skipRecords`, `inheritCI`)

These flags decide which parsed VPD records, and if `commonInterfaces`, are
published on that entry's `inventoryPath`. They can be set on any entry,
including the base FRU (index 0), and not only on child FRUs.

- **inherit** - Boolean, default `true`. If `true`, every record parsed from the
  EEPROM (and the `commonInterfaces`, if defined) is published on this inventory
  object. If `false`, `copyRecords` or `skipRecords` decide what is published.
  If neither `copyRecords` nor `skipRecords` is defined, no record is copied to
  that FRU's inventory path.
- **copyRecords** - Array of record names (e.g. `["VSYS"]`). Only the listed
  records are published (allow-list). Records that are not present in the parsed
  VPD are silently ignored. Must be an array. `commonInterfaces` are not
  published unless `inheritCI` is `true`.
- **skipRecords** - Array of record names. Every parsed record except the listed
  ones is published (deny-list). Must be an array, otherwise a message is logged
  and no records are published. `commonInterfaces` are not published unless
  `inheritCI` is `true`.
- **inheritCI** - Boolean, default `false`. Only meaningful when `inherit` is
  `false` (when `inherit` is `true`, `commonInterfaces` are already published).
  If `true`, the top-level `commonInterfaces` are also published on this
  inventory object, in addition to whatever `copyRecords` / `skipRecords`
  select. Keyword updates also sync the `commonInterfaces` properties of such
  entries.

#### Precedence

`copyRecords` and `skipRecords` are evaluated only when `inherit` is `false`,
and `copyRecords` takes precedence over `skipRecords`:

| `inherit`        | `copyRecords` | `skipRecords` | Records published on the inventory object        |
| ---------------- | ------------- | ------------- | ------------------------------------------------ |
| absent or `true` | ignored       | ignored       | All records, plus `commonInterfaces`             |
| `false`          | present       | ignored       | Only the records listed in `copyRecords`         |
| `false`          | absent        | present       | All records except those listed in `skipRecords` |
| `false`          | absent        | absent        | None. Only `extraInterfaces` apply               |

`inheritCI: true` is independent of the table above: when `inherit` is `false`,
it adds the `commonInterfaces` on top of the records selected by the table.

> **Note:** Use only one of `copyRecords` and `skipRecords` per entry. If a
> record is listed in both, `copyRecords` wins: the record is published and its
> keyword updates are applied to the inventory object.

#### Example

```json
{
  "inventoryPath": "/xyz/openbmc_project/inventory/system",
  "serviceName": "xyz.openbmc_project.Inventory.Manager",
  "inherit": false,
  "copyRecords": ["VSYS"]
}
```

Only the `VSYS` record is published on `/xyz/openbmc_project/inventory/system`.
Other records parsed from the same EEPROM (e.g. `VINI`) are not published on it.

### CCIN filter (`ccin`)

`ccin` is an array of card identification numbers (CCINs). It works as a
filter, compared against the CCIN in the parsed VPD. What it filters depends on
where it is defined:

- **At the FRU entry level** (alongside fields such as `powerOffOnly`): the
  FRU's `inventoryPath` is published only if the CCIN from the parsed VPD
  matches any of the entries in the array.
- **Under a `postAction` phase** (alongside the action tags): that action is
  performed only if the CCIN from the parsed VPD matches any of the entries in
  the array.

```json
{
  "inventoryPath": "<string>",
  "serviceName": "<string>",
  "ccin": ["<CCIN_1>", "<CCIN_2>"],
  "postAction": {
    "collection": {
      "ccin": ["<CCIN_1>"],
      "systemCmd": { "cmd": "<string>" }
    }
  }
}
```

In this example, the inventory object is published only if the parsed CCIN is
`<CCIN_1>` or `<CCIN_2>`, and the `systemCmd` post action runs only if it is
`<CCIN_1>`.

### Identification and D-Bus

#### Standard fields

- **extraInterfaces** - Object. It is used to define additional D-Bus interfaces
  and properties to be hosted under FRU's inventory object path. This section
  maps a specific Interface Name to a dictionary of Properties:Value pairs.
  The list applies only to the entry it is defined on; it is not shared with
  other entries. Every FRU entry (base or child) that needs extra interfaces
  must define its own `extraInterfaces` list.

  ```json
  {
    "inventoryPath": "<string>",
    "serviceName": "<string>",
    "extraInterfaces": {
      "xyz.openbmc_project.Inventory.Item": {
        "PrettyName": "<string: user-friendly FRU name>"
      },
      "xyz.openbmc_project.Inventory.Decorator.LocationCode": {
        "LocationCode": "<string: unique physical location code of the FRU>"
      }
    }
  }
  ```

- **cpuType** - String. CPU role (e.g. 'primary', 'secondary').

#### OEM-specific fields

These fields are handled only by an OEM handler. An OEM can add its own fields
here and implement the handling in its respective OEM handler.

- **isSystemVpd** - Bool. True if this EEPROM contains main system VPD.
  Currently supported by the IBM handler.

### Handling FRUs with redundant EEPROMs

A FRU that has a redundant EEPROM must set `redundantEeprom` in its base FRU
entry (index 0), giving the path of the redundant EEPROM. If VPD collection
from the primary EEPROM fails, the code falls back to collecting VPD from the
redundant EEPROM.

- **redundantEeprom** - String. Backup EEPROM path if primary fails or is
  inaccessible.

```json
{
  "frus": {
    "<PRIMARY_EEPROM_PATH>": [
      {
        "inventoryPath": "<string: base FRU>",
        "serviceName": "<string>",
        "redundantEeprom": "<REDUNDANT_EEPROM_PATH>"
      }
    ]
  }
}
```

### Detecting hot-plugged FRUs (`pollingRequired`)

A FRU that can be inserted or removed without a reboot is detected by polling a
GPIO. As per current support in the code, hot plugging must be described with
the GPIO details used to monitor it: for a `hotPlugging` entry under
`pollingRequired`, the code registers polling on the GPIO given there to detect
when the FRU is plugged in or removed.

- **pollingRequired** - Object. Groups the events that the code detects by
  polling.
  - **hotPlugging** - Object. Enables hot-plug detection for this FRU.
    - **gpioPresence** - Object. The GPIO to poll.
      - **pin** - String. GPIO pin name.
      - **value** - Integer. The pin value that indicates the FRU is present.

```json
{
  "inventoryPath": "<string>",
  "serviceName": "<string>",
  "pollingRequired": {
    "hotPlugging": {
      "gpioPresence": {
        "pin": "<GPIO_PIN_NAME>",
        "value": 0
      }
    }
  }
}
```

### Hardware access

- **busType** - String. Hardware bus type (e.g. I2C, SPI).
- **driverType** - String. Linux kernel driver name.
- **devAddress** - String. 7-bit I2C slave address.
- **offset** - Integer. Byte offset from where the data needs to be fed to the
  respective parser so that it can be parsed and published correctly.
- **size** - Integer. Size of VPD block in bytes.

### Serviceability

- **replaceableAtStandby** - Bool. FRU can be replaced when system is in
  standby.
- **replaceableAtRuntime** - Bool. FRU can be replaced while system is running.
- **embedded** - Bool. FRU is physically embedded into parent FRU.
- **essentialFru** - Bool. FRU is essential for system operation.

### Collection and presence

- **powerOffOnly** - Bool. VPD can be collected only when chassis is powered
  OFF.
- **synthesized** - Bool. FRUs for which vpd-manager does not collect VPD.
- **noprime** - Bool. FRU is excluded from priming.
- **readOnly** - Bool. FRU or its VPD data is read-only.
- **handlePresence** - Bool. vpd-manager manages FRU presence.
- **monitorPresence** - Bool. FRU presence is actively monitored.

### Action groups

Action Groups define a standardized way for system operations(e.g., FRU
initialization or data collection).They ensure tasks are handled reliably by
managing environmental setup, clean finalization, or safe recovery on failure.

An action is configured in four levels:

```text
<ACTION_GROUP>            preAction | postAction | postFailAction
  └─ <ACTION_PHASE>       collection | deletion
       └─ <ACTION_TAG>    gpioPresence | setGpio | systemCmd
            └─ <PROPERTY_NAME>: <VALUE>
```

```json
{
  "<ACTION_GROUP>": {
    "<ACTION_PHASE>": {
      "<ACTION_TAG>": {
        "<PROPERTY_NAME>": "<VALUE>"
      }
    }
  }
}
```

#### Action group

Multiple Action Groups are supported. Currently, following Action Groups are
supported:

- **preAction** - (Preparation) Actions that run before the main task to ensure
  the environment is ready.To validate prerequisites, allocate necessary
  resources, and establish the required state before the primary task.VPD
  collection depends on this step and may fail if it is skipped.
- **postAction** - (Finalization) Operations executed exclusively after the
  primary task (such as VPD collection) concludes successfully. Its purpose is
  to transition the system from a "transient processing state" to a "stable
  production state."
- **postFailAction** - (Safety Recovery) Actions that run only if a task fails,
  acting as a "safety net" to return the system to a known, safe state.

#### Action phase

Represents a specific phase within the group(e.g.,collection, deletion).
Multiple phases allowed. Currently following phases are supported:

- **collection** - The collection phase is executed when the FRU's VPD data is
  getting collected.
- **deletion** - The deletion phase is executed when a FRU is removed.

#### Action tag

Specifies the action to perform in a given phase.The actions will be performed
in the sequence in which it has been defined.To support any future actions other
than supported, functions with specific signature needs to be defined in JSON
utility file.Currently following actions are supported:

- **gpioPresence** - Checks GPIO pin to detect FRU's presence.
- **setGpio** - Set a GPIO pin to a required state for specific action.
- **systemCmd** - Shell command executed.

#### Property name / value

Represents parameters required by the corresponding action tag.

- `<PROPERTY_NAME>` defines the name of an action-specific parameter.
- `<VALUE>` defines the value assigned to that parameter.

The parameters supported for each action tag are:

- **gpioPresence**
  - **pin** - String. GPIO pin name.
  - **value** - Integer. Pin value that indicates the FRU is present.
- **setGpio**
  - **pin** - String. GPIO pin name.
  - **value** - Integer. Value to drive on the pin.
- **systemCmd**
  - **cmd** - String. Shell command to execute.

---

## Complete sample JSON

```json
{
  "devTree": "<string: .dtb filename>",
  "backupRestoreConfigPath": "<string: path to backup/restore JSON>",
  "commonInterfaces": {
    "xyz.openbmc_project.Inventory.Decorator.Asset": {
      "SerialNumber": {
        "recordName": "<RECORD>",
        "keywordName": "<KEYWORD>"
      }
    }
  },
  "muxes": [
    {
      "i2bus": "<string>",
      "deviceaddress": "<string>",
      "holdidlepath": "<string>"
    }
  ],
  "frus": {
    "<EEPROM_PATH_OF_FRU>": [
      {
        "inventoryPath": "<string>",
        "serviceName": "<string>",
        "isSystemVpd": false,
        "extraInterfaces": {
          "com.ibm.ipzvpd.Location": {
            "LocationCode": "<string>"
          },
          "xyz.openbmc_project.Inventory.Item": {
            "PrettyName": "<string>"
          }
        },
        "preAction": {
          "collection": {
            "gpioPresence": { "pin": "<string>", "value": 0 },
            "setGpio": { "pin": "<string>", "value": 0 },
            "systemCmd": { "cmd": "<string>" }
          },
          "deletion": {
            "systemCmd": { "cmd": "<string>" }
          }
        },
        "postAction": {
          "collection": {
            "ccin": ["<CCIN_1>", "<CCIN_2>"],
            "systemCmd": { "cmd": "<string>" }
          },
          "deletion": {
            "systemCmd": { "cmd": "<string>" },
            "setGpio": { "pin": "<string>", "value": 0 }
          }
        },
        "postFailAction": {
          "collection": {
            "setGpio": { "pin": "<string>", "value": 0 }
          },
          "deletion": {
            "setGpio": { "pin": "<string>", "value": 0 }
          }
        },
        "replaceableAtStandby": false,
        "replaceableAtRuntime": false,
        "pollingRequired": {
          "hotPlugging": {
            "gpioPresence": { "pin": "<string>", "value": 0 }
          }
        },
        "redundantEeprom": "<string>",
        "cpuType": "<string>",
        "powerOffOnly": false,
        "embedded": false,
        "synthesized": false,
        "noprime": false,
        "offset": 0,
        "size": 0,
        "busType": "<string>",
        "driverType": "<string>",
        "devAddress": "<string>",
        "handlePresence": false,
        "monitorPresence": false,
        "essentialFru": false,
        "readOnly": false,
        "inherit": true
      },
      {
        "inventoryPath": "<string: child FRU>",
        "serviceName": "<string>",
        "inherit": false,
        "copyRecords": ["<RECORD_1>", "<RECORD_2>"],
        "inheritCI": true,
        "extraInterfaces": {
          "xyz.openbmc_project.Inventory.Item": {
            "PrettyName": "<string>"
          }
        }
      }
    ]
  }
}
```

---

## Note on extensibility

> ⚠️ **Important** Although the JSON structure is generic and allows future
> extensions, the current implementation recognizes only the action groups and
> action tags explicitly supported by the source code. Adding new action groups
> or action tags requires corresponding updates in the source code.
