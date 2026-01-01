# System JSON File Documentation

This README documents the purpose, structure, and usage of system JSON files
used for configuration. It includes descriptions of mandatory fields, expected
formats, and examples tIo help developers understand and extend the system.

---

## devTree

This is a reference to the specific .dtb filename .

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
the required muxes.Each mux must be defined with the below fields.

- **i2bus** - The I2C bus number where the mux is connected.
- **deviceaddress** - The 7-bit I2C address of the mux device.
- **holdidlepath** -  The absolute sysfs path to the mux driver's hold_idle
control node.

## frus

This mandatory section maps a physical EEPROM path to an array of one or more
inventory objects.The first object in this array is considered the base FRU for
that physical path.

> **Note:** All fields by default are optional except the field which are marked
> as mandatory.

### `<ACTION_GROUP>`

Action group define high-level events during VPD collection process(e.g.,
preAction,postAction, postFailAction). Multiple groups are allowed.Currently
following action groups are supported:

- **preAction** - Executed before FRU initialization or VPD collection.
- **postAction** - Executed after successful FRU initialization or VPD
collection.
- **postFailAction** - Executed when FRU initialization or VPD collection fails,
allowing the system to be restored to a safe state.

#### `<ACTION_PHASE>`

Represents a specific phase within the group(e.g.,collection, deletion).
Multiple phases allowed. Currently following phases are supported:

- **collection** - The collection phase is executed when the FRU's VPD data is
  getting collected.
- **deletion** - The deletion phase is executed when a FRU is removed.

##### `<ACTION_TAG>`

Specifies the action to perform in a given phase.Currently following tags are
supported:

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
        "readOnly": "<bool: FRU or its VPD data is read-only>"
      },
      {
        "<All FRUs at index 1 or onwards are considered child FRUs>"
        "inherit" : "<bool: indicating whether the child FRU inherits all
        properties from the base FRU>",
        "copyRecords": "<string, specifies which VPD fields should be copied
        from the parent/base FRU. This is an array of field names,allowing the
        child FRU to copy/inherit only the relevant VPD informations>",

      },
    ]
  }
}
```
