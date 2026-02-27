#include "cpu.h"

typedef uint8_t u8;
typedef uint16_t u16;

/* Read one byte from ROM
   address - byte offset */
u8 cpu_read_rom8(struct cpu_state *cpu, u16 address)
{
	if (address >= cpu->rom_size) {
		if (cpu->trap_callback)
			cpu->trap_callback(cpu, CPU_INVALID_ADDRESS_ROM);
		/* According to documentation, the i4004 returns 0xFF when reading
		   from a non-existent ROM address */
		return 0xff;
	}

	return cpu->rom[address];
}

/* Read one byte from RAM
   address - byte offset */
u8 cpu_read_ram8(struct cpu_state *cpu, u16 address)
{
	if (address >= cpu->ram_size) {
		if (cpu->trap_callback)
			cpu->trap_callback(cpu, CPU_INVALID_ADDRESS_RAM);
		return 0xff;
	}

	return cpu->ram[address];
}

/* Write one byte to RAM
   address - byte offset
   value - byte value */
void cpu_write_ram8(struct cpu_state *cpu, u16 address, u8 value)
{
	if (address >= cpu->ram_size) {
		if (cpu->trap_callback)
			cpu->trap_callback(cpu, CPU_INVALID_ADDRESS_RAM);
		return;
	}
	cpu->ram[address] = value;
}

/* Read one byte from status RAM
   address - byte offset */
u8 cpu_read_ram_status8(struct cpu_state *cpu, u16 address)
{
	if (address >= cpu->ram_status_size) {
		if (cpu->trap_callback)
			cpu->trap_callback(cpu, CPU_INVALID_ADDRESS_RAM_STATUS);
		return 0xff;
	}

	return cpu->ram_status[address];
}

/* Write one byte to status RAM
   address - byte offset
   value - byte value */
void cpu_write_ram_status8(struct cpu_state *cpu, u16 address, u8 value)
{
	if (address >= cpu->ram_status_size) {
		if (cpu->trap_callback)
			cpu->trap_callback(cpu, CPU_INVALID_ADDRESS_RAM_STATUS);
		return;
	}
	cpu->ram_status[address] = value;
}

/* Read one nibble from RAM
   address - nibble offset */
u8 cpu_read_ram4(struct cpu_state *cpu, u16 address)
{
	if (address & 1) {
		return cpu_read_ram8(cpu, address >> 1) & 0x0f;
	} else {
		return (cpu_read_ram8(cpu, address >> 1) & 0xf0) >> 4;
	}
}

/* Write one nibble to RAM
   address - nibble offset
   value - 4-bit value */
void cpu_write_ram4(struct cpu_state *cpu, u16 address, u8 value)
{
	u8 old = cpu_read_ram8(cpu, address >> 1);

	if (address & 1) {
		cpu_write_ram8(cpu, address >> 1, (old & 0xf0) | value);
	} else {
		cpu_write_ram8(cpu, address >> 1, (old & 0x0f) | (value << 4));
	}
}

/* Read one nibble from status RAM
   address - nibble offset */
u8 cpu_read_ram_status4(struct cpu_state *cpu, u16 address)
{
	if (address & 1) {
		return cpu_read_ram_status8(cpu, address >> 1) & 0x0f;
	} else {
		return (cpu_read_ram_status8(cpu, address >> 1) & 0xf0) >> 4;
	}
}

/* Write one nibble to status RAM
   address - nibble offset
   value - 4-bit value */
void cpu_write_ram_status4(struct cpu_state *cpu, u16 address, u8 value)
{
	u8 old = cpu_read_ram_status8(cpu, address >> 1);

	if (address & 1) {
		cpu_write_ram_status8(cpu, address >> 1, (old & 0xf0) | value);
	} else {
		cpu_write_ram_status8(cpu, address >> 1, (old & 0x0f) | (value << 4));
	}
}

