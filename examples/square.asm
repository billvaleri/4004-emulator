; squares the value in the accumulator
ldm 6

xch r0
ld r0
xch r1

loop:
	ld r0
	clc
	add r3
	xch r3
	jcn c0, nocarry
	inc r2
nocarry:
	ld r1
	dac
	jcn az done
	xch r1
	jun loop

done:
	nop