	OPT		P=68040,L1,O+,W-
	xdef		_color_scanline

_color_scanline:
	movem.l		a0-a1/d0-d4,registry

	move.l		4(sp),a0
	move.l		8(sp),a1
	move.w		#384/8-1,d0
	clr.l		d1

loop
	move.w		(a0),d1
	move.w		(a1,d1*2),(a0)+

	move.w		(a0),d1
	move.w		(a1,d1*2),(a0)+

	move.w		(a0),d1
	move.w		(a1,d1*2),(a0)+

	move.w		(a0),d1
	move.w		(a1,d1*2),(a0)+

	dbf		d0,loop


	movem.l		registry,a0-a1/d0-d4
	rts

	section		bss
registry
	ds.l		7