void opcode_jcn(struct cpu_state *cpu, u8 opcode)
{
	u8 condition = opcode & 0x0f;
	u8 invert = (condition & 0x08) >> 3;
	u16 address = cpu->address[cpu->pc] & 0xf00;
	address |= cpu_read_rom8(cpu, cpu->address[cpu->pc]++);

	if (condition & 0x04) {
		if ((!cpu->acc) ^ invert)
			cpu->address[cpu->pc] = address;
	}
	if (condition & 0x02) {
		if ((cpu->carry) ^ invert)
			cpu->address[cpu->pc] = address;
	}
	if (condition & 0x01) {
		if ((cpu->test) ^ invert)
			cpu->address[cpu->pc] = address;
	}
}

void opcode_fim(struct cpu_state *cpu, u8 opcode)
{
	u8 pair = (opcode & 0x0e) >> 1;
	u8 value = cpu_read_rom8(cpu, cpu->address[cpu->pc]++);

	cpu->regs[2 * pair + 1] = value & 0x0f;
	cpu->regs[2 * pair] = (value >> 4) & 0x0f;
}

void opcode_src(struct cpu_state *cpu, u8 opcode)
{
	u8 value;
	u8 pair;
	
	pair = (opcode & 0x0e) >> 1;

	value = (cpu->regs[2 * pair + 1] & 0x0f);
	value |= (cpu->regs[2 * pair] & 0x0f) << 4;

	cpu->ram_address = value;
}

void opcode_fin(struct cpu_state *cpu, u8 opcode)
{
	u8 pair = (opcode >> 1) & 7;
	u8 address;
	u8 value;
	
	address = (cpu->regs[1] & 0x0f);
	address |= (cpu->regs[0] & 0x0f) << 4;
	address |= cpu->address[cpu->pc] & 0xf00;

	value = cpu_read_rom8(cpu, address&0xfff);

	cpu->regs[2 * pair + 1] = value & 0x0f;
	cpu->regs[2 * pair] = (value >> 4) & 0x0f;
}

void opcode_jin(struct cpu_state *cpu, u8 opcode)
{
	u8 pair = (opcode & 0x0f) >> 1;
	u8 value = (cpu->regs[2 * pair] << 4) |
				  cpu->regs[2 * pair + 1];

	cpu->address[cpu->pc] = (cpu->address[cpu->pc] & 0xf00) | value;
}

void opcode_jun(struct cpu_state *cpu, u8 opcode)
{
	cpu->address[cpu->pc] = ((opcode&0x0f)<<8) |
		cpu_read_rom8(cpu, cpu->address[cpu->pc]);
}

void opcode_jms(struct cpu_state *cpu, u8 opcode)
{
	u16 address;

	address = (opcode & 0x0f) << 8;
	address |= cpu_read_rom8(cpu, cpu->address[cpu->pc]++);

	cpu->pc++;
	if (cpu->pc > 3) {
		if (cpu->trap_callback) {
			cpu->trap_callback(cpu, CPU_STACK_OVERFLOW);
		}
		/* i4004 has a 2-bit program counter(stack pointer), overflow
		   wraps naturally */
		cpu->pc &= 3;
	}

	cpu->address[cpu->pc] = address;
}

void opcode_inc(struct cpu_state *cpu, u8 opcode)
{
	cpu->regs[opcode & 0x0f] = (cpu->regs[opcode & 0x0f] + 1) & 0xf;
}

void opcode_isz(struct cpu_state *cpu, u8 opcode)
{
	u8 jmp = cpu_read_rom8(cpu, cpu->address[cpu->pc]++);

	cpu->regs[opcode&0xf] = (cpu->regs[opcode&0xf]+1)&0x0f;

	if (cpu->regs[opcode&0xf]) {
		cpu->address[cpu->pc] = (cpu->address[cpu->pc]&0xf00) | jmp;
	}
}

