This document describes the meaning and purpose of the tags used in the system JSON files.

#devTree - Specifies the device tree path or reference.
The device tree describes the hardware layout of the platform, including buses, devices, and how components are physically connected.

#backupRestoreConfigPath - defines the file path used by the system to backup and restore the system JSON configuration.
This ensures FRU configuration can be restored during system recovery or firmware updates.

#commonInterfaces specifies the standard D-Bus interfaces and properties that apply to all FRUs in the system.
This section defines shared attributes placed under xyz.openbmc_project.Inventory.Decorator.Asset.

Common asset properties include:

SerialNumber

PartNumber

Model

SparePartNumber

BuildDate

Each of these fields maps to:

keywordName – The keyword used in VPD to locate the value.
recordName – The VPD record from which the value is extracted.

#muxes

Provides configuration for I2C multiplexer (mux) devices present in the system.
If the system uses muxes for routing I2C communication, each mux must be defined with the following fields:

i2cbus	        The I2C bus number where the mux is connected.
deviceAddress	The 7-bit I²C address of the mux device.
holdIdlePath	Path to the mux driver's hold_idle control node, used to place the mux in idle/hold mode. Example: "/sys/bus/i2c/drivers/pca954x/4-0070/hold_idle"


"frus" ->
{
  EEPROM path for the fru for e.g - "/sys/bus/i2c/drivers/at24/8-0050/eeprom"[
{
  First index is considered to be the base FRU .
  inventory path
  "isSystemVpd" flag denotes the given EEPROM path points to the main system VPD.
  "extraInterfaces" - The properties which are specific for the given FRU. It may 
  contain information such as serial number, model,submodel and LocationCode(location code of the FRU as per the workbook).
  "PrettyName" - The nick name of the mentioned FRU given by us.
  "serviceName" - name of the service .
}

# Next all the FRUs at index 1 or onwards positions are considered to be the child FRUs .
{  
  "inherit" flag denotes whether the child FRU inherits all the properties of the base FRU .

  "copyrecords" flag is used to copy specific field records from parent/base FRU.   

  "embedded" flag indicates whether a sub-FRU is physically built into its parent FRU. 
   Embedded sub-FRUs always have Present=true; non-embedded sub-FRUs always have Present=false.

  #synthesized FRUs for which vpd-manager does not collect VPD. Their VPD comes from another firmware component,
   but they still appear in the inventory. Therefore, they are defined in the system configuration JSON.

  #noprime flag indicates that the FRU is excluded from priming. vpd-manager will not create its inventory object d.
  Note: Priming is the process where vpd-manager creates initial inventory objects on D-Bus for FRUs defined in the system JSON.

  #preAction specifies actions that must be performed before collecting VPD or initializing the FRU.
  {
    Sub-tags:
    #collection – Group of pre-actions to prepare the hardware or system for VPD collection.
    {
      #gpioPresence – Configure or check a GPIO used to detect the FRU’s presence.
      {
            #pin: The GPIO pin controlling presence detection.
            #value: Expected value indicating the FRU is present.
      },

     #setGpio – Set a GPIO pin to a specific state required for FRU initialization.
     {
          #pin: The GPIO pin to control.
          #value: The value to set .
     },
     #systemCmd – Execute a system command to prepare the hardware before VPD collection.
     {
          #cmd: The shell command to run.
     }
    }
   #deletion - Specifies commands to clean up hardware or driver state.
   {
     #systemCmd – Runs a shell command to unbind or reset a driver.
     {
         #cmd: The shell command to run.
     }
   }   
  } 

 #postAction - Actions executed after VPD collection or FRU initialization, or when the FRU is removed from the system.
 {
  #collection - Actions to execute after FRU initialization/VPD collection.
  {
    #ccin - Card IDs associated with this action.
    #systemCmd - Execute a shell command to finalize hardware setup or bind drivers.
    {
          #cmd: The shell command to run.
    }
  }  
  #deletion - Actions executed after FRU removal to release resources.  
  {
    #systemCmd - Execute a shell command to unbind drivers.
    {
          #cmd: The shell command to run.
    }
    #setGpio – Reset a GPIO pin to a safe state after data collection.
     {
          #pin: The GPIO pin to reset.
          #value: The value to reset .
     }
  }
 }

 #postFailAction defines actions to perform if FRU initialization or VPD collection fails, to safely restore hardware to a known state.
 {
    #collection - Actions executed immediately after a failed FRU initialization or VPD collection, before any cleanup.
    {
      #setGpio – Reset a GPIO pin to a safe state after failed data collection.
      {
         #pin: The GPIO pin to reset.
         #value: The value to reset .
      }
    }
   #deletion - Actions executed when cleaning up after a failed FRU operation.
   {
     #setGpio – Reset a GPIO pin to a safe state after failed data collection.
      {
         #pin: The GPIO pin to reset.
         #value: The value to reset .
      }
   }
 }
 
#replaceableAtStandby - denotes if the FRU is replacable at standby.
#replaceableAtRuntime - denotes if the FRU is replacable at runtime.

#pollingRequired - specifies configurations where the system must poll GPIO pins to detect hardware state changes.
#hotPlugging - refers to the ability to insert or remove a FRU from the system at any time without needing a reboot.

#redundantEeprom - is used in system incase if the primary EEPROM fails, is inaccessible, or returns invalid data, the system can fall back to the redundant EEPROM to retrieve VPD.

#cpuType indicates the role/type of the CPU in a multi-processor system.
The value "primary" specifies that this CPU is the main or boot CPU, responsible for system initialization.

#powerOffOnly flag true indicates that the VPD for this FRU can only be collected when the chassis power is OFF.

#offset — The byte position inside the EEPROM where the shared VPD block begins.

#size — The total number of bytes allocated for this shared VPD block

E.g In platforms where all CPU/DCM combinations share the same VPD content, the same offset and size values are reused for each CPU entry.

  # For PCIeSlot, few additional properties(e.g - SlotType) can be added in extraInterfaces .
  # For PCIeCard, few additional properties(e.g - Slot Number) can be added in extraInterfaces .

}
]




}

