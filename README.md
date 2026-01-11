# SENTINEL-X
## Fault-Tolerant Embedded System with Redundancy & Automated Recovery

### Overview
SENTINEL-X is a prototype-level embedded system designed to explore fault tolerance,
redundancy, and automated recovery concepts in cyber-physical systems.
The project emphasizes system behavior and resilience rather than performance optimization.

The project focuses on detecting subsystem failures, isolating faulty components,
and switching to backup modules to maintain operation with minimal disruption.

This is a deterministic, rule-based prototype and not a production or safety-critical system.


### Motivation
In embedded and autonomous systems where manual intervention is not possible,
failure resilience becomes critical. This project was built to understand how
basic recovery behavior can be implemented using lightweight control logic.


### System Capabilities
- Continuous health monitoring using heartbeat signals and timeouts
- Fault detection and isolation
- Automated failover to redundant components
- System reconfiguration after recovery
- Event logging via serial output


### Example Fault Scenario
**Motor A Failure**
1. Heartbeat signal is lost
2. Timeout threshold is exceeded
3. Motor A is isolated
4. Backup Motor B is activated
5. System state is updated and operation continues

### Architecture
Refer to `architecture.png` for system-level design.


### Hardware Used
- Arduino-based microcontroller
- Redundant motors / actuators
- Motor driver / relay module
- Power supply
- Breadboard-based interconnections


### Software
- Arduino (.ino)
- Deterministic control logic
- Watchdog-style fault detection
- State-based recovery handling

### Validation
Faults were manually injected by:
- Disconnecting motor power
- Simulating sensor timeouts
- Forcing communication loss

Observed behavior:
- Faults were detected within predefined timeout windows
- Failed components were isolated
- Backup modules were activated automatically


### Current Status
Prototype completed. Iteration and improvements ongoing.


### Limitations
- No machine learning or adaptive optimization
- No formal fault tree analysis
- Not intended for production deployment


### Future Improvements
- Learning-based recovery optimization
- Digital twin simulation
- Distributed fault logging
- Enhanced diagnostics and metrics
