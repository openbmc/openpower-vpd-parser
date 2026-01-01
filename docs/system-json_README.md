# System JSON File Documentation

This README documents the purpose, structure, and usage of system JSON files
used for configuration. It includes descriptions of mandatory fields, expected
formats, and examples to help developers understand and extend the system.

---

## devTree

Specifies the device tree path or reference.

### backupRestoreConfigPath

It is the file path where essential FRU VPD records are backed up.This
functionality enables data synchronization between any two hardware entities to
ensure recovery when one is replaced. Note: If both entities are replaced at the
same time, data cannot be restored.

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
the required muxes. Each mux must be defined with the following fields:

```json
"muxes": [
    {
        "i2bus": "<string,The I2C bus number where the mux is connected>",
        "deviceaddress": "<string,The 7-bit I2C address of the mux device>",
        "holdidlepath": "<string,The absolute sysfs path to the mux driver's
         hold_idle control node>"
    }
]
```

## frus

This mandatory section maps a physical EEPROM path to an array of one or more
inventory objects.The first object in this array is considered the base FRU for
that physical path.

> **Note:** All fields by default are optional except the field which are marked
> as mandatory.

## Sample JSON

````json
{
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

         "EXPLANATION"
         "<DBUS_INTERFACE> -> Dbus Interface name"
         "<PROPERTY_NAME>: -> Property name and its value"

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
        "<phase>":
        {
          "<ACTION_TAG>": {
          "<PROPERTY_NAME>": "<VALUE>"
          },

         "...": "Any number of additional action tags"
         },

        "... ": "Any number of additional phases"
       }
        "...": "Any number of additional action groups"

        "EXPLANATION"
        "<ACTION_GROUP> –>  Defines the high-level event(e.g., preAction,
         postAction, postFailAction).Multiple groups are allowed"

        "<phase> –> Represents a specific stage within the group(e.g.,
        collection, deletion). Multiple phases allowed."

        "<ACTION_TAG> –> an action to perform(e.g., gpioPresence, setGpio,
         systemCmd). Multiple actions per phase."

        "<PROPERTY_NAME> / <VALUE> –> parameters for the action."

         "<Below are the standard example currently utilized by the system.>",

         "preAction": "<Actions that must be performed before collecting
          VPD or initializing the FRU>"
         {
                    "collection": "<group of pre-actions needed before
                      VPD collection>":
                     {
                        "gpioPresence": "<Configure or check a GPIO used
                        to detect the FRU’s presence>"{
                           "pin": "<string: GPIO_PIN_NAME>",
                           "value": "<int: VALUE>"
                        },
                        "setGpio": "<Set a GPIO pin to a required state
                        for initialization>"{
                            "pin": "<string: GPIO_PIN_NAME>",
                            "value": "<int: VALUE>"
                        },
                        "systemCmd": "<Shell command executed after initialization>"{
                            "cmd": "<SHELL_COMMAND_TO_EXECUTE_BEFORE_COLLECTION>"
                        }
                    },
                    "deletion": "<Actions to clean up hardware or driver state.>"{
                        "systemCmd": {
                            "cmd": "<SHELL_COMMAND_TO_CLEANUP_AFTER_DELETION>"
                        }
                    }
          },
          "postAction": "<Actions executed after VPD collection, FRU initialization
           or FRU removal>" {
                    "collection": "<Commands that need to be filled for the
                     postAction _collection_ step.>":
                    {
                        "ccin": ["<CCIN_1>", "<CCIN_2>", "..."],
                        "systemCmd":{
                            "cmd": "<SHELL_COMMAND_TO_EXECUTE_AFTER_COLLECTION>"
                        }
                    },
                    "deletion": "<Actions executed after FRU removal>"{
                        "systemCmd": {
                            "cmd": "<SHELL_COMMAND_TO_EXECUTE_AFTER_DELETION>"
                        },
                        "setGpio": "<Reset GPIOs to safe state.>"{
                            "pin": "<string: GPIO_PIN_NAME>",
                            "value":"<int: VALUE>"
                        }
                    }
          },
         "postFailAction": "<Defines actions to perform if FRU initialization or
          VPD collection fails, to safely restore hardware to a known state>": {
                    "collection": "<Actions executed immediately after a failed
                     FRU initialization or VPD collection, before any cleanup.>"
                    {
                        "setGpio": "<Reset a GPIO pin to a safe state after failed
                         data collection.>" {
                            "pin": "<string: GPIO_PIN_NAME>",
                            "value": "<int: VALUE_TO_SET>"
                        }
                    },
                    "deletion": "<Actions executed when cleaning up after a
                    failed FRU operation.>"
                    {
                        "setGpio": {
                            "pin": "<string: GPIO_PIN_NAME>",
                           "value": "<int: VALUE_TO_SET>"
                        }
                    }
        },

       "< ```Note on Extensibility``` [!IMPORTANT] Although the JSON structure
       is generic and supports future keys,the current source code only recognizes
       the specific groups and tags listed above. Adding new action groups or action
       tags requires corresponding update in the source code.>"

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
````
