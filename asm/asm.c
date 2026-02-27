/*
 * Simple, quick and dirty 4004 assembler
 * No lexer, no expressions, no preprocessor
 *
 * A minimal two-pass assembler for the Intel 4004 microprocessor.
 * 
 * Features:
 * - Two-pass assembly (labels collected in pass1, code generated in pass 2)
 * - Support for all 4004 instructions
 * - Numbers: decimal, hex, octal and binary
 * - Constants are defined
 * - Comments starting with ;
*/

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

#ifdef _WIN32
#define strcasecmp(s1,s2)		_stricmp((s1),(s2))
#endif /* _WIN32 */

#define MAX_IMM4		15
#define MAX_IMM8		255
#define MAX_ADR			4095
#define MAX_IMM			MAX_ADR
#define MAX_IDENT		32
#define MAX_TOKENS		5
#define ARRAYSIZE(a)	(sizeof(a)/sizeof(*a))

/* Instructions table for 4004 */
struct template {
	char *mnemonic;
	u8 type;
	u8 length;
	u8 opcode;
};

/* Instruction operand type */
enum {
	INS_NONE,		/* No operands*/
	INS_IMM4,		/* 4-bit immediate */
	INS_REG_IMM8,	/* Register + 8-bit immediate */
	INS_REG,		/* Register */
	INS_PAIR_IMM8,	/* Register pair + 8-bit immediate */
	INS_PAIR,		/* Register pair*/
	INS_IMM4_IMM8,	/* 4-bit + 8-bit immediate */
	INS_ADR			/* 12-bit address */
};

struct template templates[] = {
	{"nop", INS_NONE, 1, 0x00},
	{"jcn", INS_IMM4_IMM8, 2, 0x10},
	{"fim", INS_PAIR_IMM8, 2, 0x20},
	{"src", INS_PAIR, 1, 0x21},
	{"fin", INS_PAIR, 1, 0x30},
	{"jin", INS_PAIR, 1, 0x31},
	{"jun", INS_ADR, 2, 0x40},
	{"jms", INS_ADR, 2, 0x50},
	{"inc", INS_REG, 1, 0x60},
	{"isz", INS_REG_IMM8, 2, 0x70},
	{"add", INS_REG, 1, 0x80},
	{"sub", INS_REG, 1, 0x90},
	{"ld", INS_REG, 1, 0xA0},
	{"xch", INS_REG, 1, 0xB0},
	{"bbl", INS_IMM4, 1, 0xC0},
	{"ldm", INS_IMM4, 1, 0xD0},
	{"wrm", INS_NONE, 1, 0xE0},
	{"wmp", INS_NONE, 1, 0xE1},
	{"wrr", INS_NONE, 1, 0xE2},
	{"wr0", INS_NONE, 1, 0xE4},
	{"wr1", INS_NONE, 1, 0xE5},
	{"wr2", INS_NONE, 1, 0xE6},
	{"wr3", INS_NONE, 1, 0xE7},
	{"sbm", INS_NONE, 1, 0xE8},
	{"rdm", INS_NONE, 1, 0xE9},
	{"rdr", INS_NONE, 1, 0xEA},
	{"adm", INS_NONE, 1, 0xEB},
	{"rd0", INS_NONE, 1, 0xEC},
	{"rd1", INS_NONE, 1, 0xED},
	{"rd2", INS_NONE, 1, 0xEE},
	{"rd3", INS_NONE, 1, 0xEF},
	{"clb", INS_NONE, 1, 0xF0},
	{"clc", INS_NONE, 1, 0xF1},
	{"iac", INS_NONE, 1, 0xF2},
	{"cmc", INS_NONE, 1, 0xF3},
	{"cma", INS_NONE, 1, 0xF4},
	{"ral", INS_NONE, 1, 0xF5},
	{"rar", INS_NONE, 1, 0xF6},
	{"tcc", INS_NONE, 1, 0xF7},
	{"dac", INS_NONE, 1, 0xF8},
	{"tcs", INS_NONE, 1, 0xF9},
	{"stc", INS_NONE, 1, 0xFA},
	{"daa", INS_NONE, 1, 0xFB},
	{"kbp", INS_NONE, 1, 0xFC},
	{"dcl", INS_NONE, 1, 0xFD}
};

