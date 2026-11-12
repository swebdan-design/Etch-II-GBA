@ ETCH II -- A Generative Synthesis Machine -- by Synth Centric
@ Voice output stage in ARM assembly (IWRAM).
@
@ void mix_out3(const int32_t *src, int32_t *acc, const int32_t *g)
@   acc: MIX_BLOCK interleaved triples [L, R, SEND] (sample x 1024 domain)
@   g:   { gl0, gr0, gs0, dgl, dgr, dgs }  Q12 start gains and per-sample steps
@   for MIX_BLOCK samples: s = src[n] >> 2; acc[3n+k] += s * gk; gk += dgk
@ One LDMIA, three MLAs and one STMIA per sample.
@
@ void mix_out2(const int32_t *src, int32_t *acc, const int32_t *g)
@   the same without the send bus (send level 0).

	.section .iwram, "ax", %progbits
	.arm
	.align 2

	.global mix_out3
mix_out3:
	stmfd	sp!, {r4-r11, lr}
	ldmia	r2, {r3-r8}		@ r3 gl, r4 gr, r5 gs, r6 dgl, r7 dgr, r8 dgs
	mov	lr, #MIX_BLOCK_N
1:
	ldr	r12, [r0], #4
	ldmia	r1, {r9, r10, r11}
	mov	r12, r12, asr #2
	mla	r9, r12, r3, r9
	mla	r10, r12, r4, r10
	mla	r11, r12, r5, r11
	stmia	r1!, {r9, r10, r11}
	add	r3, r3, r6
	add	r4, r4, r7
	add	r5, r5, r8
	subs	lr, lr, #1
	bne	1b
	ldmfd	sp!, {r4-r11, lr}
	bx	lr

	.global mix_out2
mix_out2:
	stmfd	sp!, {r4-r11, lr}
	ldmia	r2, {r3-r8}
	mov	lr, #(MIX_BLOCK_N / 2)
1:
	ldmia	r0!, {r11, r12}		@ two samples
	ldmia	r1, {r5, r8}		@ L0 R0 (send untouched)
	mov	r11, r11, asr #2
	mla	r5, r11, r3, r5
	mla	r8, r11, r4, r8
	add	r3, r3, r6
	add	r4, r4, r7
	stmia	r1, {r5, r8}		@ L0 R0
	add	r1, r1, #12
	ldmia	r1, {r5, r8}		@ L1 R1
	mov	r12, r12, asr #2
	mla	r5, r12, r3, r5
	mla	r8, r12, r4, r8
	add	r3, r3, r6
	add	r4, r4, r7
	stmia	r1, {r5, r8}
	add	r1, r1, #12
	subs	lr, lr, #1
	bne	1b
	ldmfd	sp!, {r4-r11, lr}
	bx	lr
	.pool

@ void mix_out_s8(const int8_t *src, int32_t *acc, const int32_t *g, int32_t shift)
@   like mix_out3 for a cached int8 percussion render: s = src[n] << shift
@   (shift is folded in by the caller: gains already include the render scale)
	.global mix_out_s8
mix_out_s8:
	stmfd	sp!, {r4-r11, lr}
	ldmia	r2, {r3-r8}		@ gl gr gs dgl dgr dgs (Q12 x render scale)
	mov	lr, #MIX_BLOCK_N
1:
	ldrsb	r12, [r0], #1
	ldmia	r1, {r9, r10, r11}
	mla	r9, r12, r3, r9
	mla	r10, r12, r4, r10
	mla	r11, r12, r5, r11
	stmia	r1!, {r9, r10, r11}
	add	r3, r3, r6
	add	r4, r4, r7
	add	r5, r5, r8
	subs	lr, lr, #1
	bne	1b
	ldmfd	sp!, {r4-r11, lr}
	bx	lr
