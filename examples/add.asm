; Add 9 and 10

fim p0, 0x9A
ld r0
clc
add r1
daa

; r2,r3 - result
xch r3
ldm 0
ral
xch r2