void opcode_add(struct cpu_state *cpu, u8 opcode)
{
	cpu->acc = cpu->acc + cpu->regs[opcode & 0x0f] + cpu->carry;
	cpu->carry = (cpu->acc >> 4) & 0x01;
	cpu->acc &= 0x0f;
}

void opcode_sub(struct cpu_state *cpu, u8 opcode)
{
	cpu->acc = cpu->acc + (~cpu->regs[opcode&0x0f]&0x0f) + (~cpu->carry&1);
	cpu->carry = 0;

	if (cpu->acc & 0xf0) {
		cpu->acc &= 0x0f;
		cpu->carry = 1;
	}
}

void opcode_ld(struct cpu_state *cpu, u8 opcode)
{
	cpu->acc = cpu->regs[opcode & 0x0f];
}

void opcode_xch(struct cpu_state *cpu, u8 opcode)
{
	u8 old = cpu->regs[opcode & 0x0f];

	cpu->regs[opcode & 0x0f] = cpu->acc;
	cpu->acc = old;
}

void opcode_bbl(struct cpu_state *cpu, u8 opcode)
{
	(void)opcode;
	cpu->pc--;
	if (cpu->pc > 3) {
		if (cpu->trap_callback) {
			cpu->trap_callback(cpu, CPU_STACK_OVERFLOW);
		}
		cpu->pc &= 3;
	}

	cpu->acc = opcode & 0x0f;
}

void opcode_ldm(struct cpu_state *cpu, u8 opcode)
{
	(void)opcode;
	cpu->acc = (opcode & 0x0f);
}

void opcode_wrm(struct cpu_state *cpu, u8 opcode)
{
	(void)opcode;
	u16 offset = ((cpu->ram_bank << 8) | cpu->ram_address);

	cpu_write_ram4(cpu, offset, cpu->acc);
}

void opcode_wmp(struct cpu_state *cpu, u8 opcode)
{
	(void)opcode;
	if (cpu->ram_out) {
		cpu->ram_out(cpu->ram_bank, cpu->ram_address / 64, cpu->acc);
	}
}

void opcode_wrr(struct cpu_state *cpu, u8 opcode)
{
	(void)opcode;
	if (cpu->rom_write) {
		cpu->rom_write(cpu->acc);
	}
}

void opcode_wrX(struct cpu_state *cpu, u8 opcode)
{
	u8 status = opcode & 3;
	u8 chip = (cpu->ram_address & 0x30) >> 4;
	u16 offset = (cpu->ram_bank << 4) | (chip << 2) | status;

	cpu_write_ram_status4(cpu, offset, cpu->acc);
}

void opcode_sbm(struct cpu_state *cpu, u8 opcode)
{
	(void)opcode;
	u16 offset = ((cpu->ram_bank << 8) | cpu->ram_address);
	u8 value = cpu_read_ram4(cpu, offset);

	cpu->acc = cpu->acc + (~value & 0x0f) + (~cpu->carry&1);

	cpu->carry = 0;
	if (cpu->acc & 0xf0) {
		cpu->acc &= 0x0f;
		cpu->carry = 1;
	}
}

void opcode_rdr(struct cpu_state *cpu, u8 opcode)
{
	(void)opcode;
	if (cpu->rom_read) {
		cpu->acc = cpu->rom_read();
	}
}

void opcode_adm(struct cpu_state *cpu, u8 opcode)
{
	(void)opcode;
	u16 offset = ((cpu->ram_bank << 8) | cpu->ram_address);
	u8 value = cpu_read_ram4(cpu, offset);

	cpu->acc = cpu->acc + value + cpu->carry;

	cpu->carry = 0;
	if (cpu->acc & 0xf0) {
		cpu->acc &= 0x0f;
		cpu->carry = 1;
	}
}

void opcode_rdX(struct cpu_state *cpu, u8 opcode)
{
	u8 status = opcode & 3;
	u8 chip = (cpu->ram_address & 0x30) >> 4;
	u16 offset = (cpu->ram_bank << 4) | (chip << 2) | status;

	cpu->acc = cpu_read_ram_status4(cpu, offset);
}

