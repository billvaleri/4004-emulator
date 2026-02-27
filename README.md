# 4004-emulator

A simple, tiny and portable Intel 4004 emulator written in C.

## Description

This project implements a working Intel 4004 emulator. It supports the full 4004 instruction set and can run binary test programs and ROM images.

*This emulator was created as a learning project to explore CPU architecture and low-level programming*

## Features

- Full support for the Intel 4004 instruction set.
- Correct model of registers, flags, and stack
- No dynamic memory allocations
- Easy-to-embed architecture: I/O handlers and debug hooks

## License

MIT

## Inspiration

This project was inspired by the e4004 project by Maciej Szyc ([e4004.szyc.org](http://e4004.szyc.org)). Thanks to the author for ideas on emulator design and testing.

One article was also used to learn the 4004 assembler https://habr.com/ru/articles/269821/

# Intel 4004 Assembler (Tiny, Quick-and-Dirty)

This repository also includes a minimal Intel 4004 assembler for quick testing with the emulator.

## Description

The assembler is intentionally small and simple, quick-and-dirty.
It has no lexer, no expression evaluation, and no preprocessor, but it supports the full 4004 instruction set. Symbol resolution and features are basic by design — this tool exists to quickly produce testable binaries for the emulator.

## Features

- Two-pass assembly (labels collected first, code generated second)

- Immediate values in decimal, hexadecimal(0x...), octal(0...), or binary (0b...)

- Simple constants and labels

- Registers r0–r15 and register pairs p0–p7

- Comments starting with `;`

### The assembler is intentionally ugly and minimal—built for self-study and fast iteration.

## Examples

The `examples/` directory includes several test programs.

Build and run them with:
```bash
# build hello world program
asm/asm examples/hello.asm examples/hello.bin
emu/emu examples/hello.bin
```

# Build

Both the emulator and the assembler include very simple Makefiles for building. You can build them using following commands:
```bash
# Build everything
cd emu && make
cd ../asm && make
```
