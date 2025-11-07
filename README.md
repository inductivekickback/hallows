# Hallows

- A BLE remote control that connects to a Lego NXT candy dispenser


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