/* Token types*/
enum {
	T_EOF,		/* End of file/line */
	T_LABEL,	/* Label (ends with :) */
	T_SYM,		/* Symbol (need resolution) */
	T_INS,		/* Instruction mnemonic */
	T_REG,		/* Register (r0-r15) */
	T_PAIR,		/* Register pair (p0-p7) */
	T_IMM,		/* Immediate value */
	T_EQU		/* Constants */
};

/* Registers numbers */
enum {
	REG_R0, REG_R1, REG_R2, REG_R3,
	REG_R4, REG_R5, REG_R6, REG_R7,
	REG_R8, REG_R9, REG_RA, REG_RB,
	REG_RC, REG_RD, REG_RE, REG_RF
};

/* Register pair numbers */
enum {
	PAIR_P0, PAIR_P1, PAIR_P2, PAIR_P3,
	PAIR_P4, PAIR_P5, PAIR_P6, PAIR_P7
};

#define REGS		16
#define REGS_ALT	6

/* Register names (including alternate r10-r15) */
char *regs[REGS + REGS_ALT] = {
	"r0","r1","r2","r3",
	"r4","r5","r6","r7",
	"r8","r9","ra","rb",
	"rc","rd","re","rf",
	"r10","r11","r12",
	"r13","r14","r15"
};

#define PAIRS		8

/* Register pair names */
char *pairs[PAIRS] = {
	"p0","p1","p2","p3",
	"p4","p5","p6","p7"
};

/* Symbols table entry */
struct symbol {
	struct symbol *next;
	char *name;
	u16 value;
	u8 constant;
};

FILE *output_file;
struct symbol *symbols;	/* Symbol table head*/
u16 origin = 0;			/* Current assembly address */
int line = 0;			/* Current line index */

/* Tell about error and exit the program */
void error(char *fmt, ...)
{
	char text[1024];
	va_list ap;

	va_start(ap,fmt);
	vsnprintf(text, sizeof(text), fmt, ap);
	va_end(ap);

	fprintf(stderr, "Error, line: %d: %s\n", line+1, text);

	if (output_file)
		fclose(output_file);

	exit(1);
}

/* Writes byte to the output file */
void emit(u8 value)
{
	fputc(value, output_file);
}

/* Find symbol by name */
struct symbol *getsym(char *name)
{
	struct symbol *s;

	for (s = symbols; s; s = s->next) {
		if (!strcasecmp(s->name, name)) {
			return s;
		}
	}
	return NULL;
}

/* Add new symbol to table */
void addsym(char *name, u16 value, u8 constant)
{
	struct symbol *s;	

	if (getsym(name)) {
		error("Symbol '%s' already defined", name);
	}

	s = malloc(sizeof(struct symbol));
	assert(s);
	s->name = strdup(name);
	s->constant = constant;
	s->value = value;

	s->next = symbols;
	symbols = s;
}

/* Token structure */
struct token {
	u32 type;
	u32 value;
	char str[MAX_IDENT];
};

/* Skip whitespace and comments */
char *skip_whitespace(char *str)
{
	if (!str || !*str)
		return str;

	/* Skip spaces, tabls, commas */
	while (*str) {
		/* Skip comments */
		if (*str == ';') {
			while (*str && *str != '\r' && *str != '\n')
				str++;
			return str;
		}
		
		if (*str > ' ' && *str != ',')
			break;

		str++;
	}

	return str;
}

