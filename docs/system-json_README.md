# System JSON File Documentation

This README documents the purpose, structure, and usage of system JSON files
used for configuration. It includes descriptions of mandatory fields, expected
formats, and examples to understand and extend the system.

---

## devTree

This field specifies the .dtb filename. While optional in the configuration JSON
for non-IBM systems, it is mandatory for IBM systems because the system uses it
to apply the correct configuration. A missing field on IBM systems may result in
critical system failure.

## backupRestoreConfigPath

It is path to a JSON file which will contain details required to perform backup
/restoration of EEPROM(s).For more details refer documentation of back up and
restore JSON.

## commonInterfaces

This optional section allows defining DBus interfaces and properties that are
common to all FRUs in the system configuration JSON.It serves as a place to
specify shared interfaces so they do not need to be repeated for each FRU.

> **Example:** > interfaces such as
> `xyz.openbmc_project.Inventory.Decorator.Asset` may be included here when
> applicable.

Each of these fields maps to:

- **keywordName** – The keyword used in VPD to locate the value.
- **recordName** – The VPD record from which the value is extracted.

## muxes

This optional field provides configuration for I2C multiplexer (mux) devices
present in the system.The system uses this information to identify and enable
the required muxes.Currently this field is limited to IBM systems.Each mux must
be defined with the below fields.

- **i2bus** - The I2C bus number where the mux is connected.
- **deviceaddress** - The 7-bit I2C address of the mux device.
- **holdidlepath** - The absolute sysfs path to the mux driver's hold_idle
  control node.

## frus

This mandatory section maps a physical EEPROM path to an array of one or more
inventory objects.The first object in this array is considered the base FRU for
that physical path.

> **Note:** All fields by default are optional except the field which are marked
> as mandatory.

### `<ACTION_GROUP>`

Action Groups define a standardized way for system operations(e.g., FRU
initialization or data collection).They ensure tasks are handled reliably by
managing environmental setup, clean finalization, or safe recovery on failure.
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

#### `<ACTION_PHASE>`

Represents a specific phase within the group(e.g.,collection, deletion).
Multiple phases allowed. Currently following phases are supported:

- **collection** - The collection phase is executed when the FRU's VPD data is
  getting collected.
- **deletion** - The deletion phase is executed when a FRU is removed.

##### `<ACTION_TAG>`

Specifies the action to perform in a given phase.The actions will be performed
in the sequence in which it has been defined.To support any future actions other
than supported, functions with specific signature needs to be defined in JSON
utility file.Currently following actions are supported:

- **gpioPresence** - Checks GPIO pin to detect FRU's presence.
- **setGpio** - Set a GPIO pin to a required state for specific action.
- **systemCmd** - Shell command executed.

###### `<PROPERTY_NAME>` / `<VALUE>`

Represents parameters required by the corresponding action tag.

- `<PROPERTY_NAME>` defines the name of an action-specific parameter.

- `<VALUE>` defines the value assigned to that parameter.

Currently following property is supported:

- **ccin** - card identification number.
- **pin** - Gpio pin name.
- **cmd** - shell command to execute.

### Record inheritance flags (`inherit`, `copyRecords`, `skipRecords`, `inheritCI`)

These flags are set on an entry of the `frus.<EEPROM_PATH_OF_FRU>` array. They
decide which parsed VPD records, and which `commonInterfaces`, are published on
that entry's `inventoryPath`. They can be set on any entry, including the base
FRU (index 0), and not only on child FRUs.

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

### Note on Extensibility

> ⚠️ **Important** Although the JSON structure is generic and allows future
> extensions, the current implementation recognizes only the action groups and
> action tags explicitly supported by the source code. Adding new action groups
> or action tags requires corresponding updates in the source code.

## Sample JSON