void opcode_rdm(struct cpu_state *cpu, u8 opcode)
{
	(void)opcode;
	u16 offset = ((cpu->ram_bank << 8) | cpu->ram_address);

	cpu->acc = cpu_read_ram4(cpu, offset);
}

void opcode_clb(struct cpu_state *cpu, u8 opcode)
{
	(void)opcode;
	cpu->acc = 0;
	cpu->carry = 0;
}

void opcode_clc(struct cpu_state *cpu, u8 opcode)
{
	(void)opcode;
	cpu->carry = 0;
}

void opcode_iac(struct cpu_state *cpu, u8 opcode)
{
	(void)opcode;
	cpu->acc++;
	cpu->carry = 0;

	if (cpu->acc & 0xf0) {
		cpu->acc &= 0x0f;
		cpu->carry = 1;
	}
}

void opcode_cmc(struct cpu_state *cpu, u8 opcode)
{
	(void)opcode;
	cpu->carry = !cpu->carry;
}

void opcode_cma(struct cpu_state *cpu, u8 opcode)
{
	(void)opcode;
	cpu->acc = (~cpu->acc) & 0x0f;
}

void opcode_ral(struct cpu_state *cpu, u8 opcode)
{
	(void)opcode;
	cpu->acc = (cpu->acc << 1) | cpu->carry;
	cpu->carry = 0;

	if (cpu->acc & 0xf0) {
		cpu->acc &= 0x0f;
		cpu->carry = 1;
	}
}

void opcode_rar(struct cpu_state *cpu, u8 opcode)
{
	(void)opcode;
	u8 old_acc = cpu->acc;

	cpu->acc = (cpu->acc >> 1) | (cpu->carry << 3);
	cpu->carry = old_acc & 1;
}

void opcode_tcc(struct cpu_state *cpu, u8 opcode)
{
	(void)opcode;
	cpu->acc = cpu->carry;
	cpu->carry = 0;
}

void opcode_dac(struct cpu_state *cpu, u8 opcode)
{
	(void)opcode;
	cpu->acc += 0x0f;
	cpu->carry = 0;

	if (cpu->acc & 0xf0) {
		cpu->acc &= 0x0f;
		cpu->carry = 1;
	}
}

void opcode_tcs(struct cpu_state *cpu, u8 opcode)
{
	(void)opcode;
	cpu->acc = 9 + cpu->carry;
	cpu->carry = 0;
}

void opcode_stc(struct cpu_state *cpu, u8 opcode)
{
	(void)opcode;
	cpu->carry = 1;
}

void opcode_daa(struct cpu_state *cpu, u8 opcode)
{
	(void)opcode;
	if (cpu->acc > 9 || cpu->carry) {
		cpu->acc += 6;
	}

	if (cpu->acc & 0xf0) {
		cpu->acc &= 0x0f;
		cpu->carry = 1;
	}
}

void opcode_kbp(struct cpu_state *cpu, u8 opcode)
{
	(void)opcode;
	if (cpu->acc == 0 || cpu->acc == 1 || cpu->acc == 2 || cpu->acc == 4 ||
		cpu->acc == 8) {
		return;
	}

	cpu->acc = 0xff;
}

void opcode_dcl(struct cpu_state *cpu, u8 opcode)
{
	(void)opcode;
	cpu->ram_bank = cpu->acc & 7;
#if 0
	switch (cpu->acc & 7) {
	case 0:
		cpu->ram_bank = 0;
		break;
	case 1:
		cpu->ram_bank = 1;
		break;
	case 2:
		cpu->ram_bank = 2;
		break;
	case 3:
		cpu->ram_bank = 4;
		break;
	case 4:
		cpu->ram_bank = 3;
		break;
	case 5:
		cpu->ram_bank = 5;
		break;
	case 6:
		cpu->ram_bank = 6;
		break;
	case 7:
		cpu->ram_bank = 7;
		break;
	}
#endif
}

