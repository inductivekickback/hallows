

<p align="center">
<img src="https://github.com/user-attachments/assets/51678149-e550-4142-bbdb-fc4d3716662a" width="400">
</p>

<p align="center">
<img src="https://github.com/user-attachments/assets/6ddd7d79-e573-403b-bebd-4bd6e162c690" width="200">
</p>


![Image](https://github.com/user-attachments/assets/c12f04bf-3a4c-46c1-946e-bec4b67d823e)




## BLE Interface

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
