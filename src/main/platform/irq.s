@ ETCH II -- A Generative Synthesis Machine -- by Synth Centric
@ IRQ entry.  The BIOS saves r0-r3, r12, lr on the IRQ stack and jumps here in
@ ARM/IRQ mode.  We acknowledge the request, then switch to SYSTEM mode (IRQs
@ still masked) so the C handler runs on the large user stack rather than the
@ BIOS's 160-byte IRQ stack, call irq_dispatch(flags), and return to the BIOS.
@ Interrupts never nest: the only enabled source is VBlank, whose handler owns
@ audio mixing and sequencing.

	.section .iwram, "ax", %progbits
	.arm
	.align 2
	.global irq_entry
irq_entry:
	mov	r3, #0x04000000
	add	r3, r3, #0x200
	ldr	r2, [r3]			@ IE (lo) | IF (hi)
	and	r2, r2, r2, lsr #16		@ pending & enabled
	strh	r2, [r3, #2]			@ acknowledge in IF
	ldr	r1, =0x03007FF8			@ BIOS IntrWait flags
	ldrh	r0, [r1]
	orr	r0, r0, r2
	strh	r0, [r1]

	mrs	r1, spsr
	stmfd	sp!, {r1, lr}			@ IRQ-mode state

	mrs	r3, cpsr
	bic	r3, r3, #0x1F
	orr	r3, r3, #0x9F			@ SYS mode, I bit set
	msr	cpsr_c, r3

	stmfd	sp!, {r4, lr}			@ lr_sys belongs to the interrupted code
	mov	r0, r2
	ldr	r1, =irq_dispatch
	mov	lr, pc
	bx	r1
	ldmfd	sp!, {r4, lr}

	mrs	r3, cpsr
	bic	r3, r3, #0x1F
	orr	r3, r3, #0x92			@ back to IRQ mode, I bit set
	msr	cpsr_c, r3

	ldmfd	sp!, {r1, lr}
	msr	spsr_cxsf, r1
	bx	lr
	.pool
