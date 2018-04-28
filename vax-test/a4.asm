	.byte	65		; 'A'
	.byte	0
	.byte	1
	.byte	255		; 2^8-1
	.byte	-1
	.byte	-127		; -2^15+1
	.byte	-128		; -2^15
	.byte	0x00
	.byte	0xFF
	.byte	^X00
	.byte	^XFF
	.byte	0		; padding
	.byte	0		; padding
	.byte	0		; padding
	.byte	0		; padding
	.byte	0		; padding

	.byte	66		; 'B'
	.word	0
	.word	1
	.word	65535		; 2^16-1
	.word	-1
	.word	-32767		; -2^15+1
	.word	-32768		; -2^15
	.word	0x0000
	.word	0xFFFF
	.word	^X0000
	.word	^XFFFF
	.word	0		; padding
	.word	0		; padding
	.word	0		; padding
	.word	0		; padding
	.word	0		; padding
	.byte	0		; padding

	.byte	67		; 'C'
	.long	0
	.long	1
	.long	4294967295	; 2^32-1
	.long	-1
	.long	-2147483647	; -2^31+1
	.long	-2147483648	; -2^31
	.long	0x0000_0000
	.long	0xFFFF_FFFF
	.long	^X0000_0000
	.long	^XFFFF_FFFF
	.long	0		; padding
	.word	0		; padding
	.byte	0		; padding

	.byte	68		; 'D'
	.quad	0
	.quad	1
	.quad	18446744073709551615		; 2^64-1
	.quad	-1
	.quad	-9223372036854775807		; -2^63+1
	.quad	-9223372036854775808		; -2^63
	.quad	0x0000_0000_0000_0000
	.quad	0xFFFF_FFFF_FFFF_FFFF
	.quad	^X0000_0000_0000_0000
	.quad	^XFFFF_FFFF_FFFF_FFFF
	.quad	0		; padding
	.long	0		; padding
	.word	0		; padding
	.byte	0		; padding

	.byte	69		; 'E'
	.octo	0
	.octo	1
	.octo	340282366920938463463374607431768211455		; 2^128-1
	.octo	-1
	.octo	-170141183460469231731687303715884105727	; -2^127+1
	.octo	-170141183460469231731687303715884105728	; -2^127
	.octo	0x0000_0000_0000_0000__0000_0000_0000_0000
	.octo	0xFFFF_FFFF_FFFF_FFFF__FFFF_FFFF_FFFF_FFFF
	.octo	^X0000_0000_0000_0000__0000_0000_0000_0000
	.octo	^XFFFF_FFFF_FFFF_FFFF__FFFF_FFFF_FFFF_FFFF