void opcode_nop(struct cpu_state *cpu, u8 opcode)
{
	(void)cpu;
	(void)opcode;
}

void opcode_unknown(struct cpu_state *cpu, u8 opcode)
{
	(void)opcode;
	if (cpu->trap_callback)
		cpu->trap_callback(cpu, CPU_UNDEFINED_OPCODE);
}

void (*opcode_table[256])(struct cpu_state *, u8) = {
	/* 0x00 - 0x0F */
	opcode_nop,     /* 0x00 - nop */
	opcode_unknown, /* 0x01 */
	opcode_unknown, /* 0x02 */
	opcode_unknown, /* 0x03 */
	opcode_unknown, /* 0x04 */
	opcode_unknown, /* 0x05 */
	opcode_unknown, /* 0x06 */
	opcode_unknown, /* 0x07 */
	opcode_unknown, /* 0x08 */
	opcode_unknown, /* 0x09 */
	opcode_unknown, /* 0x0A */
	opcode_unknown, /* 0x0B */
	opcode_unknown, /* 0x0C */
	opcode_unknown, /* 0x0D */
	opcode_unknown, /* 0x0E */
	opcode_unknown, /* 0x0F */
	/* 0x10 - 0x1F */
	opcode_jcn,     /* 0x10 */
	opcode_jcn,     /* 0x11 */
	opcode_jcn,     /* 0x12 */
	opcode_jcn,     /* 0x13 */
	opcode_jcn,     /* 0x14 */
	opcode_jcn,     /* 0x15 */
	opcode_jcn,     /* 0x16 */
	opcode_jcn,     /* 0x17 */
	opcode_jcn,     /* 0x18 */
	opcode_jcn,     /* 0x19 */
	opcode_jcn,     /* 0x1A */
	opcode_jcn,     /* 0x1B */
	opcode_jcn,     /* 0x1C */
	opcode_jcn,     /* 0x1D */
	opcode_jcn,     /* 0x1E */
	opcode_jcn,     /* 0x1F */
	/* 0x20 - 0x2F */
	opcode_fim,     /* 0x20 - fim */
	opcode_src,     /* 0x21 - src */
	opcode_unknown, /* 0x22 */
	opcode_unknown, /* 0x23 */
	opcode_unknown, /* 0x24 */
	opcode_unknown, /* 0x25 */
	opcode_unknown, /* 0x26 */
	opcode_unknown, /* 0x27 */
	opcode_unknown, /* 0x28 */
	opcode_unknown, /* 0x29 */
	opcode_unknown, /* 0x2A */
	opcode_unknown, /* 0x2B */
	opcode_unknown, /* 0x2C */
	opcode_unknown, /* 0x2D */
	opcode_unknown, /* 0x2E */
	opcode_unknown, /* 0x2F */
	/* 0x30 - 0x3F */
	opcode_fin,     /* 0x30 - fin */
	opcode_jin,     /* 0x31 - jin */
	opcode_unknown, /* 0x32 */
	opcode_unknown, /* 0x33 */
	opcode_unknown, /* 0x34 */
	opcode_unknown, /* 0x35 */
	opcode_unknown, /* 0x36 */
	opcode_unknown, /* 0x37 */
	opcode_unknown, /* 0x38 */
	opcode_unknown, /* 0x39 */
	opcode_unknown, /* 0x3A */
	opcode_unknown, /* 0x3B */
	opcode_unknown, /* 0x3C */
	opcode_unknown, /* 0x3D */
	opcode_unknown, /* 0x3E */
	opcode_unknown, /* 0x3F */
	/* 0x40 - 0x4F */
	opcode_jun,     /* 0x40 */
	opcode_jun,     /* 0x41 */
	opcode_jun,     /* 0x42 */
	opcode_jun,     /* 0x43 */
	opcode_jun,     /* 0x44 */
	opcode_jun,     /* 0x45 */
	opcode_jun,     /* 0x46 */
	opcode_jun,     /* 0x47 */
	opcode_jun,     /* 0x48 */
	opcode_jun,     /* 0x49 */
	opcode_jun,     /* 0x4A */
	opcode_jun,     /* 0x4B */
	opcode_jun,     /* 0x4C */
	opcode_jun,     /* 0x4D */
	opcode_jun,     /* 0x4E */
	opcode_jun,     /* 0x4F */
	/* 0x50 - 0x5F */
	opcode_jms,     /* 0x50 */
	opcode_jms,     /* 0x51 */
	opcode_jms,     /* 0x52 */
	opcode_jms,     /* 0x53 */
	opcode_jms,     /* 0x54 */
	opcode_jms,     /* 0x55 */
	opcode_jms,     /* 0x56 */
	opcode_jms,     /* 0x57 */
	opcode_jms,     /* 0x58 */
	opcode_jms,     /* 0x59 */
	opcode_jms,     /* 0x5A */
	opcode_jms,     /* 0x5B */
	opcode_jms,     /* 0x5C */
	opcode_jms,     /* 0x5D */
	opcode_jms,     /* 0x5E */
	opcode_jms,     /* 0x5F */
	/* 0x60 - 0x6F */
	opcode_inc,     /* 0x60 */
	opcode_inc,     /* 0x61 */
	opcode_inc,     /* 0x62 */
	opcode_inc,     /* 0x63 */
	opcode_inc,     /* 0x64 */
	opcode_inc,     /* 0x65 */
	opcode_inc,     /* 0x66 */
	opcode_inc,     /* 0x67 */
	opcode_inc,     /* 0x68 */
	opcode_inc,     /* 0x69 */
	opcode_inc,     /* 0x6A */
	opcode_inc,     /* 0x6B */
	opcode_inc,     /* 0x6C */
	opcode_inc,     /* 0x6D */
	opcode_inc,     /* 0x6E */
	opcode_inc,     /* 0x6F */
	/* 0x70 - 0x7F */
	opcode_isz,     /* 0x70 */
	opcode_isz,     /* 0x71 */
	opcode_isz,     /* 0x72 */
	opcode_isz,     /* 0x73 */
	opcode_isz,     /* 0x74 */
	opcode_isz,     /* 0x75 */
	opcode_isz,     /* 0x76 */
	opcode_isz,     /* 0x77 */
	opcode_isz,     /* 0x78 */
	opcode_isz,     /* 0x79 */
	opcode_isz,     /* 0x7A */
	opcode_isz,     /* 0x7B */
	opcode_isz,     /* 0x7C */
	opcode_isz,     /* 0x7D */
	opcode_isz,     /* 0x7E */
	opcode_isz,     /* 0x7F */
	/* 0x80 - 0x8F */
	opcode_add,     /* 0x80 */
	opcode_add,     /* 0x81 */
	opcode_add,     /* 0x82 */
	opcode_add,     /* 0x83 */
	opcode_add,     /* 0x84 */
	opcode_add,     /* 0x85 */
	opcode_add,     /* 0x86 */
	opcode_add,     /* 0x87 */
	opcode_add,     /* 0x88 */
	opcode_add,     /* 0x89 */
	opcode_add,     /* 0x8A */
	opcode_add,     /* 0x8B */
	opcode_add,     /* 0x8C */
	opcode_add,     /* 0x8D */
	opcode_add,     /* 0x8E */
	opcode_add,     /* 0x8F */
	/* 0x90 - 0x9F */
	opcode_sub,     /* 0x90 */
	opcode_sub,     /* 0x91 */
	opcode_sub,     /* 0x92 */
	opcode_sub,     /* 0x93 */
	opcode_sub,     /* 0x94 */
	opcode_sub,     /* 0x95 */
	opcode_sub,     /* 0x96 */
	opcode_sub,     /* 0x97 */
	opcode_sub,     /* 0x98 */
	opcode_sub,     /* 0x99 */
	opcode_sub,     /* 0x9A */
	opcode_sub,     /* 0x9B */
	opcode_sub,     /* 0x9C */
	opcode_sub,     /* 0x9D */
	opcode_sub,     /* 0x9E */
	opcode_sub,     /* 0x9F */
	/* 0xA0 - 0xAF */
	opcode_ld,      /* 0xA0 */
	opcode_ld,      /* 0xA1 */
	opcode_ld,      /* 0xA2 */
	opcode_ld,      /* 0xA3 */
	opcode_ld,      /* 0xA4 */
	opcode_ld,      /* 0xA5 */
	opcode_ld,      /* 0xA6 */
	opcode_ld,      /* 0xA7 */
	opcode_ld,      /* 0xA8 */
	opcode_ld,      /* 0xA9 */
	opcode_ld,      /* 0xAA */
	opcode_ld,      /* 0xAB */
	opcode_ld,      /* 0xAC */
	opcode_ld,      /* 0xAD */
	opcode_ld,      /* 0xAE */
	opcode_ld,      /* 0xAF */
	/* 0xB0 - 0xBF */
	opcode_xch,     /* 0xB0 */
	opcode_xch,     /* 0xB1 */
	opcode_xch,     /* 0xB2 */
	opcode_xch,     /* 0xB3 */
	opcode_xch,     /* 0xB4 */
	opcode_xch,     /* 0xB5 */
	opcode_xch,     /* 0xB6 */
	opcode_xch,     /* 0xB7 */
	opcode_xch,     /* 0xB8 */
	opcode_xch,     /* 0xB9 */
	opcode_xch,     /* 0xBA */
	opcode_xch,     /* 0xBB */
	opcode_xch,     /* 0xBC */
	opcode_xch,     /* 0xBD */
	opcode_xch,     /* 0xBE */
	opcode_xch,     /* 0xBF */
	/* 0xC0 - 0xCF */
	opcode_bbl,     /* 0xC0 */
	opcode_bbl,     /* 0xC1 */
	opcode_bbl,     /* 0xC2 */
	opcode_bbl,     /* 0xC3 */
	opcode_bbl,     /* 0xC4 */
	opcode_bbl,     /* 0xC5 */
	opcode_bbl,     /* 0xC6 */
	opcode_bbl,     /* 0xC7 */
	opcode_bbl,     /* 0xC8 */
	opcode_bbl,     /* 0xC9 */
	opcode_bbl,     /* 0xCA */
	opcode_bbl,     /* 0xCB */
	opcode_bbl,     /* 0xCC */
	opcode_bbl,     /* 0xCD */
	opcode_bbl,     /* 0xCE */
	opcode_bbl,     /* 0xCF */
	/* 0xD0 - 0xDF */
	opcode_ldm,     /* 0xD0 */
	opcode_ldm,     /* 0xD1 */
	opcode_ldm,     /* 0xD2 */
	opcode_ldm,     /* 0xD3 */
	opcode_ldm,     /* 0xD4 */
	opcode_ldm,     /* 0xD5 */
	opcode_ldm,     /* 0xD6 */
	opcode_ldm,     /* 0xD7 */
	opcode_ldm,     /* 0xD8 */
	opcode_ldm,     /* 0xD9 */
	opcode_ldm,     /* 0xDA */
	opcode_ldm,     /* 0xDB */
	opcode_ldm,     /* 0xDC */
	opcode_ldm,     /* 0xDD */
	opcode_ldm,     /* 0xDE */
	opcode_ldm,     /* 0xDF */
	/* 0xE0 - 0xEF */
	opcode_wrm,     /* 0xE0 */
	opcode_wmp,     /* 0xE1 */
	opcode_wrr,     /* 0xE2 */
	opcode_unknown, /* 0xE3 */
	opcode_wrX,     /* 0xE4 */
	opcode_wrX,     /* 0xE5 */
	opcode_wrX,     /* 0xE6 */
	opcode_wrX,     /* 0xE7 */
	opcode_sbm,     /* 0xE8 */
	opcode_rdm,     /* 0xE9 */
	opcode_rdr,     /* 0xEA */
	opcode_adm,     /* 0xEB */
	opcode_rdX,     /* 0xEC */
	opcode_rdX,     /* 0xED */
	opcode_rdX,     /* 0xEE */
	opcode_rdX,     /* 0xEF */
	/* 0xF0 - 0xFF */
	opcode_clb,     /* 0xF0 */
	opcode_clc,     /* 0xF1 */
	opcode_iac,     /* 0xF2 */
	opcode_cmc,     /* 0xF3 */
	opcode_cma,     /* 0xF4 */
	opcode_ral,     /* 0xF5 */
	opcode_rar,     /* 0xF6 */
	opcode_tcc,     /* 0xF7 */
	opcode_dac,     /* 0xF8 */
	opcode_tcs,     /* 0xF9 */
	opcode_stc,     /* 0xFA */
	opcode_daa,     /* 0xFB */
	opcode_kbp,     /* 0xFC */
	opcode_dcl,     /* 0xFD */
	opcode_unknown, /* 0xFE */
	opcode_unknown  /* 0xFF */
};

