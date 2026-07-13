# VPD Manager: Multithreaded Architecture Design

## Author

Sunny Srivastava

## Created

2026-06-12

## Problem Description

The legacy openpower-vpd-parser code relied on a **udev-driven, single-threaded architecture** that had several significant limitations:

### 1. **Uncontrolled Process Spawning and Resource Inefficiency**

Multiple udev events could trigger numerous `ibm_vpd_app` process instances simultaneously without any coordination. This caused:

- Each udev event spawning a new process with full initialization overhead, making it inefficient for systems with many FRUs
- High memory consumption from multiple concurrent process instances
- Redundant parsing of the same JSON configuration files by each process
- Significant CPU overhead from process creation and context switching

The root cause was that udev triggered individual service instances (`ibm-vpd-parser@.service`) for each device without any central coordination or resource management.

### 2. **Reactive Event-Driven Model**

VPD collection was purely reactive, triggered only when the kernel detected hardware. This created several problems:

- No control over collection order or priority
- Difficult to implement retry logic or error recovery
- **Extremely difficult to determine when VPD collection was complete**: In systems where BMC ready state depends on all VPD being collected, the udev-driven approach provided no reliable way to know when all events had been generated and processed. We had to rely on a timeout-based heuristic (if no udev event for "x" seconds, assume completion), which was unreliable and could cause either premature BMC ready signaling or unnecessary delays.

The udev rules (`70-ibm-vpd-parser.rules`) triggered separate process instances without any coordination.

### 3. **Lack of Centralized Management**

There was no single service to coordinate VPD operations across the system. This meant:

- No unified view of VPD collection status
- No straightforward way to implement features like VPD recollection on demand or cross-FRU validation
- Each parser instance operated in isolation with no shared state

### 4. **Scalability Concerns**

The architecture didn't scale well for multi-chassis systems. As the number of chassis increased, the number of FRUs to parse grew exponentially. Each parser instance required its own memory allocation, file descriptors, and resources, leading to uncontrolled CPU and memory consumption that could block other system services. CPU optimization and memory management became critical concerns, especially in high-density systems.

## Requirements

### Functional Requirements

1. **Parallel VPD Collection**: Support concurrent VPD parsing for multiple FRUs.
2. **Centralized Management**: Single service to coordinate all VPD operations.
3. **Proactive Collection**: Manager-driven collection with configurable ordering.
4. **Runtime Operations**: Support VPD read/write/update operations via D-Bus APIs.
5. **Error Recovery**: Robust error handling with retry mechanisms.
6. **Status Tracking**: Real-time VPD collection progress reporting.
7. **Backward Compatibility**: Support existing JSON configurations.

### Non-Functional Requirements

1. **Performance**: Reduce total VPD collection time by 30%-40%
2. **Scalability**: Support multi chassis architecture with 200+ FRUs and configurable performance threshold.
3. **Resource Efficiency**: Configurable memory and CPU usage
4. **Reliability**: 99.9% success rate for VPD collection
5. **Maintainability**: Modular, testable architecture

## Alternatives Considered

### 1. **Keep Udev Model with Thread Pool**

We considered enhancing the udev-driven model by adding a thread pool to each instance.

**Pros**:

- Minimal architectural changes
- Preserves existing trigger mechanism

**Cons**:

- Still lacks centralized management
- No unified status tracking
- Difficult to implement advanced features
- Resource inefficiency remains

**Decision**: We rejected this approach because it doesn't address the core architectural issues.

### 2. **Fully Asynchronous Event-Driven**

We also considered using boost::asio for fully asynchronous I/O.

**Pros**:

- Single reactor thread for I/O multiplexing with no locking on the I/O path
- Scales well for socket/pipe-style descriptors that signal readiness

**Cons**:

Our analysis showed this wouldn't provide real asynchrony for VPD collection:

- VPD data is stored in I2C EEPROMs accessed via the `at24` driver through a sysfs binary attribute (e.g. `/sys/bus/i2c/devices/8-0050/eeprom`)
- The i2c subsystem has no AIO submission path - access goes through the at24 driver which does synchronous transfers
- These EEPROM device files aren't epoll-readiness-pollable. Unlike a socket, there's no "data is ready, read() won't block now" signal to feed into Asio's reactor
- For I2C character devices, there's no async I/O support in the kernel
- Linux I2C driver operations (read(), write(), ioctl()) are synchronous blocking calls. When you access an I2C device file, the kernel blocks until the I2C transaction completes
- Even if we used boost::asio::async_read(), it would internally use a thread pool to make the blocking read() call appear non-blocking, but the underlying I2C transaction would still be synchronous
- The only way to make such a blocking appear "async" under Asio would be to offload it to a worker thread ourselves (e.g. `post()` to a thread pool). That's just threading with an async wrapper, adding bookkeeping without any real I/O concurrency benefit

