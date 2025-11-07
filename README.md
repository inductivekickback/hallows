
# Hallows
<img src="https://github.com/user-attachments/assets/51678149-e550-4142-bbdb-fc4d3716662a" align="right" width="300" style="margin-right: 15px; margin-bottom: 15px;">

<p>The release of [excellent MicroPython tooling](https://education.lego.com/en-us/product-resources/mindstorms-ev3/teacher-resources/python-for-ev3/) for the [Lego Mindstorms EV3](https://en.wikipedia.org/wiki/Lego_Mindstorms_EV3) was clearly the peak of one of my favorite toys, followed a few years later by Lego discontuining the line completely. Along with a [Visual Studio Code plugin](https://marketplace.visualstudio.com/items?itemName=lego-education.ev3-micropython) that allows for interactive debugging the brick also gained proper support for [UART](https://en.wikipedia.org/wiki/Universal_asynchronous_receiver-transmitter). This project demonstrates a relatively simple way to expand the capabilities of EV3 using the popular [Nordic nRF52840 SoC](https://docs.nordicsemi.com/category/nrf52840-category).</p>

## Interface
The dongle




<p align="center">
<img src="https://github.com/user-attachments/assets/6ddd7d79-e573-403b-bebd-4bd6e162c690" width="200">
</p>


![Image](https://github.com/user-attachments/assets/c12f04bf-3a4c-46c1-946e-bec4b67d823e)




### BLE Interface

Candy dispenser:
    [W] Dispense from index '1', '2', '3', or '4'


## UART interface to Lego Mindstorms ev3

Communicate through a level shifter using 115200 baud rate.

Serialized messages are received over the UART from some kind of peer:
    Messages have a fixed len of three bytes.
    The timeout is 1 second.
    Commands are repeated if a timeout happens.
    All messages start with a byte that has the leading bit set.

DIRECTION:  DD, where command is 10 and response is 11
OPCODE:     DDXX
IMMEDIATE:  YYYY
DATA:       0ZZZ ZZZZ 0ZZZ ZZZZ

OPCODES:
DD00 YYYY   STATUS, DATA bytes ignored, where YYYY:
                0000 - SYNC command, aka keep alive
DD01 YYYY   DISPENSE from machine YYYY, DATA bytes ignored
