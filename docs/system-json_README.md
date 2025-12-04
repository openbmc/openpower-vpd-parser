# System JSON File Documentation

This README documents the purpose, structure, and usage of system JSON files
used for configuration. It includes descriptions of mandatory fields, expected
formats, and examples to help developers understand and extend the system.

---

## devTree

Specifies the device tree path or reference.The device tree describes the
hardware layout of the platform, including buses,devices, and how components are
physically connected.

### backupRestoreConfigPath

It is the file path where essential system VPD records are backed up. This
backup is used to restore system VPD data if the BMC or motherboard or both are
replaced, ensuring reliable system recovery. This is an optional field.

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
present in the system. Each mux must be defined with the following fields:

- **i2cbus** — The I2C bus number where the mux is connected.
- **deviceAddress** — The 7-bit I²C address of the mux device.
- **holdIdlePath** — Path to the mux driver's `hold_idle` control node.Example:
  `/sys/bus/i2c/drivers/pca954x/4-0070/hold_idle`

---

## frus

This section is mandatory. First index is considered the base FRU.

```jsonc
  {
    //EEPROM path for the FRU, e.g.:
    // "/sys/bus/i2c/drivers/at24/8-0050/eeprom"

    // First index is considered the base FRU
    "inventoryPath": "...",

    // Indicates this EEPROM path points to the main system VPD
    "isSystemVpd": true,

    // Properties specific to the given FRU (serial, model, location code,
    // Slot Type, Slot Number etc)
    "extraInterfaces": { ... },

    // Nickname of the FRU
    "PrettyName": "...",

    // Name of the service hosting the Inventory object path
    "serviceName": "...",
}
```

## preAction

Actions that must be performed before collecting VPD or initializing the FRU.
There are two types of preAction steps: **collection** and **deletion**. This
list is not limited to these types — additional preAction tags may be added in
the future as required.

### collection (preAction)

Group of pre-actions needed before VPD collection.

#### gpioPresence

Configure or check a GPIO used to detect the FRU’s presence.

##### pin (gpioPresence)

The GPIO pin controlling presence detection

##### value (gpioPresence)

Expected value to indicate presence

#### setGpio (preAction_collection)

Set a GPIO pin to a required state for initialization.

##### pin (setGpio_preAction_collection)

GPIO pin

##### value (setGpio_preAction_collection)

Value to set

#### systemCmd (preAction_collection)

Execute a system command before VPD collection.

##### cmd (systemCmd_preAction_collection)

Shell command to run

### deletion (preAction)

Actions to clean up hardware or driver state.

#### systemCmd (preAction_deletion)

Execute a system command before VPD collection.

##### cmd (systemCmd_preAction_deletion)

Shell command to run

## postAction

Actions executed after VPD collection, FRU initialization, or FRU removal. There
are two types of postAction steps: **collection** and **deletion**. This list is
not limited to these types — additional postAction tags may be added in the
future as required.

---

### collection (postAction)

Commands that need to be filled for the postAction _collection_ step.

#### ccin

Card IDs associated with the action

#### systemCmd (postAction_collection)

Shell command executed after initialization

##### cmd (systemCmd_postAction_collection)

Command to run

---

### deletion (postAction)

Actions executed after FRU removal. Commands that need to be filled for the
postAction _deletion_ step.

#### systemCmd (postAction_deletion)

Shell command executed after collection the VPD.

##### cmd (systemCmd_postAction_deletion)

Command to run

#### setGpio (postAction_deletion)

Reset GPIOs to safe state.

##### pin (setGpio_postAction_deletion)

GPIO pin

##### value (setGpio_postAction_deletion)

Reset value

## postFailAction

Defines actions to perform if FRU initialization or VPD collection fails, to
safely restore hardware to a known state.There are two types of postFailAction
steps: **collection** and **deletion**. This list is not limited to these types
additional postFailAction tags may be added in the future as required.

### collection

Actions executed immediately after a failed FRU initialization or VPD
collection, before any cleanup.

#### setGpio (postFailAction_collection)

Reset a GPIO pin to a safe state after failed data collection.

##### pin (setGpio_postFailAction_collection)

The GPIO pin to reset.

##### value (setGpio_postFailAction_collection)

The value to reset.

### deletion

Actions executed when cleaning up after a failed FRU operation.

#### setGpio (postFailAction_deletion)

Reset a GPIO pin to a safe state after failed data collection.

##### pin (setGpio_postFailAction_deletion)

The GPIO pin to reset.

##### value (setGpio_postFailAction_deletion)

The value to reset.

## FRU Properties

```json
{
  // Name of the FRU
  "fruName": "",

  // FRU can be replaced while system is in standby
  "replaceableAtStandby": ,

  // FRU can be replaced while system is running
  "replaceableAtRuntime": ,

  // System polls GPIO pins to detect hardware state changes
  "pollingRequired": ,

  // FRU can be inserted or removed without reboot
  "hotPlugging": ,

  // Use redundant EEPROM if primary EEPROM fails or is inaccessible
  "redundantEeprom": ,

  // Role/type of CPU; "primary" denotes it is main boot CPU
  "cpuType": "",

  // VPD can be collected only when chassis power is OFF if true
  "powerOffOnly": ,

  // Sub-FRU physically built into its parent FRU
  "embedded": ,

  // FRUs for which vpd-manager does not collect VPD.
  // VPD comes from another firmware component but still appears
  // in inventory
  "synthesized": ,

  // FRU is excluded from priming
  "noprime": ,

  // Byte position inside EEPROM for shared VPD block
  "offset": ,

  // Total number of bytes allocated for the shared VPD block
  "size": ,

  // Type of hardware bus used to access the device
  "busType": "",

  // Linux kernel driver to use
  "driverType": "",

  // Sysfs device path for the hardware
  "devAddress": "",

  // Indicates whether vpd-manager handles FRU presence
  "handlePresence": ,

  // Indicates whether FRU presence should be actively monitored
  "monitorPresence": ,

  // Denotes whether FRU is essential for system operation
  "essentialFru": ,

  // Indicates whether the FRU or its data is read-only
  "readOnly":
}
```

## Child FRUs

All FRUs at index 1 or onwards are considered child FRUs.

### inherit

Boolean flag indicating whether the child FRU inherits all properties from the
base FRU. Example: `true` means the child FRU inherits base FRU properties.

### copyRecords

Specifies which VPD fields should be copied from the parent/base FRU. This is an
array of field names,allowing the child FRU to copy/inherit only the relevant
VPD informations.