For multi-chassis systems:

- Async adds complexity without solving multi-chassis parallelism
- A single `io_context` runs its handlers on one thread. To get true parallelism across chassis, we'd need multiple `io_context` instances (effectively one per chassis), each driven by its own thread
- That's functionally the same as our threaded design, but with the added overhead of async callback management and completion handlers
- Threading is less complex for the multi-chassis scenario
  
**Decision**: We rejected this approach. Our analysis showed no real performance benefit for synchronous, non-pollable I2C EEPROM I/O, while it added callback/async complexity. A threaded model is simpler and achieves the same chassis-level parallelism.

## Proposed Design

### Architecture Overview

The new design introduces a persistent, multithreaded VPD Manager service that replaces the udev-driven model:

```text
┌─────────────────────────────────────────────────────────────────┐
│                         VPD Manager Service                     │
│                      (Persistent D-Bus Service)                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────┐    ┌──────────────┐    ┌────────────────┐         │
│  │ Manager  │───▶│ ConfigManager│───▶│ ThreadManager  │         │
│  │          │    │              │    │                │         │
│  │ - D-Bus  │    │ - Chassis    │    │ - Thread Pool  │         │
│  │   APIs   │    │   Config     │    │ - Concurrency  │         │
│  │ - Coord  │    │ - JSON Parse │    │ - Scheduling   │         │
│  └────┬─────┘    └──────────────┘    └───────┬────────┘         │
│       │                                      │                  │
│       │          ┌──────────────┐            │                  │
│       └─────────▶│   Worker     │◀───────────┘                  │
│                  │              │                               │
│                  │ - VPD Parse  │                               │
│                  │ - D-Bus Pub  │                               │
│                  │ - Validation │                               │
│                  └──────┬───────┘                               │
│                         │                                       │
│       ┌─────────────────┼─────────────────┐                     │
│       │                 │                 │                     │
│  ┌────▼─────┐    ┌──────▼──────┐   ┌─────▼──────┐               │
│  │ Listener │    │ParserFactory│   │BackupRestore│              │
│  │          │    │             │   │             │              │
│  │ - Events │    │ - IPZ       │   │ - Backup    │              │
│  │ - Signals│    │ - Keyword   │   │ - Restore   │              │
│  │ - Monitor│    │ - Memory    │   │ - Validate  │              │
│  └──────────┘    └─────────────┘   └─────────────┘              │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
                    ┌──────────────────┐         ┌─────────────────────────┐
                    │ Phosphor         │         │ Future Enhancement:     │
                    │ Inventory        │- - - - -│ Configurable Data       │
                    │ Manager (PIM)    │         │ Repository              │
                    └──────────────────┘         │                         │
                                                 │ Allow users to choose   │
                                                 │ inventory data storage  │
                                                 │ backend (PIM, Entity    │
                                                 │ Manager, custom, etc.)  │
                                                 └─────────────────────────┘
```

## Current Design Analysis

## Impacts

### Positive Impacts

1. **Performance**: Significant reduction in VPD collection time
2. **Scalability**: Supports high-scale systems with 100+ FRUs without creating bottlenecks
3. **Reliability**: Comprehensive error handling and recovery mechanisms
4. **Maintainability**: Modular, testable architecture
5. **Resource Efficiency**: Controlled memory and CPU usage
6. **Configurability**: Thread count can be tuned based on underlying hardware capabilities, allowing optimization to avoid bottlenecks and maximize performance

### Potential Concerns

1. **Thread Safety**: Requires careful design to avoid race conditions
   - **Mitigation**: We use per-thread Worker instances (stack-allocated) with minimal shared state
2. **I2C Bus Contention**: Multiple threads accessing I2C simultaneously
   - **Mitigation**: Semaphore-based throttling, and the kernel driver handles arbitration
3. **Complexity**: More complex than the simple udev model
   - **Mitigation**: Clear separation of concerns and comprehensive documentation
4. **Backward Compatibility**: Existing tools may expect old behavior
   - **Mitigation**: We maintain compatible D-Bus interfaces

### Thread Safety Verification