```json
{
   "muxes": [
        {
            "i2bus": "<string>",
            "deviceaddress": "<string>",
            "holdidlepath": "<string>"
        },
        {
            "...": "Any number of additional muxes"
        }
   ],
  "frus": {
    "<EEPROM_PATH_OF_FRU>": [
      {
        "inventoryPath": "<string: Inventory object path. It is mandatory field.>",
        "serviceName": "<string: Name of the service hosting the inventory object.
        It is mandatory field>",
        "isSystemVpd": "<bool: true if this EEPROM contains main system VPD>",

        "extraInterfaces":"<It is used to define additional D-Bus interfaces
         and properties to be hosted under FRU's inventory object path . This section
         maps a specific Interface Name to a dictionary of Properties:Value pairs>"
         {
          "<DBUS_INTERFACE>": {
              "<PROPERTY_NAME>": "<string: value>"
         }
         "...": "Any number of additional interfaces or properties"

         "Below are the examples : ",
          "com.ibm.ipzvpd.Location": {
                "LocationCode": "<string: Unique physical FRU's location code>"
            },
          "xyz.openbmc_project.Inventory.Item": {
                "PrettyName": "<string: User-friendly FRU name>"
            }
        },

        "<ACTION_GROUP>":
        {
         "<ACTION_PHASE>":
         {
          "<ACTION_TAG>": {
           "<PROPERTY_NAME>": "<VALUE>"
          },

         "...": "Any number of additional action tags"
         },

        "... ": "Any number of additional phases"
       },
        "...": "Any number of additional action groups"

        "<Below are the standard example currently utilized by the system.>",

         "preAction":
         {
                    "collection":
                     {
                        "gpioPresence":
                        {
                           "pin": "<string>",
                           "value": "<int: VALUE>"
                        },
                        "setGpio":{
                            "pin": "<string>",
                            "value": "<int: VALUE>"
                        },
                        "systemCmd":{
                            "cmd": "<string>"
                        }
                    },
                    "deletion":{
                        "systemCmd": {
                            "cmd": "<string>"
                        }
                    }
          },
          "postAction":
          {
                    "collection":
                    {
                        "ccin": ["<CCIN_1>", "<CCIN_2>", "..."],
                        "systemCmd":{
                            "cmd": "<string>"
                        }
                    },
                    "deletion":{
                        "systemCmd": {
                            "cmd": "<string>"
                        },
                        "setGpio":{
                            "pin": "<string>",
                            "value":"<int: VALUE>"
                        }
                    }
          },
         "postFailAction": {
                    "collection":
                    {
                        "setGpio":{
                            "pin": "<string>",
                            "value": "<int: VALUE_TO_SET>"
                        }
                    },
                    "deletion":
                    {
                        "setGpio": {
                            "pin": "<string>",
                           "value": "<int: VALUE_TO_SET>"
                        }
                    }
        },
        "replaceableAtStandby": "<bool: FRU can be replaced when system is in standby>",
        "replaceableAtRuntime": "<bool: FRU can be replaced while system is running>",
        "pollingRequired": "<bool: GPIO polling is required to detect state changes>",
        "hotPlugging": "<bool: FRU can be inserted or removed without reboot>",
        "concurrentlyMaintainable": "<bool: indicating whether the FRU can be physically
         serviced or replaced while the system is powered on and running>",

        "redundantEeprom": "<string: Backup EEPROM path if primary fails or is inaccessible>",

        "cpuType": "<string: CPU role (e.g. 'primary', 'secondary')>",
        "powerOffOnly": "<bool: VPD can be collected only when chassis is powered OFF>",
        "embedded": "<bool: FRU is physically embedded into parent FRU>",
        "synthesized": "<bool: FRUs for which vpd-manager does not collect VPD>",
        "noprime": "<bool: FRU is excluded from priming>",

        "offset": "<integer: Byte offset inside shared EEPROM>",
        "size": "<integer: Size of VPD block in bytes>",

        "busType": "<string: Hardware bus type (e.g. I2C, SPI)>",
        "driverType": "<string: Linux kernel driver name>",
        "devAddress": "<string: 7-bit I2C slave address>",

        "handlePresence": "<bool: vpd-manager manages FRU presence>",
        "monitorPresence": "<bool: FRU presence is actively monitored>",
        "essentialFru": "<bool: FRU is essential for system operation>",
        "readOnly": "<bool: FRU or its VPD data is read-only>",

        "inherit": "<bool, default true: if true, all records of the EEPROM
        and the commonInterfaces are published on this inventory object. If
        false, copyRecords / skipRecords decide which records are published.>",
        "copyRecords": "<array of strings: only these records are published.
        Used only when inherit is false.>",
        "skipRecords": "<array of strings: all records except these are
        published. Used only when inherit is false and copyRecords is absent.
        Applies to IPZ VPD only.>",
        "inheritCI": "<bool, default false: when inherit is false, also publish
        the commonInterfaces on this inventory object.>"
      },
      {
        "<Entries at index 1 or onwards model child FRUs: additional inventory
        objects that are populated from the same EEPROM as the base FRU. They
        use the same fields as above. Below is an example of a child FRU that
        publishes only selected records.>",
        "inventoryPath": "<string: Inventory object path of the child FRU. It is mandatory field.>",
        "serviceName": "<string: Name of the service hosting the inventory object.
        It is mandatory field>",
        "inherit": false,
        "copyRecords": ["<RECORD_1>", "<RECORD_2>", "..."],
        "inheritCI": true,
        "extraInterfaces": {
          "xyz.openbmc_project.Inventory.Item": {
            "PrettyName": "<string: User-friendly FRU name>"
          }
        }
      },
      {
        "...": "Any number of additional child FRUs"
      }
    ]
  }
}
```
