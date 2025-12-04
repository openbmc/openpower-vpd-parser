# System JSON File Documentation

This README documents the purpose, structure, and usage of system JSON files
used for configuration. It includes descriptions of mandatory fields, expected
formats, and examples to help developers understand and extend the system.

---

## devTree

Specifies the device tree path or reference.
The device tree describes the hardware layout of the platform, including buses,
devices, and how components are physically connected.

## backupRestoreConfigPath

Defines the file path used by the system to back up and restore the system
JSON configuration.
This ensures FRU configuration can be restored during system recovery or
firmware updates.

## commonInterfaces

Specifies the standard DBus interfaces and properties that apply to all FRUs
in the system.
This section defines shared attributes under
`xyz.openbmc_project.Inventory.Decorator.Asset`.

Common asset properties include:

- SerialNumber
- PartNumber
- Model
- SparePartNumber
- BuildDate

Each of these fields maps to:

- **keywordName** – The keyword used in VPD to locate the value.
- **recordName** – The VPD record from which the value is extracted.

## muxes

Provides configuration for I2C multiplexer (mux) devices present in the
system.
Each mux must be defined with the following fields:

- **i2cbus** — The I2C bus number where the mux is connected.
- **deviceAddress** — The 7-bit I²C address of the mux device.
- **holdIdlePath** — Path to the mux driver's `hold_idle` control node.
  Example: `/sys/bus/i2c/drivers/pca954x/4-0070/hold_idle`

---

## frus

First index is considered the base FRU.

```jsonc
  {
    //EEPROM path for the FRU, e.g.:
    // "/sys/bus/i2c/drivers/at24/8-0050/eeprom"

    // First index is considered the base FRU
    "inventoryPath": "...",

    // Indicates this EEPROM path points to the main system VPD
    "isSystemVpd": true,

    // Properties specific to the given FRU (serial, model, location code...)
    "extraInterfaces": { ... },

    // Nickname of the FRU
    "PrettyName": "...",

    // Name of the service handling this FRU
    "serviceName": "..."
}
```

## preAction

Actions that must be performed before collecting VPD or initializing the FRU.
There are two types of preAction steps: **collection** and **deletion**.

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

Actions executed after VPD collection, FRU initialization, or FRU removal.
There are two types of postAction steps: **collection** and **deletion**.

---

### collection (postAction)

Commands that need to be filled for the postAction *collection* step.

#### ccin

Card IDs associated with the action

#### systemCmd (postAction_collection)

Shell command executed after initialization

##### cmd (systemCmd_postAction_collection)

Command to run

---

### deletion (postAction)

Actions executed after FRU removal.
Commands that need to be filled for the postAction *deletion* step.

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

Defines actions to perform if FRU initialization or VPD collection fails,
to safely restore hardware to a known state.
There are two types of postFailAction steps: **collection** and **deletion**.

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

### replaceableAtStandby

Denotes if the FRU is replaceable at standby.

### replaceableAtRuntime

Denotes if the FRU is replaceable at runtime.

### pollingRequired

Specifies configurations where the system must poll GPIO pins
to detect hardware state changes.

### hotPlugging

Refers to the ability to insert or remove a FRU from the system
at any time without needing a reboot.

### redundantEeprom

Used in case the primary EEPROM fails, is inaccessible, or returns invalid
data.
The system can fall back to the redundant EEPROM to retrieve VPD.

### cpuType

Indicates the role/type of the CPU in a multi-processor system.
The value `"primary"` specifies that this CPU is the main or boot CPU,
responsible for system initialization.

### powerOffOnly

Flag set to `true` indicates that the VPD for this FRU can only be collected
when the chassis power is OFF.

### embedded

flag indicates whether a sub-FRU is physically built into its parent FRU.
Embedded sub-FRUs always have Present = true; non-embedded sub-FRUs always
have Present=false.

### synthesized

FRUs for which vpd-manager does not collect VPD.
Their VPD comes from another firmware component but still appears in the inventory.

### noprime

Indicates the FRU is excluded from priming.
Priming is the process where vpd-manager creates initial inventory
objects on D-Bus for FRUs defined in system JSON.

### offset

The byte position inside the EEPROM where the shared VPD block begins.

### size

The total number of bytes allocated for this shared VPD block.

> **Example:**
> In platforms where all CPU/DCM combinations share the same VPD content,
> the same `offset` and `size` values are reused for each CPU entry.

## PCIe Additional Properties

### PCIeSlot

For PCIe slots, few additional properties (e.g., `SlotType`) can be added
in `extraInterfaces`.

### PCIeCard

For PCIe cards, few additional properties (e.g., `SlotNumber`) can be added
in `extraInterfaces`.

## Child FRUs

All FRUs at index 1 or onwards are considered child FRUs.

### inherit

Boolean flag indicating whether the child FRU inherits all properties
from the base FRU.
Example: `true` means the child FRU inherits base FRU properties.

### copyRecords

Specifies which VPD fields should be copied from the parent/base FRU.
This is an array of field names,allowing the child FRU to copy/inherit
only the relevant VPD informations.