The design is thread-safe with respect to D-Bus access through per-thread connections. Multiple worker threads can safely call D-Bus simultaneously.

**sdbusplus Limitation**: A single `sdbusplus::bus` object cannot be safely used by multiple threads simultaneously.

**Our Solution**: Each thread creates its own independent D-Bus connection that's never shared between threads.

#### Thread Safety Guarantees

1. **No Shared State**: Each thread gets its own `sdbusplus::bus` connection object
2. **Stack-Allocated**: The `bus` object is created on the stack within each function call
3. **Thread-Local Scope**: Connection exists only for the duration of the D-Bus call
4. **Synchronous Calls**: Each `bus.call()` is blocking and completes before the function returns
5. **Automatic Cleanup**: Connection is destroyed when the function returns

#### Architecture Diagram

```text
Main Thread (boost::asio io_context)
    ↓
ThreadManager
    ↓
Spawns Thread 1              Spawns Thread 2              Spawns Thread N
    ↓                            ↓                            ↓
Worker (thread-local)        Worker (thread-local)        Worker (thread-local)
    ↓                            ↓                            ↓
Parse VPD from EEPROM        Parse VPD from EEPROM        Parse VPD from EEPROM
    ↓                            ↓                            ↓
publishVpdOnDBus()           publishVpdOnDBus()           publishVpdOnDBus()
    ↓                            ↓                            ↓
new_default() → bus1         new_default() → bus2         new_default() → busN
    ↓                            ↓                            ↓
bus1.call(PIM.Notify)        bus2.call(PIM.Notify)        busN.call(PIM.Notify)
    ↓                            ↓                            ↓
bus1 destroyed               bus2 destroyed               busN destroyed
```

**Critical Point**: No D-Bus connection object is shared between threads.

#### Performance Consideration

Creating a new D-Bus connection per call has minimal overhead. VPD collection is infrequent (boot time, hot-plug events), so the simplicity and safety outweigh the minor performance cost.

#### Conclusion

The multithreaded architecture is thread-safe because:

1. Each worker thread creates its own independent D-Bus connection using `sdbusplus::bus::new_default()`
2. There is no shared D-Bus connection state between threads
3. The D-Bus daemon itself handles concurrent client connections safely
4. Each thread's synchronous call completes before the connection is released

Each thread uses its own short-lived connection, which is consistent with sd-bus's requirement that a connection not be shared across threads.

### Key Components of proposed design

#### 1. **Manager** (`manager.hpp`, `manager.cpp`)

The Manager acts as the central coordinator and D-Bus interface provider.

**Key Features**:

- Exposes D-Bus APIs for VPD operations (write/read keywords, collect single FRU VPD on demand, trigger system-wide collection, delete FRU VPD, and more)
- Manages the lifecycle of Worker, Listener, ConfigManager, and ThreadManager
- Handles D-Bus service registration
- Coordinates with ConfigManager for configuration management

#### 2. **ThreadManager** (`thread_manager.hpp`, `thread_manager.cpp`)

ThreadManager orchestrates multithreaded VPD collection.

**Key Features**:

- **Chassis-Based Threading**: Works for both single and multi-chassis systems
- **Thread Pool Management**: Controls concurrent thread execution and can be optimized based on system capabilities to maximize performance
- **Status Tracking**: Updates D-Bus progress interface with collection status
- **System View**: Maintains chassis presence and inventory state
- **Completion Monitoring**: Tracks overall collection progress

This provides optimized parallel VPD collection that scales to multi-chassis systems.

#### 3. **Worker** (`worker.hpp`, `worker.cpp`)

Worker handles VPD parsing and D-Bus population.

**Key Features**:

- **Parser Selection**: Uses ParserFactory to select the appropriate parser (IPZ/Keyword/Memory). The factory pattern makes it easy to extend support for new parser types
- **VPD Parsing**: Reads EEPROM, validates, and parses VPD data
- **D-Bus Population**: Constructs interface/property maps and publishes to PIM
- **Pre/Post Actions**: Executes JSON-defined actions (GPIO, MUX control, etc.)
- **Error Handling**: Comprehensive exception handling with PEL generation
- **Thread-Safe**: Each thread gets its own Worker instance

#### 4. **ConfigManager** (`config_manager.hpp`, `config_manager.cpp`)

ConfigManager handles JSON configuration management.

**Key Features**:

