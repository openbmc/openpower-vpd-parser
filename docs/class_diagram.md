#OpenPower VPD Parser - Class Diagram

## Overview
This document provides a comprehensive class diagram for the openpower-vpd-parser project, which handles VPD (Vital Product Data) parsing, management, and operations for OpenPower systems.

## Key Components

### 1. Parser Core
- **ParserInterface**: Abstract interface for all VPD parsers
- **IpzVpdParser**: Parses IPZ format VPD with ECC checking
- **KeywordVpdParser**: Parses Keyword format VPD
- **DdimmVpdParser**: Parses DDIMM VPD (DDR4/DDR5)
- **JedecSpdParser**: Parses JEDEC SPD format
- **ParserFactory**: Factory pattern for creating appropriate parser instances
- **Parser**: Wrapper class that uses factory to select and use parsers

### 2. VPD Management
- **Manager**: Main management class exposing D-Bus APIs for VPD operations
- **Worker**: Processes VPD data, handles multi-threaded collection, and publishes to D-Bus
- **Listener**: Registers and handles D-Bus events (host state, asset tag, presence changes)
- **BackupAndRestore**: Handles VPD backup and restore operations

### 3. OEM Handler
- **IbmHandler**: IBM-specific implementation for system initialization, device tree selection, and VPD collection

### 4. BIOS Handler
- **BiosHandlerInterface**: Interface for BIOS attribute handling
- **IbmBiosHandler**: IBM-specific BIOS attribute backup/restore
- **BiosHandler<T>**: Template class for BIOS operations

### 5. GPIO Monitor
- **GpioMonitor**: Monitors GPIO pins for FRU presence
- **GpioEventHandler**: Handles GPIO events and triggers VPD collection/deletion

### 6. VPD Tool
- **VpdTool**: Command-line tool for VPD operations (read, write, dump, fix, clean)

## Design Patterns Used

1. **Factory Pattern**: ParserFactory creates appropriate parser instances
2. **Strategy Pattern**: ParserInterface with multiple implementations
3. **Template Pattern**: BiosHandler<T> for generic BIOS operations
4. **Observer Pattern**: Listener class for D-Bus event handling
5. **Singleton Pattern**: Static methods in Logger and EventLogger

## Key Relationships

- Manager orchestrates Worker, BackupAndRestore, and IbmHandler
- Worker uses Parser which uses ParserFactory to create specific parsers
- IbmHandler coordinates system-level operations using Worker, BackupAndRestore, and GpioMonitor
- Listener monitors D-Bus events and triggers Worker operations
- GpioMonitor creates GpioEventHandler instances for each monitored FRU

## Data Flow

1. **VPD Collection**: IbmHandler → Worker → Parser → ParserFactory → Specific Parser
2. **VPD Read/Write**: Manager → Worker → Parser → Specific Parser
3. **Event Handling**: D-Bus Event → Listener → Worker → Parser
4. **GPIO Monitoring**: GpioMonitor → GpioEventHandler → Worker
5. **BIOS Sync**: BiosHandler → IbmBiosHandler → Manager


## Class Brief

- **ParserInterface**
  Interface for all VPD parsers.
  Supports IPZ, Keyword, DDIMM, and JEDEC SPD formats.

- **Manager**
  Main VPD management class.
  Exposes D-Bus APIs for VPD read/write operations.

- **Worker**
  Processes and publishes VPD data.
  Handles multi-threaded FRU collection and D-Bus publishing.

- **IbmHandler**
  IBM-specific OEM handler.
  Manages system-specific VPD collection and configuration.

- **BiosHandler**
  Template class for BIOS attribute operations.
  Provides a feature to listen for BIOS attribute changes and backup/restore them.
  Works with concrete BIOS handler implementations (like IbmBiosHandler).
  Registers callbacks to listen to PLDM service for BIOS attribute changes.
  Can be used in factory reset scenarios to restore BIOS attributes from backed up values.

- **Logger**
  Utility class for logging messages.
  Provides static methods for logging various types of messages throughout the application.