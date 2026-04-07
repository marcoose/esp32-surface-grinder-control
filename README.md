# ESP32 Surface Grinder CNC Motion Controller

This project implements a 2-axis CNC motion controller for a surface grinder using a single ESP32-3248S035C board.
The board's integrated touchscreen and GPIO pins handle user interface and motor control (via step and direction outputs
to TMC2209 drivers or equivalent). The manual SG-612 grinder is retrofit with steppers for the table and cross-slide,
while the Z-axis is left for operator manual adjustment.

## Features

- **Touchscreen Interface**
  - X (table sweep) max travel
  - Y (cross-slide) max travel
  - Y step-over increment
  - X feed rate
- **Standalone Operation**: Runs independently on the ESP32 board.

## Hardware Requirements

- ESP32-3248S035C board (3.5" capacitive touchscreen)
- Stepper motors and drivers (e.g., TMC2209, A4988) for axes
- Belts and pulleys to connect to handwheels
- Power supply (12v for motors, with 12-5v stepdown for LCD)

## Board Modifications

As the ESP32-3248S035C has very limited GPIO, the TF card socket and pull-up resistor bank are desoldered and GPIO
pins 18 and 19 are used for one of the axes, along with the 21 and 21 already available via the existing connector for I2C.

## Software

- Built with PlatformIO and Arduino framework
- Libraries: LVGL for display, AccelStepper for motor control

## Installation

1. Clone the repository.
2. Open in PlatformIO.
3. Configure `config.h` for your setup.
4. Upload to ESP32-3248S035C.

## Usage

- Power on the board.
- Use touchscreen to set grinding parameters.
- Start operations via GUI controls.

## License

MIT License.
