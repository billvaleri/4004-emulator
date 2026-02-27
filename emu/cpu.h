#ifndef I4004_CPU_H
#define I4004_CPU_H

#include <stdint.h>

enum cpu_trap {
	CPU_UNDEFINED_OPCODE = 0,
	CPU_STACK_OVERFLOW,
	CPU_INVALID_ADDRESS_ROM,
	CPU_INVALID_ADDRESS_RAM,
	CPU_INVALID_ADDRESS_RAM_STATUS
};

/* CPU state */
struct cpu_state {
	/* Cycles counter */
	unsigned int cycles;
	/* General registers */
	uint8_t regs[16];
	/* Accumulator */
	uint8_t acc;
	/* Flags */
	uint8_t carry;
	uint8_t test;
	/* Address register */
	uint16_t address[4];
	uint8_t pc;
	/* ROM */
	uint16_t rom_size;
	uint8_t *rom;
	/* RAM */
	uint8_t ram_bank;
	uint8_t ram_address;
	uint16_t ram_size;
	uint8_t *ram;
	uint16_t ram_status_size;
	uint8_t *ram_status;
	/* All callbacks are optional and may be NULL. */
	/* trap_callback is not part of i4004 hardware. It is used by the
	   emulator to signal exceptional or undefined conditions. */
	void (*trap_callback)(struct cpu_state *state, enum cpu_trap trap);
	void (*ram_out)(int bank, int chip, int value);
	void (*rom_write)(int value);
	int (*rom_read)(void);
};

/* Executes a single processor step. */
void cpu_step(struct cpu_state *cpu);
/* Resets the CPU state, except for ROM. */
void cpu_reset(struct cpu_state *cpu);
/* Returns 0 on success. Otherwise returns -1 (not enough memory). */
/* Note: The emulator does not allocate or free memory.
   All memory pointers must remain valid for the lifetime of cpu_state. */
int cpu_init(struct cpu_state *cpu);

#endif /* I4004_CPU_H */