int opcode_cycles[] = {
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 
};

void cpu_step(struct cpu_state *cpu)
{
	u8 opcode = cpu_read_rom8(cpu, cpu->address[cpu->pc]++);
	/* i4004 has 12-bit program counter (PC) */
	cpu->address[cpu->pc] &= 0xfff;

	opcode_table[opcode](cpu, opcode);
	cpu->cycles += opcode_cycles[opcode];
}

/* Simple memset implementaiton used by cpu_reset */
void cpu_memset(void *ptr, u8 value, u16 size)
{
	u8 *p = (u8*)ptr;
	while (size--) *p++ = value;
}

/* Resets CPU state and clears RAM and status RAM. */
void cpu_reset(struct cpu_state *cpu)
{
	cpu_memset(cpu->regs, 0, 16);
	cpu_memset(cpu->address, 0, sizeof(u16)*4);
	cpu_memset(cpu->ram, 0, cpu->ram_size);
	cpu_memset(cpu->ram_status, 0, cpu->ram_status_size);

	cpu->carry = 0;
	cpu->test = 0;

	cpu->cycles = 0;
	cpu->pc = 0;
	cpu->acc = 0;

	cpu->ram_address = 0;
	cpu->ram_bank = 0;
}

int cpu_init(struct cpu_state *cpu)
{
	/* Validate that pointers are non-NULL */
	if (!cpu || !cpu->rom || !cpu->ram || !cpu->ram_status)
		return -1;

	/* Sizes must not be smaller than the minimum required for i4004 */
	if (cpu->rom_size < 256)
		return -1;
	if (cpu->ram_size < 32)
		return -1;
	if (cpu->ram_status_size < 8)
		return -1;

	/* Memory sizes are rounded down to the nearest supported hardware
	   boundary */
	if (cpu->rom_size > 4096)
		cpu->rom_size = 4096;
	if (cpu->ram_size > 1024)
		cpu->ram_size = 1024;
	if (cpu->ram_status_size > 256)
		cpu->ram_status_size = 256;

	/* It would be better if sizes are aligned to their minimum values */
	cpu->rom_size = (cpu->rom_size / 256) * 256;
	cpu->ram_size = (cpu->ram_size / 32) * 32;
	cpu->ram_status_size = (cpu->ram_status_size / 8) * 8;

	/* Reset the CPU */
	cpu_reset(cpu);

	return 0;
}
