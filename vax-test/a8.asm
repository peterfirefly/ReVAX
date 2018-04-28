; test all fragment groups
;
; normal read/write/modify, quads, bitfields, interlocked, branches

	.org	0x2004_0000

	; "rb/rw/rl/rf"
	TSTB	r0

	; "rq/rd/rg"
	MOVQ	r0, r2

	; "mb/mw/ml/mf"
	INCB	r0
;	INCB	S^#0		; illegal addressing mode
;	INCB	I^#0		; illegal addressing mode

	; "mi"
	ADAWI	S^#2, r0
	ADAWI	S^#2, (r0)
;	ADAWI	S^#2, S^#0	; illegal addressing mode
;	ADAWI	S^#2, I^#0	; illegal addressing mode

	; "mq/md/mg"
	ADDD2	r0, r2
;	ADDD2	r0, S^#0	; illegal addressing mode
;	ADDD2	r0, I^#0	; illegal addressing mode

	; "wb/ww/wl/wf"
	CLRB	r0
;	CLRB	S^#0		; illegal addressing mode
;	CLRB	I^#0		; illegal addressng mode

	; "wq/wd/wg"
	CLRQ	r0
;	CLRQ	S^#0		; illegal addressing mode
;	CLRQ	I^#0		; illegal addressing mode

	; "ab/aw/al/aq"
	PUSHAB	(r0)
;	PUSHAB	r0		; illegal addressing mode
;	PUSHAB	S^#0		; illegal addressing mode
;	PUSHAB	I^#0		; illegal addressing mode

	; "vr"
	EXTV	S^#0, S^#3, (r0), r1	; extract 3 bits starting from bit 0
					; at bitfield pointed to by r0 and put
					; those 3 bits into r1

	; "vm"
	INSV	r1, S^#0, S^#3, (r0)	; take the lower 3 bits from r1 and
					; put them into the bitfield pointed to
					; by r0 at position 0

	; "v1"
	BBS	S^#0, (r0), 0x2004_0000

	; "vi"
	BBSSI	S^#0, (r0), 0x2004_0000

	; "bb/bw"
	BRB	0x2004_0000

