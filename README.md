# DUODRIVE RC Car — ENG 177

An ESP32-based, dual-controller remote-controlled car developed for **ENG 177** at the **University of Illinois Urbana-Champaign**.

> **Project status:** Work in progress. The repository currently contains ESP-NOW controller and receiver firmware for communication testing. Motor-driving logic and an operational motor-stop failsafe have **not** yet been implemented in the committed receiver sketch.

## Project Overview

DUODRIVE explores a collaborative driving interface in which **two independent wireless controllers operate one four-wheel-drive car**:

- **Controller 1 — Throttle:** Sends commands intended for forward and reverse motion.
- **Controller 2 — Steering:** Sends commands intended for left and right turns.
- **Vehicle receiver:** Receives input from both controllers and will eventually translate the commands into motor outputs.

The ESP32 boards communicate using **ESP-NOW**, a direct, low-overhead wireless protocol. The design does **not** require an internet connection or a Wi-Fi router.

The project combines embedded programming, digital communication, electrical integration, motor control, and collaborative engineering design.

## System Architecture

```text
Controller 1 (ESP32)                  Controller 2 (ESP32)
Throttle buttons                      Steering buttons
      |                                     |
      |  ESP-NOW broadcast (channel 6)     |
      +------------------+------------------+
                         |
                         v
                 Vehicle ESP32 receiver
                 - ESP-NOW receive callback
                 - FreeRTOS message queue
                 - Command processing (TODO)
                         |
                         v
                   Motor driver (TODO)
                         |
                         v
                 Four DC drive motors
```

**Implemented:** Digital button reading, one-byte ESP-NOW broadcasting, asynchronous packet reception, queuing, and serial diagnostics.

**Planned:** Combining throttle and steering commands, motor-driver integration, PWM speed control, and verified loss-of-signal stopping.

## Hardware

| Component | Quantity | Purpose |
| --- | ---: | --- |
| ESP32 / ESP32-S3 development board | 3 | Two transmitters and one receiver |
| Digital push buttons | 4 | Two command inputs per controller in the current firmware |
| Four-wheel-drive robot chassis | 1 | Vehicle platform |
| TT DC gear motors | 4 | Wheel propulsion |
| Motor driver module | 1 or more, depending on the final wiring | Interfaces the receiver with the motors |
| 7.4 V battery pack | 1 | Main vehicle power source |
| 5 V DC-DC buck converter (LM2596 or equivalent) | 1 | Regulated supply for compatible electronics |
| Jumper wires and connectors | As needed | Electrical connections |

The project hardware plan identifies an **L298N motor driver**, but the committed firmware does not yet define motor-control pins or implement its interface. Confirm the actual module, current capacity, and wiring before connecting the motors.

The code currently reads **digital buttons**, not analog joystick axes. A joystick-based control interface can be developed separately if selected for the final build.

## Repository Structure

```text
ESP-32-Car-ENG177/
├── controller/
│   └── controller.ino   # Transmitter firmware for either controller
├── receiver/
│   └── receiver.ino     # ESP-NOW receiver and message queue
├── LICENSE              # MIT License
└── README.md            # Project documentation
```

Both controller boards use the same sketch, but **each must be flashed with a different NODE_ID**.

## Wireless Communication

The current protocol uses one-byte unsigned integer commands (`uint8_t`) sent to the ESP-NOW broadcast MAC address:

```text
FF:FF:FF:FF:FF:FF
```

| Setting | Value | Defined in |
| --- | --- | --- |
| Wi-Fi operating mode | Station (`WIFI_STA`) | Both sketches |
| Wi-Fi channel | `6` | Both sketches |
| Broadcast destination | `FF:FF:FF:FF:FF:FF` | Controller |
| Nominal transmission interval | `15 ms` | Controller |
| Serial baud rate | `115200` | Both sketches |
| Receive queue capacity | `200` messages | Receiver |
| Configured communication timeout | `5000 ms` | Receiver |