/* Parse number in C-style (decimal, hex, octal, binary) */
char *parse_number(char *str, u16 *value)
{
	char *p = str;
	int c;

	*value = 0;

	if (*p == '0') {
		p++;
		/* Hexadecimal: 0x... */
		if (*p == 'x' || *p == 'X') {
			p++;
			while (isxdigit(*p)) {
				c = tolower(*p++);
				*value = *value * 16 + (isdigit(c) ? c - '0' : c - 'a' + 10);
				if (*value > MAX_IMM) {
					error("Value too large (max: %d): %s", MAX_IMM, str);
				}
			}
		/* Binary: 0b... */
		} else if (*p == 'b' || *p == 'B') {
			p++;
			while (*p == '0' || *p == '1') {
				*value = *value * 2 + (*p++ - '0');
				if (*value > MAX_IMM) {
					error("Value too large (max: %d): %s", MAX_IMM, str);
				}
			}
		/* Octal: 0... */
		} else if (isdigit(*p)) {
			while (*p >= '0' && *p <= '7') {
				*value = *value * 8 + (*p++ - '0');
				if (*value > MAX_IMM) {
					error("Value too large (max: %d): %s", MAX_IMM, str);
				}
			}

			if (isdigit(*p)) {
				error("Invalid character '%c' in octal number: %s", *p, str);
			}
		/* Just 0 */
		} else {
			*value = 0;
			return p;
		}
	} else {
		/* Decimal */
		while (isdigit(*p)) {
			*value = *value * 10 + (*p++ - '0');
			if (*value > MAX_IMM) {
				error("Value too large (max: %d): %s", MAX_IMM, str);
			}
		}	
	}

	return p;
}

/* Parse identifier (label, symbol) */
char *parse_identifier(char *str, char *buf)
{
	size_t max = MAX_IDENT-1;

	while (--max && *str && (isalpha(*str) || isdigit(*str) || *str == '_'))
		*buf++ = *str++;

	if (max == 0)
		error("Identifier '%s' too long (max 32 characters)");

	/* Label ends with colon */
	if (*str == ':') {
		*buf++ = *str++;
	}

	*buf = '\0';
	return str;
}

/* JCN condition code aliases */
struct condition {
	char *alias;
	u8 value;
} conds[] = {
	{"nc", 0},
	{"tz", 1},
	{"t0", 1},
	{"tn", 9},
	{"t1", 9},
	{"cn", 2},
	{"c1", 2},
	{"cz", 10},
	{"c0", 10},
	{"az", 4},
	{"a0", 4},
	{"an", 12},
	{"a1", 12},
	{"nza", 12},
	{NULL, 0}
};

/* Find condition code by alias name */
int find_condition(char *str)
{
	int i;
	for (i = 0; conds[i].alias; i++) {
		if (!strcasecmp(str, conds[i].alias))
			return conds[i].value;
	}
	return -1;
}

/* Find string in table */
int find(char *str, char **table, int size)
{
	int i;
	for (i = 0; i < size; i++) {
		if (!strcasecmp(str, table[i])) {
			return i;
		}
	}
	return -1;
}

