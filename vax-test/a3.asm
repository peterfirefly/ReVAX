	; .org 0
	nop
	movab	(r0), r7

	movl	S^#23, r7
	movl	I^#1234, r0

	movl	(r0), r1
	movl	-(r0), r1
	movl	(r0)+, r1
	movl	@(r0)+, r1

	movl	@#0x1234_5678, r1

	movl	(r0)[r1], r7
	movl	-(r0)[r1], r7
	movl	(r0)+[r1], r7
	movl	@(r0)+[r1], r7
	movl	@#0x1234_5678[r1], r7

	movl	b^2(r1), r8
	movl	b^2, r8
	movl	@b^2(r1), r8
	movl	@b^2, r8
	movl	w^2(r1), r8
	movl	w^2, r8
	movl	@w^2(r1), r8
	movl	@W^2, r8
	movl	l^2(r1), r8
	movl	l^2, r8
	movl	@l^2(r1), r8
	movl	@l^2, r8

	movl	b^2(r1)[r2], r9
	movl	b^2[r2], r9
	movl	@b^2(r1)[r2], r9
	movl	@b^2[r2], r9
	movl	w^2(r1)[r2], r9
	movl	w^2[r2], r9
	movl	@w^2(r1)[r2], r9
	movl	@w^2[r2], r9
	movl	l^2(r1)[r2], r9
	movl	l^2[r2], r9
	movl	@l^2(r1)[r2], r9
	movl	@l^2[r2], r9