The transmission interval is the intended software pacing, **not a guaranteed packet-delivery rate**. ESP-NOW broadcast packets are not application-level acknowledgments; a successful return from `esp_now_send()` does not prove that the receiver processed the command.

### Command Mapping

The controller sketch computes its command from `NODE_ID` and the two button states.

| Controller | `NODE_ID` | Command byte | Meaning |
| --- | ---: | ---: | --- |
| Throttle | 1 | `0` | Neither button pressed |
| Throttle | 1 | `1` | Button 1 pressed — intended forward |
| Throttle | 1 | `2` | Button 2 pressed — intended reverse |
| Steering | 2 | `3` | Neither button pressed |
| Steering | 2 | `4` | Button 1 pressed — intended left |
| Steering | 2 | `5` | Button 2 pressed — intended right |

The direction labels describe the **planned vehicle behavior**. At present, the receiver prints the commands but does not drive motors.

If both buttons on one controller are held simultaneously, **Button 1 takes priority** because the firmware checks Button 1 first.

### How the Receiver Processes Packets

1. The ESP-NOW callback accepts packets containing exactly one byte.
2. The callback records the command and source MAC address in a `ReceivedMessage` structure.
3. The callback attempts to enqueue the structure without blocking.
4. The main loop dequeues messages and prints the received command and sender MAC address through the serial monitor.

The code does not yet associate commands with authenticated controller identities, combine current throttle and steering states, or issue motor-driver outputs.

## Controller Wiring

The current `controller/controller.ino` sketch uses **active-low inputs** with internal pull-up resistors.

| Connection | ESP32 pin | Behavior |
| --- | --- | --- |
| Button 1 | GPIO 27 | Press connects GPIO 27 to GND |
| Button 2 | GPIO 26 | Press connects GPIO 26 to GND |
| Common return | GND | Shared ground for both button circuits |

Wire each button between its GPIO input and GND. No external pull-up resistor is required for the current `INPUT_PULLUP` configuration.

**Important:** The vehicle-side motor-driver pinout has not been defined in this repository. Do not assume that the controller GPIO assignments are motor-control pins.

## Getting Started

### 1. Prerequisites

- Three compatible ESP32 development boards
- USB cables for programming and serial monitoring
- Arduino IDE with the Espressif ESP32 board package installed
- A board-package version supporting the `esp_now_recv_info_t` callback interface used by `receiver.ino`
- Two wired button-input controllers for communication testing

These sketches use the Arduino ESP32 framework and ESP-NOW libraries supplied with the ESP32 board package. No additional third-party Arduino library is referenced by the current source files.

### 2. Clone the Repository

```bash
git clone https://github.com/MikewangBJ101ID/ESP-32-Car-ENG177.git
cd ESP-32-Car-ENG177
```

### 3. Flash Controller 1

1. Open `controller/controller.ino` in Arduino IDE.
2. Keep `#define NODE_ID 1`.
3. Select the correct ESP32 board and serial port.
4. Compile and upload the sketch.
5. Open the Serial Monitor at **115200 baud**.

### 4. Flash Controller 2

1. Open the same `controller/controller.ino` sketch.
2. Change `#define NODE_ID 1` to `#define NODE_ID 2`.
3. Select the second controller's board and serial port.
4. Compile and upload the sketch.

Do not leave both controllers set to `NODE_ID 1`; their command ranges would overlap.

### 5. Flash the Vehicle Receiver

1. Open `receiver/receiver.ino` in Arduino IDE.
2. Select the vehicle ESP32 board and serial port.
3. Compile and upload the sketch.
4. Open its Serial Monitor at **115200 baud**.

All three ESP32 devices must operate on the same Wi-Fi channel, currently **channel 6**.

### 6. Run a Communication Test

Power the receiver and both controllers, then press the buttons on each controller.

The transmitter's Serial Monitor should show messages such as:

