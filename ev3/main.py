#!/usr/bin/env pybricks-micropython
"""
Dispense candy using a Lego Mindstorms ev3
------------------------------------------
Serialized messages are received over the UART from some kind of peer:
    Messages have a fixed len of two bytes.
    The timeout is 1 second.
    Commands are repeated if a timeout happens.
    All messages start with a byte that has the leading bit set.

DIRECTION:  DD, where command is 10 and response is 11
OPCODE:     DDXX
IMMEDIATE:  YYYY

OPCODES:
DD00 YYYY   STATUS, where YYYY:
                0000 - SYNC command, aka keep alive
DD01 YYYY   DISPENSE, where YYYY:
                [0, 3] - Dispenser index
"""
from pybricks.hubs import EV3Brick
from pybricks.parameters import Port, Button, Color
from pybricks.iodevices import UARTDevice
from pybricks.ev3devices import Motor

CMD_LEN_BYTES = 1
CMD_BIT_MASK = 0x80
RSP_BIT_MASK = 0x40
OPCODE_MASK = 0x30
IMMEDIATE_MASK = 0x0F

OPCODE_STATUS = 0x00
OPCODE_DISPENSE = 0x01

PENDING_INDEX = 0
MOTOR_INDEX = 1
PARAMS_INDEX = 2
BUTTON_INDEX = 3
WAIT_FOR_RELEASE_INDEX = 4
DESC_INDEX = 5

ev3 = None
ser = None
buff = []

DISPENSERS = (
    [0, Motor(Port.A), (360, 89), Button.UP, False, "Starburst"],
    [0, Motor(Port.B), (360, 89), Button.RIGHT, False, "MilkyWay"],
    [0, Motor(Port.C), (5050, 540), Button.DOWN, False, "M&Ms"],
    [0, None, (300, 450), Button.LEFT, False, "Sour Patch Kids"]
)

def _execute_status(immediate):
    if immediate == 0x00:
        print("STATUS - SYNC")
    else:
        print("ERROR: STATUS - " + str(immediate))

def _execute_dispense(immediate):
    if immediate < len(DISPENSERS):
        print("DISPENSE - " + str(immediate) + " (" + DISPENSERS[immediate][DESC_INDEX] + ")")
        DISPENSERS[immediate][PENDING_INDEX] += 1
    else:
        print("DISPENSE: Invalid dispenser(" + str(immediate) + ")")

def _respond():
    opcode = buff[0] | RSP_BIT_MASK
    _send_bytes([opcode] + buff[1:CMD_LEN_BYTES])

def _execute_cmd():
    immediate = buff[0] & IMMEDIATE_MASK
    opcode = (buff[0] & OPCODE_MASK) >> 4
    if opcode == OPCODE_STATUS:
        _execute_status(immediate)
    elif opcode == OPCODE_DISPENSE:
        _execute_dispense(immediate)
    else:
        # Unknown opcode. Reply to keep channel moving.
        pass
    _respond()

def _process_buff():
    global buff
    while buff:
        if buff[0] & CMD_BIT_MASK:
            print("Received cmd: %d", buff[0])
            break
        else:
            buff = buff[1:]

    if len(buff) < CMD_LEN_BYTES:
        return

    # Ignore responses.
    if not buff[0] & RSP_BIT_MASK:
        ev3.light.on(Color.ORANGE)
        _execute_cmd()

    buff = buff[CMD_LEN_BYTES:]
    _process_buff()

def _send_bytes(msg):
    ev3.speaker.beep()
    ev3.light.on(Color.GREEN)
    ser.write(bytes(msg))

def main():
    """
    Receive serialized commands from the UART while also responding
    to button presses.
    """
    global ev3, ser

    ev3 = EV3Brick()
    ev3.light.on(Color.RED) # GREEN, ORANGE, or RED
    ser = UARTDevice(Port.S1, baudrate=115200, timeout=100)
    ev3.speaker.beep()

    while True:
        data = ser.read_all()
        if data:
            print(data)
            buff.extend(data)
            _process_buff()

        pressed = ev3.buttons.pressed()

        for i in range(0, len(DISPENSERS)):
            if DISPENSERS[i][BUTTON_INDEX] in pressed:
                if not DISPENSERS[i][WAIT_FOR_RELEASE_INDEX]:
                    DISPENSERS[i][WAIT_FOR_RELEASE_INDEX] = True
                    DISPENSERS[i][PENDING_INDEX] += 1
                    print(DISPENSERS[i][DESC_INDEX] + " button pressed.")
            elif DISPENSERS[i][WAIT_FOR_RELEASE_INDEX]:
                DISPENSERS[i][WAIT_FOR_RELEASE_INDEX] = False

            motor = DISPENSERS[i][MOTOR_INDEX]
            if motor and motor.control.done():
                if DISPENSERS[i][PENDING_INDEX]:
                    motor.run_angle(*DISPENSERS[i][PARAMS_INDEX])
                    DISPENSERS[i][PENDING_INDEX] -= 1

if __name__ == "__main__":
    main()