/* Parse next token from input line */
char *next_token(char *str, struct token *tok)
{
	char buf[MAX_IDENT];
	u16 value = 0;
	int i;

	str = skip_whitespace(str);

	if (*str <= ' ') {
		tok->type = T_EOF;
		return NULL;
	}

	/* Try to parse number */
	if (isdigit(*str)) {
		str = parse_number(str, &value);
		tok->type = T_IMM;
		tok->value = value;
		return str;
	}

	/* Try to parse identifier */
	if (isalpha(*str)) {
		str = parse_identifier(str, buf);

		/* Check for instruction */
		for (i = 0; i < (int)ARRAYSIZE(templates); i++) {
			if (!strcasecmp(buf, templates[i].mnemonic)) {
				tok->type = T_INS;
				tok->value = i;
				return str;
			}
		}

		/* Check for register */
		i = find(buf, regs, REGS);
		if (i != -1) {
			tok->type = T_REG;
			tok->value = i;
			return str;
		}

		/* Check for alternate register names */
		i = find(buf, &regs[REGS], REGS_ALT);
		if (i != -1) {
			tok->type = T_REG;
			tok->value = 10 + i;
			return str;
		}

		/* Check for register pair */
		i = find(buf, pairs, PAIRS);
		if (i != -1) {
			tok->type = T_PAIR;
			tok->value = i;
			return str;
		}

		/* Check for condition alias name */
		i = find_condition(buf);
		if (i != -1) {
			tok->type = T_IMM;
			tok->value = i;
			return str;
		}

		/* Check for label (ends with colon) */
		if (buf[strlen(buf)-1] == ':') {
			tok->type = T_LABEL;
			buf[strlen(buf)-1] = '\0'; /* Remove colon */
			strncpy(tok->str, buf, MAX_IDENT-1);
			tok->str[MAX_IDENT-1] = '\0';
			return str;
		}

		/* It's a symbol */
		tok->type = T_SYM;
		strncpy(tok->str, buf, MAX_IDENT-1);
		tok->str[MAX_IDENT-1] = '\0';
		return str;
	}

	/* It's a constant */
	if (*str == '=') {
		tok->type = T_EQU;
		str++; /* Skip '=' */
		return str;
	}

	error("Invalid character: '%c': %s", *str, str);
	return NULL;
}

/* Check if token is a valid immediate value */
int expect_imm(struct token *token, size_t max)
{
	if (token->type == T_SYM)
		return 1; /* Will be resolved in pass 2 */
	if (token->type != T_IMM)
		return 0;
	if (token->value > max)
		error("Immediate value exceeds bound: %u", max);
	return 1;
}

/* Check if tokens match instruction template */
int match(struct token *tokens, struct template *tmplt)
{
	switch (tmplt->type) {
		case INS_NONE:
			if (tokens[1].type != T_EOF)
				return 0;
			return 1;
		case INS_IMM4:
			if (tokens[2].type != T_EOF)
				return 0;
			return expect_imm(&tokens[1], MAX_IMM4);
		case INS_REG_IMM8:
			if (tokens[3].type != T_EOF)
				return 0;
			if (tokens[1].type != T_REG)
				return 0;
			return expect_imm(&tokens[2], MAX_IMM8);
		case INS_REG:
			if (tokens[2].type != T_EOF)
				return 0;
			return tokens[1].type == T_REG;
		case INS_PAIR_IMM8:
			if (tokens[3].type != T_EOF)
				return 0;
			if (tokens[1].type != T_PAIR)
				return 0;
			return expect_imm(&tokens[2], MAX_IMM8);
		case INS_PAIR:
			if (tokens[2].type != T_EOF)
				return 0;
			return tokens[1].type == T_PAIR;
		case INS_IMM4_IMM8:
			if (tokens[3].type != T_EOF)
				return 0;
			if (!expect_imm(&tokens[1], MAX_IMM4))
				return 0;
			return expect_imm(&tokens[2], MAX_IMM8);
		case INS_ADR:
			if (tokens[2].type != T_EOF)
				return 0;
			return expect_imm(&tokens[1], MAX_ADR);
	}
	return -1;
}

/* Generate machine code from tokens */
void gen(struct token *tokens, struct template *tmplt)
{
	switch(tmplt->type) {
	case INS_NONE:
		emit(tmplt->opcode);
		break;
	case INS_IMM4:
		emit((tmplt->opcode & 0xf0) | tokens[1].value);
		break;
	case INS_REG_IMM8:
		emit((tmplt->opcode & 0xf0) | tokens[1].value);
		emit(tokens[2].value);
		break;
	case INS_REG:
		emit((tmplt->opcode & 0xf0) | tokens[1].value);
		break;
	case INS_PAIR_IMM8:
		emit((tmplt->opcode & 0xf1) | ((tokens[1].value&7)<<1));
		emit(tokens[2].value);
		break;
	case INS_PAIR:
		emit((tmplt->opcode & 0xf1) | ((tokens[1].value&7)<<1));
		break;
	case INS_IMM4_IMM8:
		emit((tmplt->opcode & 0xf0) | tokens[1].value);
		emit(tokens[2].value);
		break;
	case INS_ADR:
		emit((tmplt->opcode & 0xf0) | ((tokens[1].value & 0xf00)>>8));
		emit(tokens[1].value & 0xff);
		break;
	}
}