- **JSON Management**: Validates JSON configuration files and acts as the single point of truth for attributes defined in JSON
- **Configuration Lookup**: Provides FRU configuration by EEPROM/inventory path
- **Singleton Pattern**: Ensures a single source of configuration truth

#### 5. **Listener** (`listener.hpp`, `listener.cpp`)

Listener monitors D-Bus events and triggers actions.

**Key Features**:

- **Presence Detection**: Monitors FRU presence changes
- **Asset Tag Updates**: Syncs asset tag changes to hardware
- **Event Handling**: Implements callback-based event processing
- **Extensibility**: Plugin architecture for adding new event handlers

#### 6. **Parser Factory** (`parser_factory.hpp`, `parser_factory.cpp`)

ParserFactory creates the appropriate VPD parser based on format.

**Supported Parsers**:

- **IpzParser**: IPZ format (VHDR→VTOC→Records→Keywords)
- **KeywordVpdParser**: Simple keyword format
- **DdimmParser**: DDR4/DDR5 DDIMM memory VPD
- **IsdimmParser**: DDR4/DDR5 ISDIMM memory VPD

The design is extensible and can support new VPD formats.

#### High-Level Flow

```text
1. System Boot
   ↓
2. vpd-manager.service starts
   ↓
3. Manager constructor:
   - Initialize D-Bus connection
   - Register D-Bus interfaces
   ↓
4. wait-vpd-parsers.service starts
   ↓
5. CollectionOrchestrator:
   - Check for inventory backup
   - If backup exists: restore and skip collection
   - If no backup: trigger VPD collection
   ↓
6. Manager.collectAllFruVpd():
   - Initialize ConfigManager with system JSON
   - Create ThreadManager
   - ThreadManager spawns threads per chassis
   ↓
7. Each thread:
   - Worker.collectFruVpd()
   - Parse VPD
   - Publish to D-Bus
   ↓
8. Collection complete
   - Update overall status
   - Backup inventory data
   ↓
9. Manager enters event loop
   - Listen for D-Bus requests
   - Monitor system events
```

### Multithreading Strategy

This approach provides several benefits:

1. **Isolation**: Each chassis has an independent Worker instance
2. **No Shared State**: Eliminates the need for complex synchronization
3. **Fault Tolerance**: One chassis failure doesn't affect others
4. **Scalability**: Naturally extends to multi-chassis systems

#### Synchronization Mechanisms

**Minimal Locking**:

- **System View Updates**: Mutex-protected chassis state map
- **Status Updates**: D-Bus property updates are atomic
- **PIM Calls**: PIM handles concurrent requests internally

**Thread-Safe Design**:

- Each thread operates on independent data
- No shared mutable state between threads
- D-Bus operations are inherently thread-safe

#### Future Enhancements and Performance Optimization

**Semaphore-Based Throttling** can prevent resource exhaustion in overloaded systems, control I2C bus contention, and maintain system responsiveness.

**Retry Mechanisms** include:

- Automatic retry on transient I2C errors
- Recollection API for manual retry
- Backup VPD restoration on critical failures

### Comparison: Old vs New Architecture

| Aspect | Old (Udev-Driven) | New (Manager-Based) |
| ------ | ----------------- | ------------------- |
| **Trigger** | Udev events (reactive) | Manager service (proactive) |
| **Concurrency** | Sequential (one at a time) | Parallel (chassis-level threads) |
| **Service Model** | Transient (per-device) | Persistent (single service) |
| **Management** | Distributed (no coordination) | Centralized (Manager) |
| **Status Tracking** | None | Real-time progress reporting |
| **Resource Usage** | High (multiple processes) | Low (single service, threads) |
| **Scalability** | Poor (linear with FRU count) | Excellent (parallel processing) |

## Future Enhancements

1. **Dynamic Thread Pool Sizing**
   - Adjust thread count based on system load
   - CPU/memory-aware scheduling
   - Adaptive throttling

2. **Alignment with Community-Driven Design**
   - Support for community-driven design patterns
   - Flexibility to choose the repository for hosting inventory paths (PIM, Entity Manager, or custom solutions)

## Conclusion

The migration from the udev-driven to manager-based multithreaded architecture represents a fundamental improvement in VPD management for OpenBMC systems. This new design addresses critical performance, scalability, and maintainability issues while providing a solid foundation for future enhancements.

**Key Achievements**:

- Significant reduction in VPD collection time
- Controlled resource usage
- Centralized management
- Robust error handling and recovery
- Scalability to multi-chassis systems
- Foundation for advanced features
