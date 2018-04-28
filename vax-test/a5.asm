	.org	0x2004_0000
	BNEQ	^X2004_0000	; branch to self, disp = FE
	BRW	^X2004_0000	; branch back, disp = FFFB

