TTY_PORT=0
TTY_WAIT=0
TTY_WRITE=1

ldm TTY_WRITE
wmp
ldm TTY_PORT
wrr

fim p0, 0x48 ; 'H'
jms print_char
fim p0, 0x65 ; 'e'
jms print_char
fim p0, 0x6C ; 'l'
jms print_char
fim p0, 0x6C ; 'l'
jms print_char
fim p0, 0x6F ; 'o'
jms print_char
fim p0, 0x20 ; ' '
jms print_char
fim p0, 0x57 ; 'W'
jms print_char
fim p0, 0x6F ; 'o'
jms print_char
fim p0, 0x72 ; 'r'
jms print_char
fim p0, 0x6C ; 'l'
jms print_char
fim p0, 0x64 ; 'd'
jms print_char
fim p0, 0x0A ; '\n'
jms print_char

jun done

print_char:
	ld r0 ; load first nibble
	wmp
	ldm TTY_PORT
	wrr
	ld r1 ; load second nibble
	wmp
	ldm TTY_PORT
	wrr
	bbl 0

done:
	nop
