.globl _amiga_aga_colour
.globl _amiga_aga_bw
.globl _antic_e_test

/*
	D0	outer count & inner count
	D1	colour
	D2	Bits for bitplane  0
	D3	Bits for bitplane  1
	D4	Bits for bitplanes 2 & 3
	D5	Bits for bitplane  4
	D6	Bits for bitplane  5
	D7	Bits for bitplane  6 & 7
	A0	pointer to atari_screen
	A1	pointer to bitmap 0
	A2	pointer to bitmap 4
*/
_amiga_aga_colour:
	moveml	d0-d7/a0-a2,-(sp)

	movel	48(sp),a0
	movel	52(sp),a1
	lea	46080(a1),a2

	movew	#5760-1,d0

aga_colour_outer:
	swap	d0
	movew	#15,d0

aga_colour_inner:
	moveb	(a0)+,d1

	roxrw	#1,d1	/* Bit for bitplane 0 */
	roxlw	#1,d2

	roxrw	#1,d1	/* Bit for bitplane 1 */
	roxlw	#1,d3

	roxrw	#1,d1	/* Bit for bitplane 2 */
	roxlw	#1,d4
	swap	d4

	roxrw	#1,d1	/* Bit for bitplane 3 */
	roxlw	#1,d4
	swap	d4

	roxrw	#1,d1	/* Bit for bitplane 4 */
	roxlw	#1,d5

	roxrw	#1,d1	/* Bit for bitplane 5 */
	roxlw	#1,d6

	roxrw	#1,d1	/* Bit for bitplane 6 */
	roxlw	#1,d7
	swap	d7

	roxrw	#1,d1	/* Bit for bitplane 7 */
	roxlw	#1,d7
	swap	d7

	dbra	d0,aga_colour_inner

	movew	d3,11520(a1)
	movew	d4,23040(a1)
	swap	d4
	movew	d4,34560(a1)
	movew	d2,(a1)+

	movew	d6,11520(a2)
	movew	d7,23040(a2)
	swap	d7
	movew	d7,34560(a2)
	movew	d5,(a2)+

	swap	d0
	dbra	d0,aga_colour_outer

	moveml	(sp)+,d0-d7/a0-a2

	rts
/*
	D0	outer count & inner count
	D1	colour
	D2	Bits for bitplane  0
	D3	Bits for bitplane  1
	D4	Bits for bitplanes 2
	D5	Bits for bitplane  3
	A0	pointer to atari_screen
	A1	pointer to bitmap 0
	A2	pointer to bitmap 4
*/
_amiga_aga_bw:
	moveml	d0-d7/a0-a2,-(sp)

	movel	48(sp),a0
	movel	52(sp),a1
	lea	46080(a1),a2

	movew	#5760-1,d0

aga_bw_outer:
	swap	d0
	movew	#15,d0

aga_bw_inner:
	moveb	(a0)+,d1

	roxrw	#1,d1	/* Bit for bitplane 0 */
	roxlw	#1,d2

	roxrw	#1,d1	/* Bit for bitplane 1 */
	roxlw	#1,d3

	roxrw	#1,d1	/* Bit for bitplane 2 */
	roxlw	#1,d4

	roxrw	#1,d1	/* Bit for bitplane 3 */
	roxlw	#1,d5

	dbra	d0,aga_bw_inner

	movew	d3,11520(a1)
	movew	d4,23040(a1)
	movew	d5,34560(a1)
	movew	d2,(a1)+

	clrw	11520(a2)
	clrw	23040(a2)
	clrw	34560(a2)
	clrw	(a2)+

	swap	d0
	dbra	d0,aga_bw_outer

	moveml	(sp)+,d0-d7/a0-a2

	rts

_antic_e_test:

	moveml d0-d2/a0-a1,-(sp)

	movel 24(sp),d0
	movel	28(sp),a0
	movel	32(sp),a1

antic_e_loop:
	moveb (a0)+,d1
	beq antic_e_blank

	rorw #6,d1
	andb #3,d1
	moveb d1,(a1)+
	moveb d1,(a1)+

	rolw #2,d1
	andb #3,d1
	moveb d1,(a1)+
	moveb d1,(a1)+

	rolw #2,d1
	andb #3,d1
	moveb d1,(a1)+
	moveb d1,(a1)+

	rolw #2,d1
	andb #3,d1
	moveb d1,(a1)+
	moveb d1,(a1)+

	dbra d0,antic_e_loop

	moveml (sp)+,d0-d2/a0-a1
	rts

antic_e_blank:
	clrl (a1)+
	clrl (a1)+

	dbra d0,antic_e_loop

	moveml (sp)+,d0-d2/a0-a1
	rts


