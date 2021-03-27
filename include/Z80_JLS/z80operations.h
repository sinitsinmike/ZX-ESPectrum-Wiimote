///////////////////////////////////////////////////////////////////////////////
//
// z80cpp - Z80 emulator core
//
// Copyright (c) 2017, 2018, 2019, 2020 jsanchezv - https://github.com/jsanchezv
//
// Heretic optimizations and minor adaptations
// Copyright (c) 2021 dcrespo3d - https://github.com/dcrespo3d
//

#ifndef Z80OPERATIONS_H
#define Z80OPERATIONS_H

#include <stdint.h>

/* Read opcode from RAM */
uint8_t z80ops_fetchOpcode(uint16_t address);

/* Read/Write byte from/to RAM */
uint8_t z80ops_peek8(uint16_t address);
void z80ops_poke8(uint16_t address, uint8_t value);

/* Read/Write word from/to RAM */
uint16_t z80ops_peek16(uint16_t adddress);
void z80ops_poke16(uint16_t address, RegisterPair word);

/* In/Out byte from/to IO Bus */
uint8_t z80ops_inPort(uint16_t port);
void z80ops_outPort(uint16_t port, uint8_t value);

/* Put an address on bus lasting 'tstates' cycles */
void z80ops_addressOnBus(uint16_t address, int32_t wstates);

/* Clocks needed for processing INT and NMI */
void z80ops_interruptHandlingTime(int32_t wstates);

/* Callback to know when the INT signal is active */
bool z80ops_isActiveINT(void);


#endif // Z80OPERATIONS_H
