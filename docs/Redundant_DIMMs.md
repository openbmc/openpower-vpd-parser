# Redundant DIMM VPD Handling

## Overview
DIMM VPD is of type non-IPZ which has CRC for error detection. Error correction mechanism is not supported in DIMM VPD. So if the VPD gets corrupted, replacement is the only option.

As DIMMs are more expensive, having redundant DIMM VPD support in a system helps in avoiding immediate DIMM replacement if one VPD goes bad. 4U systems supports redundant DIMM VPDs.

This document is about handling redundant DIMM VPD in BMC VPD applications viz., ibm-read-vpd, vpd-manager
and vpd-tool.

## Defining redundant DIMM VPD in json
The redundant DIMM VPD is defined in the form of key value pair in the JSON as

```redundantEEPROM : <path to redundant EEPROM>```

Where the redundantEEPROM acts as secondary EEPROM and the parent EEPROM acts as primary.

Since redundant DIMM VPD support is available only in 4U systems, only 4U system VPD JSON needs updation.

## Handle redundant DIMM VPD parsing

VPD-parser application (ibm-read-vpd) is responsible in binding DIMM VPD(both primary and secondary) to the driver as and when CPU FRU gets detected by BMC. The secondary DIMM EEPROM is bound first so that if primary EEPROM is broken(CRC fails for the data), VPD parser can make use of the secondary EEPROM by trusting its presence.

If both primary and secondary EEPROMs are broken, VPD parser calls out the FRU.
If CRC for both primary and secondary EEPROM are valid but CRC between primary and secondary doesn't match, then vpd-parser populates primary VPD on D-Bus (assuming primary is good) and logs a predictive PEL to call out DIMM.

## Handle redundant DIMM VPD read and write

VPD applications reads VPD from primary if it is good. If primary VPD is corrupted, it reads from secondary VPD. If both primary and secondary VPDs' are corrupted, there occurs a read failure and required actions will be taken.

DIMM VPD writes are not allowed as there is no mechanism to update the ECC after a write.

## Validation in MFG mode

Read all CRC-protected regions of the SPD from both EEPROMs, and make a predictive callout of the DIMM

* if either EEPROM has a read error
* if any of the CRC-protected regions of the SPD from either EEPROM has a CRC error
* if any of the CRC-protected regions of the SPD don't match between the copy in EEPROM 1 and EEPROM 2

## Validation in non-MFG mode

* When there is a read error or CRC error in primary EEPROM, read from secondary EEPROM. If secondary EEPROM also has either a read error or CRC error, make a predictive callout of the DIMM (with deconfig, and guard)
* If secondary EEPROM read is successful, make only hidden log due to problem with primary EEPROM.
