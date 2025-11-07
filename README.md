<img src="https://github.com/user-attachments/assets/51678149-e550-4142-bbdb-fc4d3716662a" align="right" width="400" style="margin-right: 15px; margin-bottom: 15px;">

<p>The release of the excellent <a href="https://education.lego.com/en-us/product-resources/mindstorms-ev3/teacher-resources/python-for-ev3/">MicroPython tooling</a> for the <a href="https://en.wikipedia.org/wiki/Lego_Mindstorms_EV3">Lego Mindstorms EV3</a> was clearly the peak of the evolution of one of my favorite toys, followed a few years later by Lego discontuining the line completely. Along with a <a href="https://marketplace.visualstudio.com/items?itemName=lego-education.ev3-micropython">Visual Studio Code plugin</a> that allows for interactive debugging the brick also gained proper support for <a href="https://en.wikipedia.org/wiki/Universal_asynchronous_receiver-transmitter">UART</a> communications. This project demonstrates a relatively simple way to expand the capabilities of EV3 using the popular <a href="https://docs.nordicsemi.com/category/nrf52840-category">Nordic nRF52840 SoC</a>.</p>
<p>The <a href="https://docs.nordicsemi.com/bundle/ug_nrf52840_dongle/page/UG/nrf52840_Dongle/intro.html">nRF52840 dongle</a> is inexpensive, breadboard-friendly, and its onboard regulator can run from the ~5V that is supplied to EV3 sensor ports. The nRF52840 will convert the "VBUS" voltage to <a href="https://docs.nordicsemi.com/bundle/ps_nrf52840/page/uicr.html#ariaid-title69">1.8V by default (max 3.3V)</a> so a level shifter is still required to translate between the Lego and Nordic serial ports. I keep a few of these little <a href="https://www.sparkfun.com/sparkfun-logic-level-converter-bi-directional.html">Sparkfun boards</a> for situations like this:</p>
<p align="center">
<img src="https://github.com/user-attachments/assets/6ddd7d79-e573-403b-bebd-4bd6e162c690" width="200">
</p>
<p>The WHITE wire from the EV3 is not used. Tying the RED and BLACK wires together forms GND. Then the 5V from the EV3 (GREEN wire) is fed into the dongle's "VBUS" pin and the the output from the dongle's "VDD OUT" is used to run the nRF52840 and its peripherals. The level shifter translates the UART RX (BLUE wire) and TX (YELLOW wire) between the two devices:</p><br>

![Image](https://github.com/user-attachments/assets/c12f04bf-3a4c-46c1-946e-bec4b67d823e)
<br><br>

### BLE Interface

A simple BLE service and client are used to exchange data between a <a href="https://inductivekickback.blogspot.com/2021/11/halloween-2021.html">remote control (central) and a candy machine (peripheral)</a>. The remote sends a single ASCII byte, ['1', '2', '3', '4'], to select the candy to be dispensed.

## UART interface to EV3

The nRF52840 communicates with the EV3 via two UART pins, through the level shifter, using 115200 8n1 baud rate.

Serialized messages are received by the EV3 over the UART:
 - Messages have a fixed len of one byte.
 - All messages start with a byte that has the leading bit set.

```
DIRECTION:  DD, where command is 10 and response is 11
OPCODE:     DDXX
IMMEDIATE:  YYYY

OPCODES:
DD00 YYYY   STATUS, where YYYY:
                0000 - SYNC command, aka keep alive
DD01 YYYY   DISPENSE, where YYYY:
                [0, 3] - Dispenser index
```
