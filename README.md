# ESP32 UART-Based Command and Diagnostics Interface

An Embedded C firmware project that implements a UART-based command-line interface for an ESP32 using the **ESP-IDF framework**. The project enables users to control GPIO peripherals, execute system commands, monitor memory and runtime statistics, and access command history and event logs through a serial terminal.

The firmware is developed and tested using **ESP-IDF and Wokwi simulation**, without requiring physical ESP32 hardware.

## Features

### 1. UART Communication
- Configured UART0 communication at **115200 baud rate**.
- Uses 8 data bits, no parity, and 1 stop bit (8-N-1).
- Implements buffered UART communication using the ESP-IDF UART driver.
- Uses UART VFS integration for standard input/output operations.

### 2. Command-Line Interface (CLI)
- Interactive serial terminal interface.
- Supports command parsing, validation, and execution.
- Automatically converts commands to uppercase.
- Handles leading and trailing whitespace.
- Supports backspace and newline-based command submission.
- Handles invalid commands and input-buffer overflow.

### 3. GPIO and LED Control
- Configures GPIO2 as a digital output.
- Supports LED ON and OFF commands.
- Implements programmable LED blinking from 1 to 10 cycles.
- Restores the previous LED state after blinking.

### 4. System Diagnostics
Provides real-time system information through UART commands:
- System uptime.
- Free heap memory.
- Minimum free heap memory.
- Largest available contiguous heap block.
- Firmware application version.
- ESP-IDF framework version.
- Current LED state.

### 5. Command History
- Implements a fixed-size circular buffer to store the last 5 commands.
- Displays previously executed commands.
- Supports clearing command history.

### 6. Event Logging
- Implements a fixed-size circular buffer for the last 10 system events.
- Stores event information with uptime timestamps.
- Supports viewing and clearing event logs.

### 7. Communication Statistics
Tracks UART and command-processing activity:
- Total received bytes.
- Number of valid commands.
- Number of invalid commands.
- Supports displaying and clearing communication statistics.

### 8. System Testing and Management
- Implements a software self-test for UART command input, GPIO operation, and heap availability.
- Provides a software reset command using the ESP-IDF restart API.
- Includes error handling for invalid inputs and unsupported commands.

## Supported Commands

| Command | Description |
|---|---|
| `HELP` | Displays all available commands |
| `PING` | Checks CLI responsiveness |
| `LED ON` | Turns the LED ON |
| `LED OFF` | Turns the LED OFF |
| `LED BLINK <n>` | Blinks the LED for n cycles (1–10) |
| `GET STATUS` | Displays overall system status |
| `GET UPTIME` | Displays system uptime |
| `GET HEAP` | Displays heap memory statistics |
| `GET VERSION` | Displays firmware and ESP-IDF versions |
| `GET STATS` | Displays UART communication statistics |
| `HISTORY` | Displays previously executed commands |
| `CLEAR HISTORY` | Clears command history |
| `GET LOG` | Displays stored event logs |
| `CLEAR LOG` | Clears event logs |
| `SELF TEST` | Executes software self-tests |
| `CLEAR STATS` | Resets communication statistics |
| `RESET` | Restarts the ESP32 |

## Hardware and Software Requirements

### Hardware
- ESP32 development board (optional for physical deployment).
- USB-to-UART interface (for physical hardware testing).

### Software
- Embedded C
- ESP-IDF v6.1.1
- Visual Studio Code
- Wokwi Simulator
- CMake
- FreeRTOS APIs provided by ESP-IDF

## Project Structure

```text
uart_diagnostics/
│
├── main/
│   ├── main.c
│   └── CMakeLists.txt
│
├── build/
│   └── Generated build files
│
├── CMakeLists.txt
├── sdkconfig
├── wokwi.toml
└── README.md
```

## Building the Project

### Step 1: Clone the Repository

```bash
git clone https://github.com/ShouvikNITS/ESP32-UART-Command-Diagnostics.git
```

### Step 2: Navigate to the Project

```bash
cd ESP32-UART-Command-Diagnostics
```

### Step 3: Configure ESP-IDF Environment

Open the ESP-IDF terminal or configure the ESP-IDF environment in Visual Studio Code.

### Step 4: Build the Firmware

```bash
idf.py build
```

### Step 5: Flash the Firmware (Physical ESP32)

Connect an ESP32 development board and execute:

```bash
idf.py -p COMx flash monitor
```

Replace `COMx` with the appropriate serial port.

## Simulation and Testing

The project was tested using the Wokwi ESP32 simulator.

The simulation validates:
- UART command transmission and reception.
- CLI command parsing and execution.
- GPIO-based LED control.
- LED blinking functionality.
- System uptime and heap diagnostics.
- Command history and event logging.
- Communication statistics.
- Self-test and software reset commands.

All implemented commands were tested successfully in the simulation environment.

## Implementation Notes

- Command history and event logs use RAM-based circular buffers and are cleared after a system restart.
- LED blinking is implemented synchronously, so CLI command processing pauses while the LED is blinking.
- The self-test checks the software UART input path, GPIO API operation, and heap availability. It does not perform a physical electrical UART loopback test.
- Physical hardware testing has not been performed as part of the simulation-only validation.

## Skills Demonstrated

- Embedded C Programming
- ESP32 Microcontroller Programming
- UART Serial Communication
- GPIO Peripheral Interfacing
- ESP-IDF Framework
- FreeRTOS API Usage
- Command Parsing and Input Validation
- Circular Buffer Implementation
- Embedded System Diagnostics
- Heap Memory Monitoring
- Firmware Debugging and Simulation
- CMake-Based Embedded Build Systems

## Future Improvements

- Implement non-blocking LED blinking using FreeRTOS tasks.
- Add UART command timeout and communication error handling.
- Implement persistent event logging using non-volatile storage.
- Add UART loopback testing on physical hardware.
- Introduce a more scalable command parser and modular firmware architecture.