/*
 * Assemble one line of source code
 * pass: 0 = first pass
 *       1 = second pass
*/
void assemble(char *str, int pass)
{
	struct token tokens[MAX_TOKENS];
	struct template *tmplt;
	struct symbol *sym;
	char *p = str;
	u8 index = 0;

	/* Parse all tokens on line */
	while (index < MAX_TOKENS) {
		p = next_token(p, &tokens[index]);

		if (tokens[index].type == T_EOF)
			break;
		
		/* Handle constants */
		if (tokens[index].type == T_EQU) {
			if (index != 1 || tokens[0].type != T_SYM)
				error("Constant name expected before '=' token");
			p = next_token(p, &tokens[++index]);
			if (tokens[index].type != T_IMM)
				error("Constant (immediate) value expected after '=' token");
			p = next_token(p, &tokens[++index]);
			if (tokens[index].type != T_EOF)
				error("Unexpected end of constant line");
			if (pass == 0)
				addsym(tokens[0].str, tokens[2].value, 1);
			return;
		}

		/* Collect labels during first pass */
		if (tokens[index].type == T_LABEL) {
			if (pass == 0)
				addsym(tokens[index].str, origin, 0);
			continue;
		}

		/* Resolve symbols during second pass */
		if (pass == 1 && tokens[index].type == T_SYM) {
			/* Special case for constants */
			if (index == 0) {
				index++;
				continue;
			}

			sym = getsym(tokens[index].str);
			if (!sym)
				error("Undefined symbol '%s'", tokens[index].str);
			
			tokens[index].type = T_IMM;
			tokens[index].value = sym->value;
		}

		index++;
	}

	/* Skip empty lines */
	if (tokens[0].type == T_EOF)
		return;

	if (index >= MAX_TOKENS)
		error("Too many tokens on line: %s", str);
	if (tokens[0].type != T_INS)
		error("Missing instruction: %s\n", str);

	tmplt = &templates[tokens[0].value];

	if (!match(tokens, tmplt))
		error("Invalid operands for instruction: %s", str);

	origin += tmplt->length;
	if (origin > MAX_ADR) {
		error("Program exceeds available ROM (max address: 0x%X, current: 0x%X)",
			MAX_ADR, origin);
	}

	if (pass == 1)
		gen(tokens, tmplt);
}

int main(int argc, char **argv)
{
	char *output_filename;
	char *input_filename;
	FILE *input_file;
	char str[256];

	if (argc < 2 || argc > 3) {
		fprintf(stderr, "Usage: %s <input filename> [output filename]\n", argv[0]);
		exit(1);
	}

	input_filename = argv[1];
	output_filename = (argc == 3) ? argv[2] : "out.bin";
	
	input_file = fopen(input_filename, "r");
	if (!input_file)
		error("Couldn't open input file '%s': %s",
			input_filename, strerror(errno));

	output_file = fopen(output_filename, "wb");
	if (!output_file)
		error("Couldn't create output file '%s': %s",
			output_filename, strerror(errno));

	/* First pass - collect labels */
	origin = 0;
	line = 0;
	while (fgets(str, sizeof(str), input_file)) {
		assemble(str, 0);
		line++;
	}

	rewind(input_file);

	/* Second pass - generate code */
	origin = 0;
	line = 0;
	/* Second pass */
	while (fgets(str, sizeof(str), input_file)) {
		assemble(str, 1);
		line++;
	}

	fprintf(stderr, "Assembled %u bytes to %s\n",
		origin, output_filename);

	fclose(output_file);
	fclose(input_file);
	return 0;
}
