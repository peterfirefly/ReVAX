	.org	0x2004_0000
	BNEQ	^X2004_0000	; branch to self, disp = FE
	BRW	^X2004_0000	; branch back, disp = FFFB

	ACBW    r1, r2, r3, ^X2004_0000
	SOBGEQ  r1, ^X2004_0000
	AOBLEQ  r1, r2, ^X2004_0000
	BLBS    r0, ^X2004_0000
	BBSS    r0, r1, ^X2004_0000

