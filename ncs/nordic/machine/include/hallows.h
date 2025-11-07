/** @file
 *  @brief Serial protocol for controlling a candy machine
 */

#ifndef HALLOWS_H_
#define HALLOWS_H_

#ifdef __cplusplus
extern "C" {
#endif

#define CMD_LEN_BYTES   1
#define CMD_BIT         0x80
#define RSP_BIT         0x40
#define OPCODE_MASK     0x30
#define IMMEDIATE_MASK  0x0F

enum hallows_opcode {
    OPCODE_STATUS   = 0x00,
    OPCODE_DISPENSE = 0x10,
};

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* HALLOWS_H_ */
