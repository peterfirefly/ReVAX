; CLRB xxx w/ all addressing modes except lit6 and imm

	.org	0x2004_0000

	CLRB	r0
	CLRB	(r0)
	CLRB	-(r0)
	CLRB	(r0)+
	CLRB	@(r0)+

	CLRB	@#0x1234_5678

	CLRB	(r0)[r1]
	CLRB	-(r0)[r1]
	CLRB	(r0)+[r1]
	CLRB	@(r0)+[r1]
	CLRB	@#0x1234_5678[r1]

	CLRB	b^2(r1)
	CLRB	b^2
	CLRB	@b^2(r1)
	CLRB	@b^2
	CLRB	w^2(r1)
	CLRB	w^2
	CLRB	@w^2(r1)
	CLRB	@W^2
	CLRB	l^2(r1)
	CLRB	l^2
	CLRB	@l^2(r1)
	CLRB	@l^2

	CLRB	b^2(r1)[r2]
	CLRB	b^2[r2]
	CLRB	@b^2(r1)[r2]
	CLRB	@b^2[r2]
	CLRB	w^2(r1)[r2]
	CLRB	w^2[r2]
	CLRB	@w^2(r1)[r2]
	CLRB	@w^2[r2]
	CLRB	l^2(r1)[r2]
	CLRB	l^2[r2]
	CLRB	@l^2(r1)[r2]
	CLRB	@l^2[r2]