```text
Board ID: 1
ESP-NOW ready
Sent Message: 1
```

The receiver's Serial Monitor should show messages in this format:

```text
ESP-NOW receiver ready
Received ID: 1 from XX:XX:XX:XX:XX:XX
```

The example MAC address is a placeholder. Verify that the receiver observes the expected command IDs **0–5** and that both controller boards are distinguishable by their MAC addresses.

**This is a wireless communication test only; uploading these sketches will not make the car drive.**

## Planned Vehicle Control

The intended vehicle algorithm treats throttle (`T`) and steering (`S`) as separate inputs and uses differential drive to calculate left- and right-side motor commands:

```text
left_output  = clamp(T + S, -1, 1)
right_output = clamp(T - S, -1, 1)
```

For the present digital command scheme, the firmware will need to define discrete values for the throttle and steering directions, retain the latest state from **each** controller, and convert the resulting outputs into motor direction and PWM signals.

This algorithm is a **design proposal**, not functionality in the current `receiver.ino`.

## Safety and Power Considerations

- **Keep the wheels off the ground during initial motor tests.**
- Never apply the 7.4 V battery voltage directly to ESP32 GPIO pins.
- Use a regulated supply compatible with the ESP32 board and motor-driver requirements.
- Confirm ground-reference connections where required by the circuit.
- Check the motor driver's continuous-current and startup-current ratings before connecting all four motors.
- Provide a practical way to disconnect vehicle power quickly.
- Implement and verify a stop-on-timeout function **before** allowing the car to drive unattended.

**Current failsafe limitation:** `receiver.ino` defines `FORCE_SHUTDOWN_TIME` as 5000 ms, but its timeout branch contains only comments. It does **not** currently disable motor outputs. The timer is also global rather than independently tracking each controller.

## Known Limitations

- The receiver has **no implemented motor-control logic**.
- The configured 5-second timeout does **not** actuate a motor shutdown.
- Controller messages are broadcasts with **no encryption or application-level sender authentication**.
- The receiver does not yet filter unexpected command values or verify the source MAC address.
- The receive callback may drop packets if the queue is full; these drops are not currently logged.
- The receiver handles packets as events rather than maintaining separate, persistent throttle and steering states.
- Hardware integration and full vehicle testing have not yet been documented.

## Development Roadmap

- [x] Establish the GitHub project repository.
- [x] Implement button-based command generation for two controllers.
- [x] Configure ESP-NOW broadcast transmission.
- [x] Implement ESP-NOW reception and FreeRTOS-based message queuing.
- [x] Add basic serial diagnostics.
- [ ] Verify reliable communication using all three physical ESP32 boards.
- [ ] Confirm the motor driver, power distribution, and complete wiring diagram.
- [ ] Implement command validation and per-controller state tracking.
- [ ] Implement differential steering and PWM motor control.
- [ ] Implement a verified, fail-safe motor stop on lost communication.
- [ ] Test forward/reverse, left/right, combined steering, and recovery after signal loss.
- [ ] Document the final physical assembly, test results, and demonstration.

Completed checkboxes indicate features visible in the committed source code or repository, **not** successful validation on the assembled car.

## Contributing

This repository is the shared source of truth for the ENG 177 project. Team members should:

1. Pull the latest changes before editing.
2. Make focused changes to the relevant sketch.
3. Explain significant code or hardware changes in commits or pull requests.
4. Test changes on the appropriate ESP32 board when hardware is available.
5. Update this README whenever command definitions, wiring, or functionality changes.

Please avoid using chat messages as the only record of code revisions.

## License

This repository is licensed under the **MIT License**. See [LICENSE](LICENSE) for the full text.

---

**Course:** ENG 177 — University of Illinois Urbana-Champaign  
**Project:** DUODRIVE RC Car  
**Repository:** [ESP-32-Car-ENG177](https://github.com/MikewangBJ101ID/ESP-32-Car-ENG177)
