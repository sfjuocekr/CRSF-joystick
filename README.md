# CRSF-joystick

A very basic Teensy 3.1/3.2 HID joystick for the CrossFire protocol, probably works with other microcontrollers as well.

Be sure to set the baud rate of your receiver to 115200, you can do this with ExpressLRS by setting RCVR_UART_BAUD=115200.

By default it just reports 5V and 100% battery using telemetry to stop my radio from yelling at me :)

SBUS = Serial1 (pin 0)
CRSF = Serial2 (rx pin 9, tx pin 10)

Channels 1, 2, 3 and 4 are axis; the rest is assumed to be three position switches.
Having separate buttons makes setting up simulator functions a breeze!
I have added a few hacks to make different simulators compatible with this joystick.

Also works in BetaFlight passthrough to flash your receiver, be sure to use a compatible baud rate for your device!

# Credits:

 * SBUS from bolderflight: https://github.com/bolderflight/SBUS
 * CrsfSerial from CapnBry: https://github.com/CapnBry/CRServoF/tree/master/lib/CrsfSerial
