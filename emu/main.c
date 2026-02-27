/*
 * main.c
 *
 * Tiny frontend for the Intel 4004 emulator
 * 
 * This file is intentionally small, dependency free, and easy to read
 * so it can serve as an example of embedding the emulator in other
 * projects. Error handling is straightforward for educational purposes.
 * 
 * It has a simple virtual IO device controller and a simple TTY
 * 
 * Usage:
 * emu <binary ROM filename>
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include "cpu.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

void trap_callback(struct cpu_state *state, enum cpu_trap trap)
{
	static char *trap_to_str[] = {
		"CPU_UNDEFINED_OPCODE",
		"CPU_STACK_OVERFLOW",
		"CPU_INVALID_ADDRESS_ROM",
		"CPU_INVALID_ADDRESS_RAM",
		"CPU_INVALID_ADDRESS_RAM_STATUS"
	};
	(void)state;
	fprintf(stderr, "trap_callback: %s, pc=%3X, op=%02X\n", trap_to_str[trap], state->address[state->pc]&0xfff, state->rom[state->address[state->pc]]);
	exit(1);
}

/* IO Devices list */

struct io_device {
	u8 (*read)(void);
	void (*write)(u8 value);
};

enum {
	TTY_WAIT = 0,
	TTY_PRINT = 1
};

u8 tty_state = TTY_WAIT;
u8 tty_char = 0;

u8 tty_read(void)
{
	return 0;
}

void tty_write(u8 value)
{
	static u8 nibbles = 0;
	switch (tty_state) {
	case TTY_WAIT:
		if (value == 1) {
			tty_state = TTY_PRINT;
			tty_char = 0;
		}
		break;
	case TTY_PRINT:
		tty_char = (tty_char << 4) | (value & 0xf);
		nibbles++;
		
		if (nibbles == 2) {
			if (tty_char == 0) {
				tty_state = TTY_WAIT;
				tty_char = 0;
				nibbles = 0;
				return;
			}

			fprintf(stdout, "%c", tty_char);
			tty_char = 0;
			nibbles = 0;
		}

		break;
	}
}

/* Simple basic virtual IO controller */

struct io_device devices[16] = { {tty_read, tty_write} };
u8 port;
u8 ram_port;

void ram_write(int bank, int chip, int value)
{
	(void)bank;
	(void)chip;
	ram_port = value & 15;
}

void rom_write(int value)
{
	port = (value & 15);

	devices[port].write(ram_port);
}

int rom_read(void)
{
	return devices[port].read() & 15;
}

void read_rom(char *filename, u8 *rom, u16 size)
{
	FILE *file;
	size_t length;

	file = fopen(filename, "rb");
	if (!file) {
		fprintf(stderr, "Couldn't open ROM %s: %s\n", filename,
			strerror(errno));
		exit(1);
	}

	fseek(file, 0, SEEK_END);
	length = ftell(file);
	length = (length > size) ? size : length;
	rewind(file);
	fread(rom, 1, length, file);
	fclose(file);
}

int main(int argc, char **argv)
{
	struct cpu_state cpu;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <binary ROM filename>\n", argv[0]);
		exit(1);
	}

	memset(&cpu, 0, sizeof(struct cpu_state));

	cpu.trap_callback = trap_callback;
	cpu.ram_out = ram_write;
	cpu.rom_write = rom_write;
	cpu.rom_read = rom_read;

	cpu.ram_size = 256*4;
	cpu.rom_size = 256*4;
	cpu.ram_status_size = 64;

	cpu.ram = malloc(cpu.ram_size);
	cpu.rom = malloc(cpu.rom_size);
	cpu.ram_status = malloc(cpu.ram_status_size);

	memset(cpu.rom, 0, cpu.rom_size);

	read_rom(argv[1], cpu.rom, cpu.rom_size);

	if (cpu_init(&cpu) == -1) {
		fprintf(stderr, "cpu_init failed\n");
		exit(1);
	}
	cpu_reset(&cpu);

	while (1) {
		cpu_step(&cpu);
	}

	return 0;
}
