	.file	"cpu.c"
	.version	"01.01"
gcc2_compiled.:
.section	.rodata
.LC0:
	.string	"$Id: cpu.c,v 1.11 1996/06/30 23:30:28 david Exp $"
.data
	.align 4
	.type	 rcsid,@object
	.size	 rcsid,4
rcsid:
	.long .LC0
	.align 4
	.type	 ram_below,@object
	.size	 ram_below,4
ram_below:
	.long 40960
	.align 4
	.type	 ram_above,@object
	.size	 ram_above,4
ram_above:
	.long 65535
.text
	.align 16
.globl CPU_GetStatus
	.type	 CPU_GetStatus,@function
CPU_GetStatus:
	pushl %ebp
	movl %esp,%ebp
	cmpb $0,N
	je .L2
	orb $128,regP
	jmp .L3
	.align 16
.L2:
	andb $127,regP
.L3:
	cmpb $0,Z
	je .L4
	andb $253,regP
	jmp .L5
	.align 16
.L4:
	orb $2,regP
.L5:
	cmpb $0,V
	je .L6
	orb $64,regP
	jmp .L7
	.align 16
.L6:
	andb $191,regP
.L7:
	cmpb $0,C
	je .L8
	orb $1,regP
	jmp .L9
	.align 16
.L8:
	andb $254,regP
.L9:
.L1:
	movl %ebp,%esp
	popl %ebp
	ret
.Lfe1:
	.size	 CPU_GetStatus,.Lfe1-CPU_GetStatus
	.align 16
.globl CPU_PutStatus
	.type	 CPU_PutStatus,@function
CPU_PutStatus:
	pushl %ebp
	movl %esp,%ebp
	movb regP,%al
	andb $128,%al
	testb %al,%al
	je .L11
	movb $128,N
	jmp .L12
	.align 16
.L11:
	movb $0,N
.L12:
	movb regP,%al
	andb $2,%al
	testb %al,%al
	je .L13
	movb $0,Z
	jmp .L14
	.align 16
.L13:
	movb $1,Z
.L14:
	movb regP,%al
	andb $64,%al
	testb %al,%al
	je .L15
	movb $1,V
	jmp .L16
	.align 16
.L15:
	movb $0,V
.L16:
	movb regP,%al
	andb $1,%al
	testb %al,%al
	je .L17
	movb $1,C
	jmp .L18
	.align 16
.L17:
	movb $0,C
.L18:
.L10:
	movl %ebp,%esp
	popl %ebp
	ret
.Lfe2:
	.size	 CPU_PutStatus,.Lfe2-CPU_PutStatus
	.align 16
.globl CPU_Reset
	.type	 CPU_Reset,@function
CPU_Reset:
	pushl %ebp
	movl %esp,%ebp
	subl $16,%esp
	pushl %edi
	pushl %esi
	pushl %ebx
	nop
	movl $0,-4(%ebp)
.L20:
	cmpl $255,-4(%ebp)
	jle .L23
	jmp .L21
	.align 16
.L23:
	movl -4(%ebp),%eax
	addl $BCDtoDEC,%eax
	movl %eax,-8(%ebp)
	movl -4(%ebp),%ecx
	sarl $4,%ecx
	movl %ecx,%edx
	andl $15,%edx
	movl %edx,-12(%ebp)
	movl -12(%ebp),%ecx
	sall $3,%ecx
	addl -12(%ebp),%ecx
	movl -12(%ebp),%esi
	addl %ecx,%esi
	movl %esi,-12(%ebp)
	movb -4(%ebp),%cl
	andb $15,%cl
	movb -12(%ebp),%al
	addb %cl,%al
	movl -8(%ebp),%edx
	movb %al,(%edx)
	movl -4(%ebp),%esi
	addl $DECtoBCD,%esi
	movl %esi,-8(%ebp)
	movl -4(%ebp),%ecx
	movl %ecx,%eax
	movl $100,%esi
	cltd
	idivl %esi
	movl %edx,%edi
	movl %eax,-12(%ebp)
	movl %edi,%eax
	movl $10,%esi
	cltd
	idivl %esi
	movl %edx,%ecx
	movl %eax,-16(%ebp)
	movb -16(%ebp),%al
	salb $4,%al
	movb %al,-12(%ebp)
	movl -4(%ebp),%edi
	movl %edi,%eax
	movl $10,%esi
	cltd
	idivl %esi
	movl %edx,-16(%ebp)
	movl %eax,%ecx
	movb -16(%ebp),%al
	orb -12(%ebp),%al
	movl -8(%ebp),%edx
	movb %al,(%edx)
.L22:
	incl -4(%ebp)
	jmp .L20
	.align 16
.L21:
	movl $0,IRQ
	movb $32,regP
	movb $255,regS
	cmpb $2,attrib+65533
	jne .L24
	pushl $65533
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,-8(%ebp)
	movl -8(%ebp),%ebx
	sall $8,%ebx
	jmp .L25
	.align 16
.L24:
	movzbl memory+65533,%ebx
	sall $8,%ebx
.L25:
	cmpb $2,attrib+65532
	jne .L26
	pushl $65532
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,-8(%ebp)
	movl -8(%ebp),%esi
	orl %ebx,%esi
	movl %esi,-12(%ebp)
	movw -12(%ebp),%ax
	movw %ax,-8(%ebp)
	jmp .L27
	.align 16
.L26:
	movzbl memory+65532,%edx
	movl %edx,-12(%ebp)
	movl -12(%ebp),%ecx
	orl %ebx,%ecx
	movw %cx,-8(%ebp)
.L27:
	movw -8(%ebp),%si
	movw %si,regPC
.L19:
	leal -28(%ebp),%esp
	popl %ebx
	popl %esi
	popl %edi
	movl %ebp,%esp
	popl %ebp
	ret
.Lfe3:
	.size	 CPU_Reset,.Lfe3-CPU_Reset
	.align 16
.globl SetRAM
	.type	 SetRAM,@function
SetRAM:
	pushl %ebp
	movl %esp,%ebp
	subl $4,%esp
	nop
	movl 8(%ebp),%eax
	movl %eax,-4(%ebp)
.L29:
	movl -4(%ebp),%eax
	cmpl %eax,12(%ebp)
	jge .L32
	jmp .L30
	.align 16
.L32:
	movl -4(%ebp),%eax
	addl $attrib,%eax
	movb $0,(%eax)
.L31:
	incl -4(%ebp)
	jmp .L29
	.align 16
.L30:
.L28:
	movl %ebp,%esp
	popl %ebp
	ret
.Lfe4:
	.size	 SetRAM,.Lfe4-SetRAM
	.align 16
.globl SetROM
	.type	 SetROM,@function
SetROM:
	pushl %ebp
	movl %esp,%ebp
	subl $4,%esp
	nop
	movl 8(%ebp),%eax
	movl %eax,-4(%ebp)
.L34:
	movl -4(%ebp),%eax
	cmpl %eax,12(%ebp)
	jge .L37
	jmp .L35
	.align 16
.L37:
	movl -4(%ebp),%eax
	addl $attrib,%eax
	movb $1,(%eax)
.L36:
	incl -4(%ebp)
	jmp .L34
	.align 16
.L35:
.L33:
	movl %ebp,%esp
	popl %ebp
	ret
.Lfe5:
	.size	 SetROM,.Lfe5-SetROM
	.align 16
.globl SetHARDWARE
	.type	 SetHARDWARE,@function
SetHARDWARE:
	pushl %ebp
	movl %esp,%ebp
	subl $4,%esp
	nop
	movl 8(%ebp),%eax
	movl %eax,-4(%ebp)
.L39:
	movl -4(%ebp),%eax
	cmpl %eax,12(%ebp)
	jge .L42
	jmp .L40
	.align 16
.L42:
	movl -4(%ebp),%eax
	addl $attrib,%eax
	movb $2,(%eax)
.L41:
	incl -4(%ebp)
	jmp .L39
	.align 16
.L40:
.L38:
	movl %ebp,%esp
	popl %ebp
	ret
.Lfe6:
	.size	 SetHARDWARE,.Lfe6-SetHARDWARE
	.align 16
.globl NMI
	.type	 NMI,@function
NMI:
	pushl %ebp
	movl %esp,%ebp
	subl $8,%esp
	pushl %ebx
	movb regS,%cl
	movb %cl,-1(%ebp)
	movzbl -1(%ebp),%ecx
	movw regPC,%bx
	shrw $8,%bx
	movb %bl,memory+256(%ecx)
	decb -1(%ebp)
	movzbl -1(%ebp),%ecx
	movb regPC,%bl
	movb %bl,memory+256(%ecx)
	decb -1(%ebp)
	movb N,%al
	andb $128,%al
	movb %al,-2(%ebp)
	movzbl -2(%ebp),%eax
	movl %eax,-8(%ebp)
	cmpb $0,V
	je .L44
	movl -8(%ebp),%ebx
	orb $64,%bl
	movb %bl,%cl
	jmp .L45
	.align 16
.L44:
	movb -8(%ebp),%cl
.L45:
	movb %cl,-2(%ebp)
	movb regP,%cl
	andb $60,%cl
	orb %cl,-2(%ebp)
	movzbl -2(%ebp),%edx
	cmpb $0,Z
	jne .L46
	movl %edx,%ebx
	orb $2,%bl
	movb %bl,%cl
	jmp .L47
	.align 16
.L46:
	movb %dl,%cl
.L47:
	movb %cl,-2(%ebp)
	movb C,%al
	orb %al,-2(%ebp)
	movzbl -1(%ebp),%ecx
	movb -2(%ebp),%bl
	movb %bl,memory+256(%ecx)
	decb -1(%ebp)
	orb $4,regP
	movzbw memory+65531,%cx
	movl %ecx,%ebx
	salw $8,%bx
	movzbw memory+65530,%cx
	movl %ebx,%eax
	orw %cx,%ax
	movw %ax,regPC
	movb -1(%ebp),%cl
	movb %cl,regS
.L43:
	movl -12(%ebp),%ebx
	movl %ebp,%esp
	popl %ebp
	ret
.Lfe7:
	.size	 NMI,.Lfe7-NMI
.section	.rodata
.LC1:
	.string	"*** Invalid Opcode %02x at address %04x\n"
.text
	.align 16
.globl GO
	.type	 GO,@function
GO:
	pushl %ebp
	movl %esp,%ebp
	subl $172,%esp
	pushl %edi
	pushl %esi
	pushl %ebx
	movw regPC,%ax
	movw %ax,-2(%ebp)
	movb regS,%al
	movb %al,-3(%ebp)
	movb regA,%al
	movb %al,-4(%ebp)
	movb regX,%al
	movb %al,-5(%ebp)
	movb regY,%al
	movb %al,-6(%ebp)
	cmpl $0,IRQ
	je .L49
	movb regP,%al
	andb $4,%al
	testb %al,%al
	jne .L50
	movw -2(%ebp),%ax
	movw %ax,-12(%ebp)
	movzbl -3(%ebp),%eax
	movw -12(%ebp),%dx
	shrw $8,%dx
	movb %dl,memory+256(%eax)
	decb -3(%ebp)
	movzbl -3(%ebp),%eax
	movb -12(%ebp),%dl
	movb %dl,memory+256(%eax)
	decb -3(%ebp)
	movb N,%bl
	andb $128,%bl
	movb %bl,-9(%ebp)
	movzbl -9(%ebp),%ebx
	movl %ebx,-172(%ebp)
	cmpb $0,V
	je .L51
	movl -172(%ebp),%edx
	orb $64,%dl
	movb %dl,%al
	jmp .L52
	.align 16
.L51:
	movb -172(%ebp),%al
.L52:
	movb %al,-9(%ebp)
	movb regP,%al
	andb $60,%al
	orb %al,-9(%ebp)
	movzbl -9(%ebp),%esi
	cmpb $0,Z
	jne .L53
	movl %esi,%edx
	orb $2,%dl
	movb %dl,%al
	jmp .L54
	.align 16
.L53:
	movl %esi,%ebx
	movb %bl,%al
.L54:
	movb %al,-9(%ebp)
	movb C,%bl
	orb %bl,-9(%ebp)
	movzbl -3(%ebp),%eax
	movb -9(%ebp),%dl
	movb %dl,memory+256(%eax)
	decb -3(%ebp)
	orb $4,regP
	movzbw memory+65535,%ax
	movl %eax,%edx
	salw $8,%dx
	movzbw memory+65534,%ax
	movl %edx,%ebx
	orw %ax,%bx
	movw %bx,-2(%ebp)
	movl $0,IRQ
.L50:
.L49:
	nop
.L55:
	decl 8(%ebp)
	cmpl $-1,8(%ebp)
	jne .L57
	jmp .L56
	.align 16
.L57:
	movzwl -2(%ebp),%edx
	movzbl memory(%edx),%eax
	incw -2(%ebp)
	cmpl $255,%eax
	ja .L572
	movl .L571(,%eax,4),%eax
	jmp *%eax
	.align 16
	.align 4
.L571:
	.long .L59
	.long .L61
	.long .L63
	.long .L65
	.long .L67
	.long .L69
	.long .L71
	.long .L73
	.long .L75
	.long .L77
	.long .L79
	.long .L81
	.long .L83
	.long .L85
	.long .L87
	.long .L89
	.long .L91
	.long .L93
	.long .L95
	.long .L97
	.long .L99
	.long .L101
	.long .L103
	.long .L105
	.long .L107
	.long .L109
	.long .L111
	.long .L113
	.long .L115
	.long .L117
	.long .L119
	.long .L121
	.long .L123
	.long .L125
	.long .L127
	.long .L129
	.long .L131
	.long .L133
	.long .L135
	.long .L137
	.long .L139
	.long .L141
	.long .L143
	.long .L145
	.long .L147
	.long .L149
	.long .L151
	.long .L153
	.long .L155
	.long .L157
	.long .L159
	.long .L161
	.long .L163
	.long .L165
	.long .L167
	.long .L169
	.long .L171
	.long .L173
	.long .L175
	.long .L177
	.long .L179
	.long .L181
	.long .L183
	.long .L185
	.long .L187
	.long .L189
	.long .L191
	.long .L193
	.long .L195
	.long .L197
	.long .L199
	.long .L201
	.long .L203
	.long .L205
	.long .L207
	.long .L209
	.long .L211
	.long .L213
	.long .L215
	.long .L217
	.long .L219
	.long .L221
	.long .L223
	.long .L225
	.long .L227
	.long .L229
	.long .L231
	.long .L233
	.long .L235
	.long .L237
	.long .L239
	.long .L241
	.long .L243
	.long .L245
	.long .L247
	.long .L249
	.long .L251
	.long .L253
	.long .L255
	.long .L257
	.long .L259
	.long .L261
	.long .L263
	.long .L265
	.long .L267
	.long .L269
	.long .L271
	.long .L273
	.long .L275
	.long .L277
	.long .L279
	.long .L281
	.long .L283
	.long .L285
	.long .L287
	.long .L289
	.long .L291
	.long .L293
	.long .L295
	.long .L297
	.long .L299
	.long .L301
	.long .L303
	.long .L305
	.long .L307
	.long .L309
	.long .L311
	.long .L313
	.long .L315
	.long .L317
	.long .L319
	.long .L321
	.long .L323
	.long .L325
	.long .L327
	.long .L329
	.long .L331
	.long .L333
	.long .L335
	.long .L337
	.long .L339
	.long .L341
	.long .L343
	.long .L345
	.long .L347
	.long .L349
	.long .L351
	.long .L353
	.long .L355
	.long .L357
	.long .L359
	.long .L361
	.long .L363
	.long .L365
	.long .L367
	.long .L369
	.long .L371
	.long .L373
	.long .L375
	.long .L377
	.long .L379
	.long .L381
	.long .L383
	.long .L385
	.long .L387
	.long .L389
	.long .L391
	.long .L393
	.long .L395
	.long .L397
	.long .L399
	.long .L401
	.long .L403
	.long .L405
	.long .L407
	.long .L409
	.long .L411
	.long .L413
	.long .L415
	.long .L417
	.long .L419
	.long .L421
	.long .L423
	.long .L425
	.long .L427
	.long .L429
	.long .L431
	.long .L433
	.long .L435
	.long .L437
	.long .L439
	.long .L441
	.long .L443
	.long .L445
	.long .L447
	.long .L449
	.long .L451
	.long .L453
	.long .L455
	.long .L457
	.long .L459
	.long .L461
	.long .L463
	.long .L465
	.long .L467
	.long .L469
	.long .L471
	.long .L473
	.long .L475
	.long .L477
	.long .L479
	.long .L481
	.long .L483
	.long .L485
	.long .L487
	.long .L489
	.long .L491
	.long .L493
	.long .L495
	.long .L497
	.long .L499
	.long .L501
	.long .L503
	.long .L505
	.long .L507
	.long .L509
	.long .L511
	.long .L513
	.long .L515
	.long .L517
	.long .L519
	.long .L521
	.long .L523
	.long .L525
	.long .L527
	.long .L529
	.long .L531
	.long .L533
	.long .L535
	.long .L537
	.long .L539
	.long .L541
	.long .L543
	.long .L545
	.long .L547
	.long .L549
	.long .L551
	.long .L553
	.long .L555
	.long .L557
	.long .L559
	.long .L561
	.long .L563
	.long .L565
	.long .L567
	.long .L569
	.align 16
.L59:
	jmp .L60
	.align 16
.L61:
	jmp .L62
	.align 16
.L63:
	jmp .L64
	.align 16
.L65:
	jmp .L66
	.align 16
.L67:
	jmp .L68
	.align 16
.L69:
	jmp .L70
	.align 16
.L71:
	jmp .L72
	.align 16
.L73:
	jmp .L74
	.align 16
.L75:
	jmp .L76
	.align 16
.L77:
	jmp .L78
	.align 16
.L79:
	jmp .L80
	.align 16
.L81:
	jmp .L82
	.align 16
.L83:
	jmp .L84
	.align 16
.L85:
	jmp .L86
	.align 16
.L87:
	jmp .L88
	.align 16
.L89:
	jmp .L90
	.align 16
.L91:
	jmp .L92
	.align 16
.L93:
	jmp .L94
	.align 16
.L95:
	jmp .L96
	.align 16
.L97:
	jmp .L98
	.align 16
.L99:
	jmp .L100
	.align 16
.L101:
	jmp .L102
	.align 16
.L103:
	jmp .L104
	.align 16
.L105:
	jmp .L106
	.align 16
.L107:
	jmp .L108
	.align 16
.L109:
	jmp .L110
	.align 16
.L111:
	jmp .L112
	.align 16
.L113:
	jmp .L114
	.align 16
.L115:
	jmp .L116
	.align 16
.L117:
	jmp .L118
	.align 16
.L119:
	jmp .L120
	.align 16
.L121:
	jmp .L122
	.align 16
.L123:
	jmp .L124
	.align 16
.L125:
	jmp .L126
	.align 16
.L127:
	jmp .L128
	.align 16
.L129:
	jmp .L130
	.align 16
.L131:
	jmp .L132
	.align 16
.L133:
	jmp .L134
	.align 16
.L135:
	jmp .L136
	.align 16
.L137:
	jmp .L138
	.align 16
.L139:
	jmp .L140
	.align 16
.L141:
	jmp .L142
	.align 16
.L143:
	jmp .L144
	.align 16
.L145:
	jmp .L146
	.align 16
.L147:
	jmp .L148
	.align 16
.L149:
	jmp .L150
	.align 16
.L151:
	jmp .L152
	.align 16
.L153:
	jmp .L154
	.align 16
.L155:
	jmp .L156
	.align 16
.L157:
	jmp .L158
	.align 16
.L159:
	jmp .L160
	.align 16
.L161:
	jmp .L162
	.align 16
.L163:
	jmp .L164
	.align 16
.L165:
	jmp .L166
	.align 16
.L167:
	jmp .L168
	.align 16
.L169:
	jmp .L170
	.align 16
.L171:
	jmp .L172
	.align 16
.L173:
	jmp .L174
	.align 16
.L175:
	jmp .L176
	.align 16
.L177:
	jmp .L178
	.align 16
.L179:
	jmp .L180
	.align 16
.L181:
	jmp .L182
	.align 16
.L183:
	jmp .L184
	.align 16
.L185:
	jmp .L186
	.align 16
.L187:
	jmp .L188
	.align 16
.L189:
	jmp .L190
	.align 16
.L191:
	jmp .L192
	.align 16
.L193:
	jmp .L194
	.align 16
.L195:
	jmp .L196
	.align 16
.L197:
	jmp .L198
	.align 16
.L199:
	jmp .L200
	.align 16
.L201:
	jmp .L202
	.align 16
.L203:
	jmp .L204
	.align 16
.L205:
	jmp .L206
	.align 16
.L207:
	jmp .L208
	.align 16
.L209:
	jmp .L210
	.align 16
.L211:
	jmp .L212
	.align 16
.L213:
	jmp .L214
	.align 16
.L215:
	jmp .L216
	.align 16
.L217:
	jmp .L218
	.align 16
.L219:
	jmp .L220
	.align 16
.L221:
	jmp .L222
	.align 16
.L223:
	jmp .L224
	.align 16
.L225:
	jmp .L226
	.align 16
.L227:
	jmp .L228
	.align 16
.L229:
	jmp .L230
	.align 16
.L231:
	jmp .L232
	.align 16
.L233:
	jmp .L234
	.align 16
.L235:
	jmp .L236
	.align 16
.L237:
	jmp .L238
	.align 16
.L239:
	jmp .L240
	.align 16
.L241:
	jmp .L242
	.align 16
.L243:
	jmp .L244
	.align 16
.L245:
	jmp .L246
	.align 16
.L247:
	jmp .L248
	.align 16
.L249:
	jmp .L250
	.align 16
.L251:
	jmp .L252
	.align 16
.L253:
	jmp .L254
	.align 16
.L255:
	jmp .L256
	.align 16
.L257:
	jmp .L258
	.align 16
.L259:
	jmp .L260
	.align 16
.L261:
	jmp .L262
	.align 16
.L263:
	jmp .L264
	.align 16
.L265:
	jmp .L266
	.align 16
.L267:
	jmp .L268
	.align 16
.L269:
	jmp .L270
	.align 16
.L271:
	jmp .L272
	.align 16
.L273:
	jmp .L274
	.align 16
.L275:
	jmp .L276
	.align 16
.L277:
	jmp .L278
	.align 16
.L279:
	jmp .L280
	.align 16
.L281:
	jmp .L282
	.align 16
.L283:
	jmp .L284
	.align 16
.L285:
	jmp .L286
	.align 16
.L287:
	jmp .L288
	.align 16
.L289:
	jmp .L290
	.align 16
.L291:
	jmp .L292
	.align 16
.L293:
	jmp .L294
	.align 16
.L295:
	jmp .L296
	.align 16
.L297:
	jmp .L298
	.align 16
.L299:
	jmp .L300
	.align 16
.L301:
	jmp .L302
	.align 16
.L303:
	jmp .L304
	.align 16
.L305:
	jmp .L306
	.align 16
.L307:
	jmp .L308
	.align 16
.L309:
	jmp .L310
	.align 16
.L311:
	jmp .L312
	.align 16
.L313:
	jmp .L314
	.align 16
.L315:
	jmp .L316
	.align 16
.L317:
	jmp .L318
	.align 16
.L319:
	jmp .L320
	.align 16
.L321:
	jmp .L322
	.align 16
.L323:
	jmp .L324
	.align 16
.L325:
	jmp .L326
	.align 16
.L327:
	jmp .L328
	.align 16
.L329:
	jmp .L330
	.align 16
.L331:
	jmp .L332
	.align 16
.L333:
	jmp .L334
	.align 16
.L335:
	jmp .L336
	.align 16
.L337:
	jmp .L338
	.align 16
.L339:
	jmp .L340
	.align 16
.L341:
	jmp .L342
	.align 16
.L343:
	jmp .L344
	.align 16
.L345:
	jmp .L346
	.align 16
.L347:
	jmp .L348
	.align 16
.L349:
	jmp .L350
	.align 16
.L351:
	jmp .L352
	.align 16
.L353:
	jmp .L354
	.align 16
.L355:
	jmp .L356
	.align 16
.L357:
	jmp .L358
	.align 16
.L359:
	jmp .L360
	.align 16
.L361:
	jmp .L362
	.align 16
.L363:
	jmp .L364
	.align 16
.L365:
	jmp .L366
	.align 16
.L367:
	jmp .L368
	.align 16
.L369:
	jmp .L370
	.align 16
.L371:
	jmp .L372
	.align 16
.L373:
	jmp .L374
	.align 16
.L375:
	jmp .L376
	.align 16
.L377:
	jmp .L378
	.align 16
.L379:
	jmp .L380
	.align 16
.L381:
	jmp .L382
	.align 16
.L383:
	jmp .L384
	.align 16
.L385:
	jmp .L386
	.align 16
.L387:
	jmp .L388
	.align 16
.L389:
	jmp .L390
	.align 16
.L391:
	jmp .L392
	.align 16
.L393:
	jmp .L394
	.align 16
.L395:
	jmp .L396
	.align 16
.L397:
	jmp .L398
	.align 16
.L399:
	jmp .L400
	.align 16
.L401:
	jmp .L402
	.align 16
.L403:
	jmp .L404
	.align 16
.L405:
	jmp .L406
	.align 16
.L407:
	jmp .L408
	.align 16
.L409:
	jmp .L410
	.align 16
.L411:
	jmp .L412
	.align 16
.L413:
	jmp .L414
	.align 16
.L415:
	jmp .L416
	.align 16
.L417:
	jmp .L418
	.align 16
.L419:
	jmp .L420
	.align 16
.L421:
	jmp .L422
	.align 16
.L423:
	jmp .L424
	.align 16
.L425:
	jmp .L426
	.align 16
.L427:
	jmp .L428
	.align 16
.L429:
	jmp .L430
	.align 16
.L431:
	jmp .L432
	.align 16
.L433:
	jmp .L434
	.align 16
.L435:
	jmp .L436
	.align 16
.L437:
	jmp .L438
	.align 16
.L439:
	jmp .L440
	.align 16
.L441:
	jmp .L442
	.align 16
.L443:
	jmp .L444
	.align 16
.L445:
	jmp .L446
	.align 16
.L447:
	jmp .L448
	.align 16
.L449:
	jmp .L450
	.align 16
.L451:
	jmp .L452
	.align 16
.L453:
	jmp .L454
	.align 16
.L455:
	jmp .L456
	.align 16
.L457:
	jmp .L458
	.align 16
.L459:
	jmp .L460
	.align 16
.L461:
	jmp .L462
	.align 16
.L463:
	jmp .L464
	.align 16
.L465:
	jmp .L466
	.align 16
.L467:
	jmp .L468
	.align 16
.L469:
	jmp .L470
	.align 16
.L471:
	jmp .L472
	.align 16
.L473:
	jmp .L474
	.align 16
.L475:
	jmp .L476
	.align 16
.L477:
	jmp .L478
	.align 16
.L479:
	jmp .L480
	.align 16
.L481:
	jmp .L482
	.align 16
.L483:
	jmp .L484
	.align 16
.L485:
	jmp .L486
	.align 16
.L487:
	jmp .L488
	.align 16
.L489:
	jmp .L490
	.align 16
.L491:
	jmp .L492
	.align 16
.L493:
	jmp .L494
	.align 16
.L495:
	jmp .L496
	.align 16
.L497:
	jmp .L498
	.align 16
.L499:
	jmp .L500
	.align 16
.L501:
	jmp .L502
	.align 16
.L503:
	jmp .L504
	.align 16
.L505:
	jmp .L506
	.align 16
.L507:
	jmp .L508
	.align 16
.L509:
	jmp .L510
	.align 16
.L511:
	jmp .L512
	.align 16
.L513:
	jmp .L514
	.align 16
.L515:
	jmp .L516
	.align 16
.L517:
	jmp .L518
	.align 16
.L519:
	jmp .L520
	.align 16
.L521:
	jmp .L522
	.align 16
.L523:
	jmp .L524
	.align 16
.L525:
	jmp .L526
	.align 16
.L527:
	jmp .L528
	.align 16
.L529:
	jmp .L530
	.align 16
.L531:
	jmp .L532
	.align 16
.L533:
	jmp .L534
	.align 16
.L535:
	jmp .L536
	.align 16
.L537:
	jmp .L538
	.align 16
.L539:
	jmp .L540
	.align 16
.L541:
	jmp .L542
	.align 16
.L543:
	jmp .L544
	.align 16
.L545:
	jmp .L546
	.align 16
.L547:
	jmp .L548
	.align 16
.L549:
	jmp .L550
	.align 16
.L551:
	jmp .L552
	.align 16
.L553:
	jmp .L554
	.align 16
.L555:
	jmp .L556
	.align 16
.L557:
	jmp .L558
	.align 16
.L559:
	jmp .L560
	.align 16
.L561:
	jmp .L562
	.align 16
.L563:
	jmp .L564
	.align 16
.L565:
	jmp .L566
	.align 16
.L567:
	jmp .L568
	.align 16
.L569:
	jmp .L570
	.align 16
.L572:
.L58:
	nop
.L60:
	movb regP,%al
	andb $4,%al
	testb %al,%al
	jne .L573
	movw -2(%ebp),%bx
	incw %bx
	movw %bx,-12(%ebp)
	movzbl -3(%ebp),%eax
	movw -12(%ebp),%dx
	shrw $8,%dx
	movb %dl,memory+256(%eax)
	decb -3(%ebp)
	movzbl -3(%ebp),%eax
	movb -12(%ebp),%dl
	movb %dl,memory+256(%eax)
	decb -3(%ebp)
	orb $16,regP
	movb N,%bl
	andb $128,%bl
	movb %bl,-9(%ebp)
	movzbl -9(%ebp),%edi
	cmpb $0,V
	je .L574
	movl %edi,%edx
	orb $64,%dl
	movb %dl,%al
	jmp .L575
	.align 16
.L574:
	movl %edi,%ebx
	movb %bl,%al
.L575:
	movb %al,-9(%ebp)
	movb regP,%al
	andb $60,%al
	orb %al,-9(%ebp)
	movzbl -9(%ebp),%ebx
	movl %ebx,-28(%ebp)
	cmpb $0,Z
	jne .L576
	movl -28(%ebp),%edx
	orb $2,%dl
	movb %dl,%al
	jmp .L577
	.align 16
.L576:
	movb -28(%ebp),%al
.L577:
	movb %al,-9(%ebp)
	movb C,%bl
	orb %bl,-9(%ebp)
	movzbl -3(%ebp),%eax
	movb -9(%ebp),%dl
	movb %dl,memory+256(%eax)
	decb -3(%ebp)
	orb $4,regP
	movzbw memory+65535,%ax
	movl %eax,%edx
	salw $8,%dx
	movzbw memory+65534,%ax
	movl %edx,%ebx
	orw %ax,%bx
	movw %bx,-2(%ebp)
.L573:
	jmp .L578
	.align 16
.L62:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%dx
	movzbw -5(%ebp),%ax
	movl %edx,%ebx
	addw %ax,%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib+1(%eax)
	jne .L579
	movzwl -8(%ebp),%eax
	leal 1(%eax),%edx
	pushl %edx
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl %eax,%ebx
	sall $8,%ebx
	movl %ebx,-32(%ebp)
	jmp .L580
	.align 16
.L579:
	movzwl -8(%ebp),%eax
	movzbl memory+1(%eax),%ebx
	movl %ebx,-32(%ebp)
	sall $8,-32(%ebp)
.L580:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L581
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl -32(%ebp),%edx
	orl %eax,%edx
	movl %edx,%eax
	jmp .L582
	.align 16
.L581:
	movzwl -8(%ebp),%edx
	movzbl memory(%edx),%ecx
	movl -32(%ebp),%edx
	orl %ecx,%edx
	movl %edx,%eax
.L582:
	movw %ax,-8(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L583
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L584
	.align 16
.L583:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L584:
	movb %al,-9(%ebp)
	movb -4(%ebp),%al
	orb -9(%ebp),%al
	movb %al,%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L68:
	incw -2(%ebp)
	jmp .L578
	.align 16
.L70:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	movb -4(%ebp),%al
	orb -9(%ebp),%al
	movb %al,%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L72:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	movb -9(%ebp),%al
	shrb $7,%al
	movb %al,C
	movb -9(%ebp),%al
	addb %al,%al
	movb %al,%dl
	movb %dl,N
	movb %dl,Z
	movzwl -8(%ebp),%eax
	movb Z,%dl
	movb %dl,memory(%eax)
	jmp .L578
	.align 16
.L76:
	movb N,%bl
	andb $128,%bl
	movb %bl,-9(%ebp)
	movzbl -9(%ebp),%ebx
	movl %ebx,-36(%ebp)
	cmpb $0,V
	je .L585
	movl -36(%ebp),%edx
	orb $64,%dl
	movb %dl,%al
	jmp .L586
	.align 16
.L585:
	movb -36(%ebp),%al
.L586:
	movb %al,-9(%ebp)
	movb regP,%al
	andb $60,%al
	orb %al,-9(%ebp)
	movzbl -9(%ebp),%ebx
	movl %ebx,-40(%ebp)
	cmpb $0,Z
	jne .L587
	movl -40(%ebp),%edx
	orb $2,%dl
	movb %dl,%al
	jmp .L588
	.align 16
.L587:
	movb -40(%ebp),%al
.L588:
	movb %al,-9(%ebp)
	movb C,%bl
	orb %bl,-9(%ebp)
	movzbl -3(%ebp),%eax
	movb -9(%ebp),%dl
	movb %dl,memory+256(%eax)
	decb -3(%ebp)
	jmp .L578
	.align 16
.L78:
	movzwl -2(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	incw -2(%ebp)
	movb -4(%ebp),%al
	orb -9(%ebp),%al
	movb %al,%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L80:
	movb -4(%ebp),%al
	shrb $7,%al
	movb %al,C
	movb -4(%ebp),%al
	addb %al,%al
	movb %al,%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L84:
	addw $2,-2(%ebp)
	jmp .L578
	.align 16
.L86:
	movzwl -2(%ebp),%eax
#APP
	movw memory(%eax),%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L589
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L590
	.align 16
.L589:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L590:
	movb %al,-9(%ebp)
	movb -4(%ebp),%al
	orb -9(%ebp),%al
	movb %al,%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L88:
	movzwl -2(%ebp),%eax
#APP
	movw memory(%eax),%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L591
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L592
	.align 16
.L591:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L592:
	movb %al,-9(%ebp)
	movb -9(%ebp),%al
	shrb $7,%al
	movb %al,C
	movb -9(%ebp),%al
	addb %al,%al
	movb %al,%dl
	movb %dl,N
	movb %dl,Z
	movzwl -8(%ebp),%eax
	cmpb $0,attrib(%eax)
	jne .L593
	movzwl -8(%ebp),%eax
	movb Z,%dl
	movb %dl,memory(%eax)
	jmp .L594
	.align 16
.L593:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L595
	movzbl Z,%eax
	pushl %eax
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_PutByte
	addl $8,%esp
	movl %eax,%eax
	testl %eax,%eax
	je .L596
	jmp .L56
	.align 16
.L596:
.L595:
.L594:
	jmp .L578
	.align 16
.L92:
	movb N,%al
	andb $128,%al
	testb %al,%al
	jne .L597
	movzwl -2(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-13(%ebp)
	movsbw -13(%ebp),%ax
	addw %ax,-2(%ebp)
.L597:
	incw -2(%ebp)
	jmp .L578
	.align 16
.L94:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib+1(%eax)
	jne .L598
	movzwl -8(%ebp),%eax
	leal 1(%eax),%edx
	pushl %edx
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl %eax,%ebx
	sall $8,%ebx
	movl %ebx,-44(%ebp)
	jmp .L599
	.align 16
.L598:
	movzwl -8(%ebp),%eax
	movzbl memory+1(%eax),%ebx
	movl %ebx,-44(%ebp)
	sall $8,-44(%ebp)
.L599:
	movzbl -6(%ebp),%ebx
	movl %ebx,-48(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L600
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl -44(%ebp),%edx
	orl %eax,%edx
	movl -48(%ebp),%ecx
	addl %edx,%ecx
	movl %ecx,%eax
	jmp .L601
	.align 16
.L600:
	movzwl -8(%ebp),%edx
	movzbl memory(%edx),%ecx
	movl -44(%ebp),%edx
	orl %ecx,%edx
	movl -48(%ebp),%ecx
	addl %edx,%ecx
	movl %ecx,%eax
.L601:
	movw %ax,-8(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L602
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L603
	.align 16
.L602:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L603:
	movb %al,-9(%ebp)
	movb -4(%ebp),%al
	orb -9(%ebp),%al
	movb %al,%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L100:
	incw -2(%ebp)
	jmp .L578
	.align 16
.L102:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%dx
	movzbw -5(%ebp),%ax
	addw %ax,%dx
	movl %edx,%ebx
	movb $0,%bh
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	movb -4(%ebp),%al
	orb -9(%ebp),%al
	movb %al,%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L104:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%dx
	movzbw -5(%ebp),%ax
	addw %ax,%dx
	movl %edx,%ebx
	movb $0,%bh
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	movb -9(%ebp),%al
	shrb $7,%al
	movb %al,C
	movb -9(%ebp),%al
	addb %al,%al
	movb %al,%dl
	movb %dl,N
	movb %dl,Z
	movzwl -8(%ebp),%eax
	movb Z,%dl
	movb %dl,memory(%eax)
	jmp .L578
	.align 16
.L108:
	movb $0,C
	jmp .L578
	.align 16
.L110:
	movzwl -2(%ebp),%eax
	movzbw -6(%ebp),%dx
#APP
	movw memory(%eax),%bx; addw %dx,%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L604
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L605
	.align 16
.L604:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L605:
	movb %al,-9(%ebp)
	movb -4(%ebp),%al
	orb -9(%ebp),%al
	movb %al,%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L112:
	jmp .L578
	.align 16
.L116:
	incw -2(%ebp)
	jmp .L578
	.align 16
.L118:
	movzwl -2(%ebp),%eax
	movzbw -5(%ebp),%dx
#APP
	movw memory(%eax),%bx; addw %dx,%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L606
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L607
	.align 16
.L606:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L607:
	movb %al,-9(%ebp)
	movb -4(%ebp),%al
	orb -9(%ebp),%al
	movb %al,%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L120:
	movzwl -2(%ebp),%eax
	movzbw -5(%ebp),%dx
#APP
	movw memory(%eax),%bx; addw %dx,%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L608
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L609
	.align 16
.L608:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L609:
	movb %al,-9(%ebp)
	movb -9(%ebp),%al
	shrb $7,%al
	movb %al,C
	movb -9(%ebp),%al
	addb %al,%al
	movb %al,%dl
	movb %dl,N
	movb %dl,Z
	movzwl -8(%ebp),%eax
	cmpb $0,attrib(%eax)
	jne .L610
	movzwl -8(%ebp),%eax
	movb Z,%dl
	movb %dl,memory(%eax)
	jmp .L611
	.align 16
.L610:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L612
	movzbl Z,%eax
	pushl %eax
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_PutByte
	addl $8,%esp
	movl %eax,%eax
	testl %eax,%eax
	je .L613
	jmp .L56
	.align 16
.L613:
.L612:
.L611:
	jmp .L578
	.align 16
.L124:
	movw -2(%ebp),%bx
	incw %bx
	movw %bx,-12(%ebp)
	movzbl -3(%ebp),%eax
	movw -12(%ebp),%dx
	shrw $8,%dx
	movb %dl,memory+256(%eax)
	decb -3(%ebp)
	movzbl -3(%ebp),%eax
	movb -12(%ebp),%dl
	movb %dl,memory+256(%eax)
	decb -3(%ebp)
	movzwl -2(%ebp),%eax
	movzbw memory+1(%eax),%dx
	movl %edx,%eax
	salw $8,%ax
	movzwl -2(%ebp),%edx
	movzbw memory(%edx),%cx
	movl %eax,%ebx
	orw %cx,%bx
	movw %bx,-2(%ebp)
	jmp .L578
	.align 16
.L126:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%dx
	movzbw -5(%ebp),%ax
	movl %edx,%ebx
	addw %ax,%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib+1(%eax)
	jne .L614
	movzwl -8(%ebp),%eax
	leal 1(%eax),%edx
	pushl %edx
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl %eax,%ebx
	sall $8,%ebx
	movl %ebx,-52(%ebp)
	jmp .L615
	.align 16
.L614:
	movzwl -8(%ebp),%eax
	movzbl memory+1(%eax),%ebx
	movl %ebx,-52(%ebp)
	sall $8,-52(%ebp)
.L615:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L616
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl -52(%ebp),%edx
	orl %eax,%edx
	movl %edx,%eax
	jmp .L617
	.align 16
.L616:
	movzwl -8(%ebp),%edx
	movzbl memory(%edx),%ecx
	movl -52(%ebp),%edx
	orl %ecx,%edx
	movl %edx,%eax
.L617:
	movw %ax,-8(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L618
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L619
	.align 16
.L618:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L619:
	movb %al,-9(%ebp)
	movb -4(%ebp),%al
	andb -9(%ebp),%al
	movb %al,%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L132:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,N
	movb N,%bl
	andb $64,%bl
	movb %bl,V
	movb -4(%ebp),%bl
	andb N,%bl
	movb %bl,Z
	jmp .L578
	.align 16
.L134:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	movb -4(%ebp),%al
	andb -9(%ebp),%al
	movb %al,%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L136:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	cmpb $0,C
	je .L620
	movb -9(%ebp),%al
	shrb $7,%al
	movb %al,C
	movb -9(%ebp),%al
	addb %al,%al
	movb %al,%dl
	orb $1,%dl
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L621
	.align 16
.L620:
	movb -9(%ebp),%al
	shrb $7,%al
	movb %al,C
	movb -9(%ebp),%al
	addb %al,%al
	movb %al,%dl
	movb %dl,N
	movb %dl,Z
.L621:
	movzwl -8(%ebp),%eax
	movb Z,%dl
	movb %dl,memory(%eax)
	jmp .L578
	.align 16
.L140:
	incb -3(%ebp)
	movzbl -3(%ebp),%eax
	movb memory+256(%eax),%dl
	movb %dl,-9(%ebp)
	movb -9(%ebp),%bl
	andb $128,%bl
	movb %bl,N
	movb -9(%ebp),%al
	shrb $6,%al
	movb %al,%dl
	andb $1,%dl
	movb %dl,V
	movb -9(%ebp),%al
	shrb $1,%al
	movb %al,%dl
	xorb $1,%dl
	movb %dl,%al
	andb $1,%al
	movb %al,Z
	movb -9(%ebp),%bl
	andb $1,%bl
	movb %bl,C
	movb -9(%ebp),%al
	andb $60,%al
	movb %al,%bl
	orb $32,%bl
	movb %bl,regP
	jmp .L578
	.align 16
.L142:
	movzwl -2(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	incw -2(%ebp)
	movb -4(%ebp),%al
	andb -9(%ebp),%al
	movb %al,%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L144:
	cmpb $0,C
	je .L622
	movb -4(%ebp),%al
	shrb $7,%al
	movb %al,C
	movb -4(%ebp),%al
	addb %al,%al
	movb %al,%dl
	orb $1,%dl
	movb %dl,%al
	movb %al,-4(%ebp)
	movb %al,%al
	movb %al,N
	movb %al,Z
	jmp .L623
	.align 16
.L622:
	movb -4(%ebp),%al
	shrb $7,%al
	movb %al,C
	movb -4(%ebp),%al
	addb %al,%al
	movb %al,%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
.L623:
	jmp .L578
	.align 16
.L148:
	movzwl -2(%ebp),%eax
#APP
	movw memory(%eax),%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L624
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L625
	.align 16
.L624:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L625:
	movb %al,N
	movb N,%bl
	andb $64,%bl
	movb %bl,V
	movb -4(%ebp),%bl
	andb N,%bl
	movb %bl,Z
	jmp .L578
	.align 16
.L150:
	movzwl -2(%ebp),%eax
#APP
	movw memory(%eax),%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L626
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L627
	.align 16
.L626:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L627:
	movb %al,-9(%ebp)
	movb -4(%ebp),%al
	andb -9(%ebp),%al
	movb %al,%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L152:
	movzwl -2(%ebp),%eax
#APP
	movw memory(%eax),%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L628
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L629
	.align 16
.L628:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L629:
	movb %al,-9(%ebp)
	cmpb $0,C
	je .L630
	movb -9(%ebp),%al
	shrb $7,%al
	movb %al,C
	movb -9(%ebp),%al
	addb %al,%al
	movb %al,%dl
	orb $1,%dl
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L631
	.align 16
.L630:
	movb -9(%ebp),%al
	shrb $7,%al
	movb %al,C
	movb -9(%ebp),%al
	addb %al,%al
	movb %al,%dl
	movb %dl,N
	movb %dl,Z
.L631:
	movzwl -8(%ebp),%eax
	cmpb $0,attrib(%eax)
	jne .L632
	movzwl -8(%ebp),%eax
	movb Z,%dl
	movb %dl,memory(%eax)
	jmp .L633
	.align 16
.L632:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L634
	movzbl Z,%eax
	pushl %eax
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_PutByte
	addl $8,%esp
	movl %eax,%eax
	testl %eax,%eax
	je .L635
	jmp .L56
	.align 16
.L635:
.L634:
.L633:
	jmp .L578
	.align 16
.L156:
	movb N,%al
	andb $128,%al
	testb %al,%al
	je .L636
	movzwl -2(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-13(%ebp)
	movsbw -13(%ebp),%ax
	addw %ax,-2(%ebp)
.L636:
	incw -2(%ebp)
	jmp .L578
	.align 16
.L158:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib+1(%eax)
	jne .L637
	movzwl -8(%ebp),%eax
	leal 1(%eax),%edx
	pushl %edx
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl %eax,%ebx
	sall $8,%ebx
	movl %ebx,-56(%ebp)
	jmp .L638
	.align 16
.L637:
	movzwl -8(%ebp),%eax
	movzbl memory+1(%eax),%ebx
	movl %ebx,-56(%ebp)
	sall $8,-56(%ebp)
.L638:
	movzbl -6(%ebp),%ebx
	movl %ebx,-60(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L639
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl -56(%ebp),%edx
	orl %eax,%edx
	movl -60(%ebp),%ecx
	addl %edx,%ecx
	movl %ecx,%eax
	jmp .L640
	.align 16
.L639:
	movzwl -8(%ebp),%edx
	movzbl memory(%edx),%ecx
	movl -56(%ebp),%edx
	orl %ecx,%edx
	movl -60(%ebp),%ecx
	addl %edx,%ecx
	movl %ecx,%eax
.L640:
	movw %ax,-8(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L641
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L642
	.align 16
.L641:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L642:
	movb %al,-9(%ebp)
	movb -4(%ebp),%al
	andb -9(%ebp),%al
	movb %al,%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L164:
	incw -2(%ebp)
	jmp .L578
	.align 16
.L166:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%dx
	movzbw -5(%ebp),%ax
	addw %ax,%dx
	movl %edx,%ebx
	movb $0,%bh
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	movb -4(%ebp),%al
	andb -9(%ebp),%al
	movb %al,%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L168:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%dx
	movzbw -5(%ebp),%ax
	addw %ax,%dx
	movl %edx,%ebx
	movb $0,%bh
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	cmpb $0,C
	je .L643
	movb -9(%ebp),%al
	shrb $7,%al
	movb %al,C
	movb -9(%ebp),%al
	addb %al,%al
	movb %al,%dl
	orb $1,%dl
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L644
	.align 16
.L643:
	movb -9(%ebp),%al
	shrb $7,%al
	movb %al,C
	movb -9(%ebp),%al
	addb %al,%al
	movb %al,%dl
	movb %dl,N
	movb %dl,Z
.L644:
	movzwl -8(%ebp),%eax
	movb Z,%dl
	movb %dl,memory(%eax)
	jmp .L578
	.align 16
.L172:
	movb $1,C
	jmp .L578
	.align 16
.L174:
	movzwl -2(%ebp),%eax
	movzbw -6(%ebp),%dx
#APP
	movw memory(%eax),%bx; addw %dx,%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L645
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L646
	.align 16
.L645:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L646:
	movb %al,-9(%ebp)
	movb -4(%ebp),%al
	andb -9(%ebp),%al
	movb %al,%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L176:
	jmp .L578
	.align 16
.L180:
	addw $2,-2(%ebp)
	jmp .L578
	.align 16
.L182:
	movzwl -2(%ebp),%eax
	movzbw -5(%ebp),%dx
#APP
	movw memory(%eax),%bx; addw %dx,%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L647
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L648
	.align 16
.L647:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L648:
	movb %al,-9(%ebp)
	movb -4(%ebp),%al
	andb -9(%ebp),%al
	movb %al,%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L184:
	movzwl -2(%ebp),%eax
	movzbw -5(%ebp),%dx
#APP
	movw memory(%eax),%bx; addw %dx,%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L649
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L650
	.align 16
.L649:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L650:
	movb %al,-9(%ebp)
	cmpb $0,C
	je .L651
	movb -9(%ebp),%al
	shrb $7,%al
	movb %al,C
	movb -9(%ebp),%al
	addb %al,%al
	movb %al,%dl
	orb $1,%dl
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L652
	.align 16
.L651:
	movb -9(%ebp),%al
	shrb $7,%al
	movb %al,C
	movb -9(%ebp),%al
	addb %al,%al
	movb %al,%dl
	movb %dl,N
	movb %dl,Z
.L652:
	movzwl -8(%ebp),%eax
	cmpb $0,attrib(%eax)
	jne .L653
	movzwl -8(%ebp),%eax
	movb Z,%dl
	movb %dl,memory(%eax)
	jmp .L654
	.align 16
.L653:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L655
	movzbl Z,%eax
	pushl %eax
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_PutByte
	addl $8,%esp
	movl %eax,%eax
	testl %eax,%eax
	je .L656
	jmp .L56
	.align 16
.L656:
.L655:
.L654:
	jmp .L578
	.align 16
.L188:
	incb -3(%ebp)
	movzbl -3(%ebp),%eax
	movb memory+256(%eax),%dl
	movb %dl,-9(%ebp)
	movb -9(%ebp),%bl
	andb $128,%bl
	movb %bl,N
	movb -9(%ebp),%al
	shrb $6,%al
	movb %al,%dl
	andb $1,%dl
	movb %dl,V
	movb -9(%ebp),%al
	shrb $1,%al
	movb %al,%dl
	xorb $1,%dl
	movb %dl,%al
	andb $1,%al
	movb %al,Z
	movb -9(%ebp),%bl
	andb $1,%bl
	movb %bl,C
	movb -9(%ebp),%al
	andb $60,%al
	movb %al,%bl
	orb $32,%bl
	movb %bl,regP
	incb -3(%ebp)
	movzbl -3(%ebp),%eax
	movb memory+256(%eax),%dl
	movb %dl,-9(%ebp)
	incb -3(%ebp)
	movzbl -3(%ebp),%eax
	movzbw memory+256(%eax),%dx
	movl %edx,%eax
	salw $8,%ax
	movzbw -9(%ebp),%dx
	movl %eax,%ebx
	orw %dx,%bx
	movw %bx,-2(%ebp)
	jmp .L578
	.align 16
.L190:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%dx
	movzbw -5(%ebp),%ax
	movl %edx,%ebx
	addw %ax,%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib+1(%eax)
	jne .L657
	movzwl -8(%ebp),%eax
	leal 1(%eax),%edx
	pushl %edx
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl %eax,%ebx
	sall $8,%ebx
	movl %ebx,-64(%ebp)
	jmp .L658
	.align 16
.L657:
	movzwl -8(%ebp),%eax
	movzbl memory+1(%eax),%ebx
	movl %ebx,-64(%ebp)
	sall $8,-64(%ebp)
.L658:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L659
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl -64(%ebp),%edx
	orl %eax,%edx
	movl %edx,%eax
	jmp .L660
	.align 16
.L659:
	movzwl -8(%ebp),%edx
	movzbl memory(%edx),%ecx
	movl -64(%ebp),%edx
	orl %ecx,%edx
	movl %edx,%eax
.L660:
	movw %ax,-8(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L661
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L662
	.align 16
.L661:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L662:
	movb %al,-9(%ebp)
	movb -4(%ebp),%al
	xorb -9(%ebp),%al
	movb %al,%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L196:
	incw -2(%ebp)
	jmp .L578
	.align 16
.L198:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	movb -4(%ebp),%al
	xorb -9(%ebp),%al
	movb %al,%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L200:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	movb -9(%ebp),%bl
	andb $1,%bl
	movb %bl,C
	movb -9(%ebp),%bl
	shrb $1,%bl
	movb %bl,Z
	movb $0,N
	movzwl -8(%ebp),%eax
	movb Z,%dl
	movb %dl,memory(%eax)
	jmp .L578
	.align 16
.L204:
	movzbl -3(%ebp),%eax
	movb -4(%ebp),%dl
	movb %dl,memory+256(%eax)
	decb -3(%ebp)
	jmp .L578
	.align 16
.L206:
	movzwl -2(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	incw -2(%ebp)
	movb -4(%ebp),%al
	xorb -9(%ebp),%al
	movb %al,%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L208:
	movb -4(%ebp),%bl
	andb $1,%bl
	movb %bl,C
	shrb $1,-4(%ebp)
	movb $0,N
	movb -4(%ebp),%al
	movb %al,Z
	jmp .L578
	.align 16
.L212:
	movzwl -2(%ebp),%eax
	movzbw memory+1(%eax),%dx
	movl %edx,%eax
	salw $8,%ax
	movzwl -2(%ebp),%edx
	movzbw memory(%edx),%cx
	movl %eax,%ebx
	orw %cx,%bx
	movw %bx,-2(%ebp)
	jmp .L578
	.align 16
.L214:
	movzwl -2(%ebp),%eax
#APP
	movw memory(%eax),%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L663
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L664
	.align 16
.L663:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L664:
	movb %al,-9(%ebp)
	movb -4(%ebp),%al
	xorb -9(%ebp),%al
	movb %al,%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L216:
	movzwl -2(%ebp),%eax
#APP
	movw memory(%eax),%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L665
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L666
	.align 16
.L665:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L666:
	movb %al,-9(%ebp)
	movb -9(%ebp),%bl
	andb $1,%bl
	movb %bl,C
	movb -9(%ebp),%bl
	shrb $1,%bl
	movb %bl,Z
	movb $0,N
	movzwl -8(%ebp),%eax
	cmpb $0,attrib(%eax)
	jne .L667
	movzwl -8(%ebp),%eax
	movb Z,%dl
	movb %dl,memory(%eax)
	jmp .L668
	.align 16
.L667:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L669
	movzbl Z,%eax
	pushl %eax
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_PutByte
	addl $8,%esp
	movl %eax,%eax
	testl %eax,%eax
	je .L670
	jmp .L56
	.align 16
.L670:
.L669:
.L668:
	jmp .L578
	.align 16
.L220:
	cmpb $0,V
	jne .L671
	movzwl -2(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-13(%ebp)
	movsbw -13(%ebp),%ax
	addw %ax,-2(%ebp)
.L671:
	incw -2(%ebp)
	jmp .L578
	.align 16
.L222:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib+1(%eax)
	jne .L672
	movzwl -8(%ebp),%eax
	leal 1(%eax),%edx
	pushl %edx
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl %eax,%ebx
	sall $8,%ebx
	movl %ebx,-68(%ebp)
	jmp .L673
	.align 16
.L672:
	movzwl -8(%ebp),%eax
	movzbl memory+1(%eax),%ebx
	movl %ebx,-68(%ebp)
	sall $8,-68(%ebp)
.L673:
	movzbl -6(%ebp),%ebx
	movl %ebx,-72(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L674
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl -68(%ebp),%edx
	orl %eax,%edx
	movl -72(%ebp),%ecx
	addl %edx,%ecx
	movl %ecx,%eax
	jmp .L675
	.align 16
.L674:
	movzwl -8(%ebp),%edx
	movzbl memory(%edx),%ecx
	movl -68(%ebp),%edx
	orl %ecx,%edx
	movl -72(%ebp),%ecx
	addl %edx,%ecx
	movl %ecx,%eax
.L675:
	movw %ax,-8(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L676
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L677
	.align 16
.L676:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L677:
	movb %al,-9(%ebp)
	movb -4(%ebp),%al
	xorb -9(%ebp),%al
	movb %al,%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L228:
	incw -2(%ebp)
	jmp .L578
	.align 16
.L230:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%dx
	movzbw -5(%ebp),%ax
	addw %ax,%dx
	movl %edx,%ebx
	movb $0,%bh
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	movb -4(%ebp),%al
	xorb -9(%ebp),%al
	movb %al,%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L232:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%dx
	movzbw -5(%ebp),%ax
	addw %ax,%dx
	movl %edx,%ebx
	movb $0,%bh
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	movb -9(%ebp),%bl
	andb $1,%bl
	movb %bl,C
	movb -9(%ebp),%bl
	shrb $1,%bl
	movb %bl,Z
	movb $0,N
	movzwl -8(%ebp),%eax
	movb Z,%dl
	movb %dl,memory(%eax)
	jmp .L578
	.align 16
.L236:
	andb $251,regP
	jmp .L578
	.align 16
.L238:
	movzwl -2(%ebp),%eax
	movzbw -6(%ebp),%dx
#APP
	movw memory(%eax),%bx; addw %dx,%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L678
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L679
	.align 16
.L678:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L679:
	movb %al,-9(%ebp)
	movb -4(%ebp),%al
	xorb -9(%ebp),%al
	movb %al,%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L240:
	jmp .L578
	.align 16
.L244:
	addw $2,-2(%ebp)
	jmp .L578
	.align 16
.L246:
	movzwl -2(%ebp),%eax
	movzbw -5(%ebp),%dx
#APP
	movw memory(%eax),%bx; addw %dx,%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L680
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L681
	.align 16
.L680:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L681:
	movb %al,-9(%ebp)
	movb -4(%ebp),%al
	xorb -9(%ebp),%al
	movb %al,%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L248:
	movzwl -2(%ebp),%eax
	movzbw -5(%ebp),%dx
#APP
	movw memory(%eax),%bx; addw %dx,%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L682
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L683
	.align 16
.L682:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L683:
	movb %al,-9(%ebp)
	movb -9(%ebp),%bl
	andb $1,%bl
	movb %bl,C
	movb -9(%ebp),%bl
	shrb $1,%bl
	movb %bl,Z
	movb $0,N
	movzwl -8(%ebp),%eax
	cmpb $0,attrib(%eax)
	jne .L684
	movzwl -8(%ebp),%eax
	movb Z,%dl
	movb %dl,memory(%eax)
	jmp .L685
	.align 16
.L684:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L686
	movzbl Z,%eax
	pushl %eax
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_PutByte
	addl $8,%esp
	movl %eax,%eax
	testl %eax,%eax
	je .L687
	jmp .L56
	.align 16
.L687:
.L686:
.L685:
	jmp .L578
	.align 16
.L252:
	incb -3(%ebp)
	movzbl -3(%ebp),%eax
	movb memory+256(%eax),%dl
	movb %dl,-9(%ebp)
	incb -3(%ebp)
	movzbl -3(%ebp),%eax
	movzbw memory+256(%eax),%dx
	movl %edx,%eax
	salw $8,%ax
	movzbw -9(%ebp),%dx
	orw %dx,%ax
	movl %eax,%ebx
	incw %bx
	movw %bx,-2(%ebp)
	jmp .L578
	.align 16
.L254:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%dx
	movzbw -5(%ebp),%ax
	movl %edx,%ebx
	addw %ax,%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib+1(%eax)
	jne .L688
	movzwl -8(%ebp),%eax
	leal 1(%eax),%edx
	pushl %edx
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl %eax,%ebx
	sall $8,%ebx
	movl %ebx,-76(%ebp)
	jmp .L689
	.align 16
.L688:
	movzwl -8(%ebp),%eax
	movzbl memory+1(%eax),%ebx
	movl %ebx,-76(%ebp)
	sall $8,-76(%ebp)
.L689:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L690
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl -76(%ebp),%edx
	orl %eax,%edx
	movl %edx,%eax
	jmp .L691
	.align 16
.L690:
	movzwl -8(%ebp),%edx
	movzbl memory(%edx),%ecx
	movl -76(%ebp),%edx
	orl %ecx,%edx
	movl %edx,%eax
.L691:
	movw %ax,-8(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L692
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L693
	.align 16
.L692:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L693:
	movb %al,-9(%ebp)
	jmp .L694
	.align 16
.L260:
	incw -2(%ebp)
	jmp .L578
	.align 16
.L262:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	jmp .L694
	.align 16
.L264:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	cmpb $0,C
	je .L695
	movb -9(%ebp),%bl
	andb $1,%bl
	movb %bl,C
	movb -9(%ebp),%al
	shrb $1,%al
	movb %al,%dl
	orb $128,%dl
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L696
	.align 16
.L695:
	movb -9(%ebp),%bl
	andb $1,%bl
	movb %bl,C
	movb -9(%ebp),%al
	shrb $1,%al
	movb %al,%dl
	movb %dl,N
	movb %dl,Z
.L696:
	movzwl -8(%ebp),%eax
	movb Z,%dl
	movb %dl,memory(%eax)
	jmp .L578
	.align 16
.L268:
	incb -3(%ebp)
	movzbl -3(%ebp),%eax
	movb memory+256(%eax),%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L270:
	movzwl -2(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	incw -2(%ebp)
	jmp .L694
	.align 16
.L272:
	cmpb $0,C
	je .L697
	movb -4(%ebp),%bl
	andb $1,%bl
	movb %bl,C
	movb -4(%ebp),%al
	shrb $1,%al
	movb %al,%dl
	orb $128,%dl
	movb %dl,%al
	movb %al,-4(%ebp)
	movb %al,%al
	movb %al,N
	movb %al,Z
	jmp .L698
	.align 16
.L697:
	movb -4(%ebp),%bl
	andb $1,%bl
	movb %bl,C
	movb -4(%ebp),%al
	shrb $1,%al
	movb %al,%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
.L698:
	jmp .L578
	.align 16
.L276:
	movzwl -2(%ebp),%eax
	movzbw memory+1(%eax),%dx
	movl %edx,%eax
	salw $8,%ax
	movzwl -2(%ebp),%edx
	movzbw memory(%edx),%cx
	movl %eax,%ebx
	orw %cx,%bx
	movw %bx,-8(%ebp)
	movzwl -8(%ebp),%eax
	movzbw memory+1(%eax),%dx
	movl %edx,%eax
	salw $8,%ax
	movzwl -8(%ebp),%edx
	movzbw memory(%edx),%cx
	movl %eax,%ebx
	orw %cx,%bx
	movw %bx,-2(%ebp)
	jmp .L578
	.align 16
.L278:
	movzwl -2(%ebp),%eax
#APP
	movw memory(%eax),%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L699
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L700
	.align 16
.L699:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L700:
	movb %al,-9(%ebp)
	jmp .L694
	.align 16
.L280:
	movzwl -2(%ebp),%eax
#APP
	movw memory(%eax),%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L701
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L702
	.align 16
.L701:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L702:
	movb %al,-9(%ebp)
	cmpb $0,C
	je .L703
	movb -9(%ebp),%bl
	andb $1,%bl
	movb %bl,C
	movb -9(%ebp),%al
	shrb $1,%al
	movb %al,%dl
	orb $128,%dl
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L704
	.align 16
.L703:
	movb -9(%ebp),%bl
	andb $1,%bl
	movb %bl,C
	movb -9(%ebp),%al
	shrb $1,%al
	movb %al,%dl
	movb %dl,N
	movb %dl,Z
.L704:
	movzwl -8(%ebp),%eax
	cmpb $0,attrib(%eax)
	jne .L705
	movzwl -8(%ebp),%eax
	movb Z,%dl
	movb %dl,memory(%eax)
	jmp .L706
	.align 16
.L705:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L707
	movzbl Z,%eax
	pushl %eax
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_PutByte
	addl $8,%esp
	movl %eax,%eax
	testl %eax,%eax
	je .L708
	jmp .L56
	.align 16
.L708:
.L707:
.L706:
	jmp .L578
	.align 16
.L284:
	cmpb $0,V
	je .L709
	movzwl -2(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-13(%ebp)
	movsbw -13(%ebp),%ax
	addw %ax,-2(%ebp)
.L709:
	incw -2(%ebp)
	jmp .L578
	.align 16
.L286:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib+1(%eax)
	jne .L710
	movzwl -8(%ebp),%eax
	leal 1(%eax),%edx
	pushl %edx
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl %eax,%ebx
	sall $8,%ebx
	movl %ebx,-80(%ebp)
	jmp .L711
	.align 16
.L710:
	movzwl -8(%ebp),%eax
	movzbl memory+1(%eax),%ebx
	movl %ebx,-80(%ebp)
	sall $8,-80(%ebp)
.L711:
	movzbl -6(%ebp),%ebx
	movl %ebx,-84(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L712
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl -80(%ebp),%edx
	orl %eax,%edx
	movl -84(%ebp),%ecx
	addl %edx,%ecx
	movl %ecx,%eax
	jmp .L713
	.align 16
.L712:
	movzwl -8(%ebp),%edx
	movzbl memory(%edx),%ecx
	movl -80(%ebp),%edx
	orl %ecx,%edx
	movl -84(%ebp),%ecx
	addl %edx,%ecx
	movl %ecx,%eax
.L713:
	movw %ax,-8(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L714
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L715
	.align 16
.L714:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L715:
	movb %al,-9(%ebp)
	jmp .L694
	.align 16
.L292:
	incw -2(%ebp)
	jmp .L578
	.align 16
.L294:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%dx
	movzbw -5(%ebp),%ax
	addw %ax,%dx
	movl %edx,%ebx
	movb $0,%bh
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	jmp .L694
	.align 16
.L296:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%dx
	movzbw -5(%ebp),%ax
	addw %ax,%dx
	movl %edx,%ebx
	movb $0,%bh
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	cmpb $0,C
	je .L716
	movb -9(%ebp),%bl
	andb $1,%bl
	movb %bl,C
	movb -9(%ebp),%al
	shrb $1,%al
	movb %al,%dl
	orb $128,%dl
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L717
	.align 16
.L716:
	movb -9(%ebp),%bl
	andb $1,%bl
	movb %bl,C
	movb -9(%ebp),%al
	shrb $1,%al
	movb %al,%dl
	movb %dl,N
	movb %dl,Z
.L717:
	movzwl -8(%ebp),%eax
	movb Z,%dl
	movb %dl,memory(%eax)
	jmp .L578
	.align 16
.L300:
	orb $4,regP
	jmp .L578
	.align 16
.L302:
	movzwl -2(%ebp),%eax
	movzbw -6(%ebp),%dx
#APP
	movw memory(%eax),%bx; addw %dx,%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L718
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L719
	.align 16
.L718:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L719:
	movb %al,-9(%ebp)
	jmp .L694
	.align 16
.L304:
	jmp .L578
	.align 16
.L308:
	addw $2,-2(%ebp)
	jmp .L578
	.align 16
.L310:
	movzwl -2(%ebp),%eax
	movzbw -5(%ebp),%dx
#APP
	movw memory(%eax),%bx; addw %dx,%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L720
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L721
	.align 16
.L720:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L721:
	movb %al,-9(%ebp)
	jmp .L694
	.align 16
.L312:
	movzwl -2(%ebp),%eax
	movzbw -5(%ebp),%dx
#APP
	movw memory(%eax),%bx; addw %dx,%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L722
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L723
	.align 16
.L722:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L723:
	movb %al,-9(%ebp)
	cmpb $0,C
	je .L724
	movb -9(%ebp),%bl
	andb $1,%bl
	movb %bl,C
	movb -9(%ebp),%al
	shrb $1,%al
	movb %al,%dl
	orb $128,%dl
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L725
	.align 16
.L724:
	movb -9(%ebp),%bl
	andb $1,%bl
	movb %bl,C
	movb -9(%ebp),%al
	shrb $1,%al
	movb %al,%dl
	movb %dl,N
	movb %dl,Z
.L725:
	movzwl -8(%ebp),%eax
	cmpb $0,attrib(%eax)
	jne .L726
	movzwl -8(%ebp),%eax
	movb Z,%dl
	movb %dl,memory(%eax)
	jmp .L727
	.align 16
.L726:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L728
	movzbl Z,%eax
	pushl %eax
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_PutByte
	addl $8,%esp
	movl %eax,%eax
	testl %eax,%eax
	je .L729
	jmp .L56
	.align 16
.L729:
.L728:
.L727:
	jmp .L578
	.align 16
.L316:
	incw -2(%ebp)
	jmp .L578
	.align 16
.L318:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%dx
	movzbw -5(%ebp),%ax
	movl %edx,%ebx
	addw %ax,%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib+1(%eax)
	jne .L730
	movzwl -8(%ebp),%eax
	leal 1(%eax),%edx
	pushl %edx
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl %eax,%ebx
	sall $8,%ebx
	movl %ebx,-88(%ebp)
	jmp .L731
	.align 16
.L730:
	movzwl -8(%ebp),%eax
	movzbl memory+1(%eax),%ebx
	movl %ebx,-88(%ebp)
	sall $8,-88(%ebp)
.L731:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L732
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl -88(%ebp),%edx
	orl %eax,%edx
	movl %edx,%eax
	jmp .L733
	.align 16
.L732:
	movzwl -8(%ebp),%edx
	movzbl memory(%edx),%ecx
	movl -88(%ebp),%edx
	orl %ecx,%edx
	movl %edx,%eax
.L733:
	movw %ax,-8(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $0,attrib(%eax)
	jne .L734
	movzwl -8(%ebp),%eax
	movb -4(%ebp),%dl
	movb %dl,memory(%eax)
	jmp .L735
	.align 16
.L734:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L736
	movzbl -4(%ebp),%eax
	pushl %eax
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_PutByte
	addl $8,%esp
	movl %eax,%eax
	testl %eax,%eax
	je .L737
	jmp .L56
	.align 16
.L737:
.L736:
.L735:
	jmp .L578
	.align 16
.L320:
	incw -2(%ebp)
	jmp .L578
	.align 16
.L324:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb -6(%ebp),%dl
	movb %dl,memory(%eax)
	jmp .L578
	.align 16
.L326:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb -4(%ebp),%dl
	movb %dl,memory(%eax)
	jmp .L578
	.align 16
.L328:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb -5(%ebp),%dl
	movb %dl,memory(%eax)
	jmp .L578
	.align 16
.L332:
	decb -6(%ebp)
	movb -6(%ebp),%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L334:
	incw -2(%ebp)
	jmp .L578
	.align 16
.L336:
	movb -5(%ebp),%al
	movb %al,-4(%ebp)
	movb %al,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L340:
	movzwl -2(%ebp),%eax
#APP
	movw memory(%eax),%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $0,attrib(%eax)
	jne .L738
	movzwl -8(%ebp),%eax
	movb -6(%ebp),%dl
	movb %dl,memory(%eax)
	jmp .L739
	.align 16
.L738:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L740
	movzbl -6(%ebp),%eax
	pushl %eax
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_PutByte
	addl $8,%esp
	movl %eax,%eax
	testl %eax,%eax
	je .L741
	jmp .L56
	.align 16
.L741:
.L740:
.L739:
	jmp .L578
	.align 16
.L342:
	movzwl -2(%ebp),%eax
#APP
	movw memory(%eax),%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $0,attrib(%eax)
	jne .L742
	movzwl -8(%ebp),%eax
	movb -4(%ebp),%dl
	movb %dl,memory(%eax)
	jmp .L743
	.align 16
.L742:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L744
	movzbl -4(%ebp),%eax
	pushl %eax
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_PutByte
	addl $8,%esp
	movl %eax,%eax
	testl %eax,%eax
	je .L745
	jmp .L56
	.align 16
.L745:
.L744:
.L743:
	jmp .L578
	.align 16
.L344:
	movzwl -2(%ebp),%eax
#APP
	movw memory(%eax),%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $0,attrib(%eax)
	jne .L746
	movzwl -8(%ebp),%eax
	movb -5(%ebp),%dl
	movb %dl,memory(%eax)
	jmp .L747
	.align 16
.L746:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L748
	movzbl -5(%ebp),%eax
	pushl %eax
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_PutByte
	addl $8,%esp
	movl %eax,%eax
	testl %eax,%eax
	je .L749
	jmp .L56
	.align 16
.L749:
.L748:
.L747:
	jmp .L578
	.align 16
.L348:
	cmpb $0,C
	jne .L750
	movzwl -2(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-13(%ebp)
	movsbw -13(%ebp),%ax
	addw %ax,-2(%ebp)
.L750:
	incw -2(%ebp)
	jmp .L578
	.align 16
.L350:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib+1(%eax)
	jne .L751
	movzwl -8(%ebp),%eax
	leal 1(%eax),%edx
	pushl %edx
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl %eax,%ebx
	sall $8,%ebx
	movl %ebx,-92(%ebp)
	jmp .L752
	.align 16
.L751:
	movzwl -8(%ebp),%eax
	movzbl memory+1(%eax),%ebx
	movl %ebx,-92(%ebp)
	sall $8,-92(%ebp)
.L752:
	movzbl -6(%ebp),%ebx
	movl %ebx,-96(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L753
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl -92(%ebp),%edx
	orl %eax,%edx
	movl -96(%ebp),%ecx
	addl %edx,%ecx
	movl %ecx,%eax
	jmp .L754
	.align 16
.L753:
	movzwl -8(%ebp),%edx
	movzbl memory(%edx),%ecx
	movl -92(%ebp),%edx
	orl %ecx,%edx
	movl -96(%ebp),%ecx
	addl %edx,%ecx
	movl %ecx,%eax
.L754:
	movw %ax,-8(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $0,attrib(%eax)
	jne .L755
	movzwl -8(%ebp),%eax
	movb -4(%ebp),%dl
	movb %dl,memory(%eax)
	jmp .L756
	.align 16
.L755:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L757
	movzbl -4(%ebp),%eax
	pushl %eax
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_PutByte
	addl $8,%esp
	movl %eax,%eax
	testl %eax,%eax
	je .L758
	jmp .L56
	.align 16
.L758:
.L757:
.L756:
	jmp .L578
	.align 16
.L356:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%dx
	movzbw -5(%ebp),%ax
	addw %ax,%dx
	movl %edx,%ebx
	movb $0,%bh
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb -6(%ebp),%dl
	movb %dl,memory(%eax)
	jmp .L578
	.align 16
.L358:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%dx
	movzbw -5(%ebp),%ax
	addw %ax,%dx
	movl %edx,%ebx
	movb $0,%bh
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb -4(%ebp),%dl
	movb %dl,memory(%eax)
	jmp .L578
	.align 16
.L360:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%dx
	movzbw -6(%ebp),%ax
	addw %ax,%dx
	movl %edx,%ebx
	movb $0,%bh
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $0,attrib(%eax)
	jne .L759
	movzwl -8(%ebp),%eax
	movb -5(%ebp),%dl
	movb %dl,memory(%eax)
	jmp .L760
	.align 16
.L759:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L761
	movzbl -5(%ebp),%eax
	pushl %eax
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_PutByte
	addl $8,%esp
	movl %eax,%eax
	testl %eax,%eax
	je .L762
	jmp .L56
	.align 16
.L762:
.L761:
.L760:
	jmp .L578
	.align 16
.L364:
	movb -6(%ebp),%al
	movb %al,-4(%ebp)
	movb %al,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L366:
	movzwl -2(%ebp),%eax
	movzbw -6(%ebp),%dx
#APP
	movw memory(%eax),%bx; addw %dx,%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $0,attrib(%eax)
	jne .L763
	movzwl -8(%ebp),%eax
	movb -4(%ebp),%dl
	movb %dl,memory(%eax)
	jmp .L764
	.align 16
.L763:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L765
	movzbl -4(%ebp),%eax
	pushl %eax
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_PutByte
	addl $8,%esp
	movl %eax,%eax
	testl %eax,%eax
	je .L766
	jmp .L56
	.align 16
.L766:
.L765:
.L764:
	jmp .L578
	.align 16
.L368:
	movb -5(%ebp),%al
	movb %al,-3(%ebp)
	jmp .L578
	.align 16
.L374:
	movzwl -2(%ebp),%eax
	movzbw -5(%ebp),%dx
#APP
	movw memory(%eax),%bx; addw %dx,%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $0,attrib(%eax)
	jne .L767
	movzwl -8(%ebp),%eax
	movb -4(%ebp),%dl
	movb %dl,memory(%eax)
	jmp .L768
	.align 16
.L767:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L769
	movzbl -4(%ebp),%eax
	pushl %eax
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_PutByte
	addl $8,%esp
	movl %eax,%eax
	testl %eax,%eax
	je .L770
	jmp .L56
	.align 16
.L770:
.L769:
.L768:
	jmp .L578
	.align 16
.L380:
	movzwl -2(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-6(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	incw -2(%ebp)
	jmp .L578
	.align 16
.L382:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%dx
	movzbw -5(%ebp),%ax
	movl %edx,%ebx
	addw %ax,%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib+1(%eax)
	jne .L771
	movzwl -8(%ebp),%eax
	leal 1(%eax),%edx
	pushl %edx
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl %eax,%ebx
	sall $8,%ebx
	movl %ebx,-100(%ebp)
	jmp .L772
	.align 16
.L771:
	movzwl -8(%ebp),%eax
	movzbl memory+1(%eax),%ebx
	movl %ebx,-100(%ebp)
	sall $8,-100(%ebp)
.L772:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L773
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl -100(%ebp),%edx
	orl %eax,%edx
	movl %edx,%eax
	jmp .L774
	.align 16
.L773:
	movzwl -8(%ebp),%edx
	movzbl memory(%edx),%ecx
	movl -100(%ebp),%edx
	orl %ecx,%edx
	movl %edx,%eax
.L774:
	movw %ax,-8(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L775
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L776
	.align 16
.L775:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L776:
	movb %al,%al
	movb %al,-4(%ebp)
	movb %al,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L384:
	movzwl -2(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-5(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	incw -2(%ebp)
	jmp .L578
	.align 16
.L386:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%dx
	movzbw -5(%ebp),%ax
	movl %edx,%ebx
	addw %ax,%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib+1(%eax)
	jne .L777
	movzwl -8(%ebp),%eax
	leal 1(%eax),%edx
	pushl %edx
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl %eax,%ebx
	sall $8,%ebx
	movl %ebx,-104(%ebp)
	jmp .L778
	.align 16
.L777:
	movzwl -8(%ebp),%eax
	movzbl memory+1(%eax),%ebx
	movl %ebx,-104(%ebp)
	sall $8,-104(%ebp)
.L778:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L779
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl -104(%ebp),%edx
	orl %eax,%edx
	movl %edx,%eax
	jmp .L780
	.align 16
.L779:
	movzwl -8(%ebp),%edx
	movzbl memory(%edx),%ecx
	movl -104(%ebp),%edx
	orl %ecx,%edx
	movl %edx,%eax
.L780:
	movw %ax,-8(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L781
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L782
	.align 16
.L781:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L782:
	movb %al,%al
	movb %al,-4(%ebp)
	movb %al,%al
	movb %al,-5(%ebp)
	movb %al,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L388:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-6(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L390:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L392:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-5(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L394:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L783
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L784
	.align 16
.L783:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L784:
	movb %al,%al
	movb %al,-4(%ebp)
	movb %al,%al
	movb %al,-5(%ebp)
	movb %al,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L396:
	movb -4(%ebp),%al
	movb %al,-6(%ebp)
	movb %al,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L398:
	movzwl -2(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	incw -2(%ebp)
	jmp .L578
	.align 16
.L400:
	movb -4(%ebp),%al
	movb %al,-5(%ebp)
	movb %al,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L404:
	movzwl -2(%ebp),%eax
#APP
	movw memory(%eax),%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L785
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L786
	.align 16
.L785:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L786:
	movb %al,%al
	movb %al,-6(%ebp)
	movb %al,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L406:
	movzwl -2(%ebp),%eax
#APP
	movw memory(%eax),%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L787
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L788
	.align 16
.L787:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L788:
	movb %al,%al
	movb %al,-4(%ebp)
	movb %al,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L408:
	movzwl -2(%ebp),%eax
#APP
	movw memory(%eax),%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L789
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L790
	.align 16
.L789:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L790:
	movb %al,%al
	movb %al,-5(%ebp)
	movb %al,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L410:
	movzwl -2(%ebp),%eax
#APP
	movw memory(%eax),%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L791
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L792
	.align 16
.L791:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L792:
	movb %al,%al
	movb %al,-4(%ebp)
	movb %al,%al
	movb %al,-5(%ebp)
	movb %al,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L412:
	cmpb $0,C
	je .L793
	movzwl -2(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-13(%ebp)
	movsbw -13(%ebp),%ax
	addw %ax,-2(%ebp)
.L793:
	incw -2(%ebp)
	jmp .L578
	.align 16
.L414:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib+1(%eax)
	jne .L794
	movzwl -8(%ebp),%eax
	leal 1(%eax),%edx
	pushl %edx
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl %eax,%ebx
	sall $8,%ebx
	movl %ebx,-108(%ebp)
	jmp .L795
	.align 16
.L794:
	movzwl -8(%ebp),%eax
	movzbl memory+1(%eax),%ebx
	movl %ebx,-108(%ebp)
	sall $8,-108(%ebp)
.L795:
	movzbl -6(%ebp),%ebx
	movl %ebx,-112(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L796
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl -108(%ebp),%edx
	orl %eax,%edx
	movl -112(%ebp),%ecx
	addl %edx,%ecx
	movl %ecx,%eax
	jmp .L797
	.align 16
.L796:
	movzwl -8(%ebp),%edx
	movzbl memory(%edx),%ecx
	movl -108(%ebp),%edx
	orl %ecx,%edx
	movl -112(%ebp),%ecx
	addl %edx,%ecx
	movl %ecx,%eax
.L797:
	movw %ax,-8(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L798
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L799
	.align 16
.L798:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L799:
	movb %al,%al
	movb %al,-4(%ebp)
	movb %al,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L418:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib+1(%eax)
	jne .L800
	movzwl -8(%ebp),%eax
	leal 1(%eax),%edx
	pushl %edx
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl %eax,%ebx
	sall $8,%ebx
	movl %ebx,-116(%ebp)
	jmp .L801
	.align 16
.L800:
	movzwl -8(%ebp),%eax
	movzbl memory+1(%eax),%ebx
	movl %ebx,-116(%ebp)
	sall $8,-116(%ebp)
.L801:
	movzbl -6(%ebp),%ebx
	movl %ebx,-120(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L802
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl -116(%ebp),%edx
	orl %eax,%edx
	movl -120(%ebp),%ecx
	addl %edx,%ecx
	movl %ecx,%eax
	jmp .L803
	.align 16
.L802:
	movzwl -8(%ebp),%edx
	movzbl memory(%edx),%ecx
	movl -116(%ebp),%edx
	orl %ecx,%edx
	movl -120(%ebp),%ecx
	addl %edx,%ecx
	movl %ecx,%eax
.L803:
	movw %ax,-8(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L804
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L805
	.align 16
.L804:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L805:
	movb %al,%al
	movb %al,-4(%ebp)
	movb %al,%al
	movb %al,-5(%ebp)
	movb %al,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L420:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%dx
	movzbw -5(%ebp),%ax
	addw %ax,%dx
	movl %edx,%ebx
	movb $0,%bh
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-6(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L422:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%dx
	movzbw -5(%ebp),%ax
	addw %ax,%dx
	movl %edx,%ebx
	movb $0,%bh
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-4(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L424:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%dx
	movzbw -6(%ebp),%ax
	addw %ax,%dx
	movl %edx,%ebx
	movb $0,%bh
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L806
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L807
	.align 16
.L806:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L807:
	movb %al,%al
	movb %al,-5(%ebp)
	movb %al,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L426:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%dx
	movzbw -6(%ebp),%ax
	addw %ax,%dx
	movl %edx,%ebx
	movb $0,%bh
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L808
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L809
	.align 16
.L808:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L809:
	movb %al,%al
	movb %al,-4(%ebp)
	movb %al,%al
	movb %al,-5(%ebp)
	movb %al,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L428:
	movb $0,V
	jmp .L578
	.align 16
.L430:
	movzwl -2(%ebp),%eax
	movzbw -6(%ebp),%dx
#APP
	movw memory(%eax),%bx; addw %dx,%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L810
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L811
	.align 16
.L810:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L811:
	movb %al,%al
	movb %al,-4(%ebp)
	movb %al,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L432:
	movb -3(%ebp),%al
	movb %al,-5(%ebp)
	movb %al,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L436:
	movzwl -2(%ebp),%eax
	movzbw -5(%ebp),%dx
#APP
	movw memory(%eax),%bx; addw %dx,%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L812
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L813
	.align 16
.L812:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L813:
	movb %al,%al
	movb %al,-6(%ebp)
	movb %al,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L438:
	movzwl -2(%ebp),%eax
	movzbw -5(%ebp),%dx
#APP
	movw memory(%eax),%bx; addw %dx,%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L814
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L815
	.align 16
.L814:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L815:
	movb %al,%al
	movb %al,-4(%ebp)
	movb %al,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L440:
	movzwl -2(%ebp),%eax
	movzbw -6(%ebp),%dx
#APP
	movw memory(%eax),%bx; addw %dx,%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L816
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L817
	.align 16
.L816:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L817:
	movb %al,%al
	movb %al,-5(%ebp)
	movb %al,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L442:
	movzwl -2(%ebp),%eax
	movzbw -6(%ebp),%dx
#APP
	movw memory(%eax),%bx; addw %dx,%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L818
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L819
	.align 16
.L818:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L819:
	movb %al,%al
	movb %al,-4(%ebp)
	movb %al,%al
	movb %al,-5(%ebp)
	movb %al,%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L444:
	movzwl -2(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	incw -2(%ebp)
	movb -6(%ebp),%al
	subb -9(%ebp),%al
	movb %al,%dl
	movb %dl,N
	movb %dl,Z
	movb -6(%ebp),%al
	cmpb %al,-9(%ebp)
	setbe %al
	movb %al,C
	jmp .L578
	.align 16
.L446:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%dx
	movzbw -5(%ebp),%ax
	movl %edx,%ebx
	addw %ax,%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib+1(%eax)
	jne .L820
	movzwl -8(%ebp),%eax
	leal 1(%eax),%edx
	pushl %edx
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl %eax,%ebx
	sall $8,%ebx
	movl %ebx,-124(%ebp)
	jmp .L821
	.align 16
.L820:
	movzwl -8(%ebp),%eax
	movzbl memory+1(%eax),%ebx
	movl %ebx,-124(%ebp)
	sall $8,-124(%ebp)
.L821:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L822
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl -124(%ebp),%edx
	orl %eax,%edx
	movl %edx,%eax
	jmp .L823
	.align 16
.L822:
	movzwl -8(%ebp),%edx
	movzbl memory(%edx),%ecx
	movl -124(%ebp),%edx
	orl %ecx,%edx
	movl %edx,%eax
.L823:
	movw %ax,-8(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L824
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L825
	.align 16
.L824:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L825:
	movb %al,-9(%ebp)
	movb -4(%ebp),%al
	subb -9(%ebp),%al
	movb %al,%dl
	movb %dl,N
	movb %dl,Z
	movb -4(%ebp),%al
	cmpb %al,-9(%ebp)
	setbe %al
	movb %al,C
	jmp .L578
	.align 16
.L448:
	incw -2(%ebp)
	jmp .L578
	.align 16
.L452:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	movb -6(%ebp),%al
	subb -9(%ebp),%al
	movb %al,%dl
	movb %dl,N
	movb %dl,Z
	movb -6(%ebp),%al
	cmpb %al,-9(%ebp)
	setbe %al
	movb %al,C
	jmp .L578
	.align 16
.L454:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	movb -4(%ebp),%al
	subb -9(%ebp),%al
	movb %al,%dl
	movb %dl,N
	movb %dl,Z
	movb -4(%ebp),%al
	cmpb %al,-9(%ebp)
	setbe %al
	movb %al,C
	jmp .L578
	.align 16
.L456:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	decb memory(%eax)
	movb memory(%eax),%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L460:
	incb -6(%ebp)
	movb -6(%ebp),%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L462:
	movzwl -2(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	incw -2(%ebp)
	movb -4(%ebp),%al
	subb -9(%ebp),%al
	movb %al,%dl
	movb %dl,N
	movb %dl,Z
	movb -4(%ebp),%al
	cmpb %al,-9(%ebp)
	setbe %al
	movb %al,C
	jmp .L578
	.align 16
.L464:
	decb -5(%ebp)
	movb -5(%ebp),%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L468:
	movzwl -2(%ebp),%eax
#APP
	movw memory(%eax),%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L826
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L827
	.align 16
.L826:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L827:
	movb %al,-9(%ebp)
	movb -6(%ebp),%al
	subb -9(%ebp),%al
	movb %al,%dl
	movb %dl,N
	movb %dl,Z
	movb -6(%ebp),%al
	cmpb %al,-9(%ebp)
	setbe %al
	movb %al,C
	jmp .L578
	.align 16
.L470:
	movzwl -2(%ebp),%eax
#APP
	movw memory(%eax),%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L828
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L829
	.align 16
.L828:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L829:
	movb %al,-9(%ebp)
	movb -4(%ebp),%al
	subb -9(%ebp),%al
	movb %al,%dl
	movb %dl,N
	movb %dl,Z
	movb -4(%ebp),%al
	cmpb %al,-9(%ebp)
	setbe %al
	movb %al,C
	jmp .L578
	.align 16
.L472:
	movzwl -2(%ebp),%eax
#APP
	movw memory(%eax),%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L830
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	decb %al
	jmp .L831
	.align 16
.L830:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
	decb %al
.L831:
	movb %al,%al
	movb %al,N
	movb %al,Z
	movzwl -8(%ebp),%eax
	cmpb $0,attrib(%eax)
	jne .L832
	movzwl -8(%ebp),%eax
	movb Z,%dl
	movb %dl,memory(%eax)
	jmp .L833
	.align 16
.L832:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L834
	movzbl Z,%eax
	pushl %eax
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_PutByte
	addl $8,%esp
	movl %eax,%eax
	testl %eax,%eax
	je .L835
	jmp .L56
	.align 16
.L835:
.L834:
.L833:
	jmp .L578
	.align 16
.L476:
	cmpb $0,Z
	je .L836
	movzwl -2(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-13(%ebp)
	movsbw -13(%ebp),%ax
	addw %ax,-2(%ebp)
.L836:
	incw -2(%ebp)
	jmp .L578
	.align 16
.L478:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib+1(%eax)
	jne .L837
	movzwl -8(%ebp),%eax
	leal 1(%eax),%edx
	pushl %edx
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl %eax,%ebx
	sall $8,%ebx
	movl %ebx,-128(%ebp)
	jmp .L838
	.align 16
.L837:
	movzwl -8(%ebp),%eax
	movzbl memory+1(%eax),%ebx
	movl %ebx,-128(%ebp)
	sall $8,-128(%ebp)
.L838:
	movzbl -6(%ebp),%ebx
	movl %ebx,-132(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L839
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl -128(%ebp),%edx
	orl %eax,%edx
	movl -132(%ebp),%ecx
	addl %edx,%ecx
	movl %ecx,%eax
	jmp .L840
	.align 16
.L839:
	movzwl -8(%ebp),%edx
	movzbl memory(%edx),%ecx
	movl -128(%ebp),%edx
	orl %ecx,%edx
	movl -132(%ebp),%ecx
	addl %edx,%ecx
	movl %ecx,%eax
.L840:
	movw %ax,-8(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L841
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L842
	.align 16
.L841:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L842:
	movb %al,-9(%ebp)
	movb -4(%ebp),%al
	subb -9(%ebp),%al
	movb %al,%dl
	movb %dl,N
	movb %dl,Z
	movb -4(%ebp),%al
	cmpb %al,-9(%ebp)
	setbe %al
	movb %al,C
	jmp .L578
	.align 16
.L484:
	incw -2(%ebp)
	jmp .L578
	.align 16
.L486:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%dx
	movzbw -5(%ebp),%ax
	addw %ax,%dx
	movl %edx,%ebx
	movb $0,%bh
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	movb -4(%ebp),%al
	subb -9(%ebp),%al
	movb %al,%dl
	movb %dl,N
	movb %dl,Z
	movb -4(%ebp),%al
	cmpb %al,-9(%ebp)
	setbe %al
	movb %al,C
	movb -4(%ebp),%al
	subb -9(%ebp),%al
	movb %al,%dl
	movb %dl,N
	movb %dl,Z
	movb -4(%ebp),%al
	cmpb %al,-9(%ebp)
	setbe %al
	movb %al,C
	jmp .L578
	.align 16
.L488:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%dx
	movzbw -5(%ebp),%ax
	addw %ax,%dx
	movl %edx,%ebx
	movb $0,%bh
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	decb memory(%eax)
	movb memory(%eax),%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L492:
	andb $247,regP
	jmp .L578
	.align 16
.L494:
	movzwl -2(%ebp),%eax
	movzbw -6(%ebp),%dx
#APP
	movw memory(%eax),%bx; addw %dx,%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L843
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L844
	.align 16
.L843:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L844:
	movb %al,-9(%ebp)
	movb -4(%ebp),%al
	subb -9(%ebp),%al
	movb %al,%dl
	movb %dl,N
	movb %dl,Z
	movb -4(%ebp),%al
	cmpb %al,-9(%ebp)
	setbe %al
	movb %al,C
	jmp .L578
	.align 16
.L496:
	jmp .L578
	.align 16
.L500:
	addw $2,-2(%ebp)
	jmp .L578
	.align 16
.L502:
	movzwl -2(%ebp),%eax
	movzbw -5(%ebp),%dx
#APP
	movw memory(%eax),%bx; addw %dx,%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L845
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L846
	.align 16
.L845:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L846:
	movb %al,-9(%ebp)
	movb -4(%ebp),%al
	subb -9(%ebp),%al
	movb %al,%dl
	movb %dl,N
	movb %dl,Z
	movb -4(%ebp),%al
	cmpb %al,-9(%ebp)
	setbe %al
	movb %al,C
	jmp .L578
	.align 16
.L504:
	movzwl -2(%ebp),%eax
	movzbw -5(%ebp),%dx
#APP
	movw memory(%eax),%bx; addw %dx,%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L847
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	decb %al
	jmp .L848
	.align 16
.L847:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
	decb %al
.L848:
	movb %al,%al
	movb %al,N
	movb %al,Z
	movzwl -8(%ebp),%eax
	cmpb $0,attrib(%eax)
	jne .L849
	movzwl -8(%ebp),%eax
	movb Z,%dl
	movb %dl,memory(%eax)
	jmp .L850
	.align 16
.L849:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L851
	movzbl Z,%eax
	pushl %eax
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_PutByte
	addl $8,%esp
	movl %eax,%eax
	testl %eax,%eax
	je .L852
	jmp .L56
	.align 16
.L852:
.L851:
.L850:
	jmp .L578
	.align 16
.L508:
	movzwl -2(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	incw -2(%ebp)
	movb -5(%ebp),%al
	subb -9(%ebp),%al
	movb %al,%dl
	movb %dl,N
	movb %dl,Z
	movb -5(%ebp),%al
	cmpb %al,-9(%ebp)
	setbe %al
	movb %al,C
	jmp .L578
	.align 16
.L510:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%dx
	movzbw -5(%ebp),%ax
	movl %edx,%ebx
	addw %ax,%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib+1(%eax)
	jne .L853
	movzwl -8(%ebp),%eax
	leal 1(%eax),%edx
	pushl %edx
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl %eax,%ebx
	sall $8,%ebx
	movl %ebx,-136(%ebp)
	jmp .L854
	.align 16
.L853:
	movzwl -8(%ebp),%eax
	movzbl memory+1(%eax),%ebx
	movl %ebx,-136(%ebp)
	sall $8,-136(%ebp)
.L854:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L855
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl -136(%ebp),%edx
	orl %eax,%edx
	movl %edx,%eax
	jmp .L856
	.align 16
.L855:
	movzwl -8(%ebp),%edx
	movzbl memory(%edx),%ecx
	movl -136(%ebp),%edx
	orl %ecx,%edx
	movl %edx,%eax
.L856:
	movw %ax,-8(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L857
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L858
	.align 16
.L857:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L858:
	movb %al,-9(%ebp)
	jmp .L859
	.align 16
.L512:
	incw -2(%ebp)
	jmp .L578
	.align 16
.L516:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	movb -5(%ebp),%al
	subb -9(%ebp),%al
	movb %al,%dl
	movb %dl,N
	movb %dl,Z
	movb -5(%ebp),%al
	cmpb %al,-9(%ebp)
	setbe %al
	movb %al,C
	jmp .L578
	.align 16
.L518:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	jmp .L859
	.align 16
.L520:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	incb memory(%eax)
	movb memory(%eax),%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L524:
	incb -5(%ebp)
	movb -5(%ebp),%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L526:
	movzwl -2(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	incw -2(%ebp)
	jmp .L859
	.align 16
.L528:
	jmp .L578
	.align 16
.L532:
	movzwl -2(%ebp),%eax
#APP
	movw memory(%eax),%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L860
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L861
	.align 16
.L860:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L861:
	movb %al,-9(%ebp)
	movb -5(%ebp),%al
	subb -9(%ebp),%al
	movb %al,%dl
	movb %dl,N
	movb %dl,Z
	movb -5(%ebp),%al
	cmpb %al,-9(%ebp)
	setbe %al
	movb %al,C
	jmp .L578
	.align 16
.L534:
	movzwl -2(%ebp),%eax
#APP
	movw memory(%eax),%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L862
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L863
	.align 16
.L862:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L863:
	movb %al,-9(%ebp)
	jmp .L859
	.align 16
.L536:
	movzwl -2(%ebp),%eax
#APP
	movw memory(%eax),%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L864
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	incb %al
	jmp .L865
	.align 16
.L864:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
	incb %al
.L865:
	movb %al,%al
	movb %al,N
	movb %al,Z
	movzwl -8(%ebp),%eax
	cmpb $0,attrib(%eax)
	jne .L866
	movzwl -8(%ebp),%eax
	movb Z,%dl
	movb %dl,memory(%eax)
	jmp .L867
	.align 16
.L866:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L868
	movzbl Z,%eax
	pushl %eax
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_PutByte
	addl $8,%esp
	movl %eax,%eax
	testl %eax,%eax
	je .L869
	jmp .L56
	.align 16
.L869:
.L868:
.L867:
	jmp .L578
	.align 16
.L540:
	cmpb $0,Z
	jne .L870
	movzwl -2(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-13(%ebp)
	movsbw -13(%ebp),%ax
	addw %ax,-2(%ebp)
.L870:
	incw -2(%ebp)
	jmp .L578
	.align 16
.L542:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%bx
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib+1(%eax)
	jne .L871
	movzwl -8(%ebp),%eax
	leal 1(%eax),%edx
	pushl %edx
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl %eax,%ebx
	sall $8,%ebx
	movl %ebx,-140(%ebp)
	jmp .L872
	.align 16
.L871:
	movzwl -8(%ebp),%eax
	movzbl memory+1(%eax),%ebx
	movl %ebx,-140(%ebp)
	sall $8,-140(%ebp)
.L872:
	movzbl -6(%ebp),%ebx
	movl %ebx,-144(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L873
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%eax
	movl -140(%ebp),%edx
	orl %eax,%edx
	movl -144(%ebp),%ecx
	addl %edx,%ecx
	movl %ecx,%eax
	jmp .L874
	.align 16
.L873:
	movzwl -8(%ebp),%edx
	movzbl memory(%edx),%ecx
	movl -140(%ebp),%edx
	orl %ecx,%edx
	movl -144(%ebp),%ecx
	addl %edx,%ecx
	movl %ecx,%eax
.L874:
	movw %ax,-8(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L875
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L876
	.align 16
.L875:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L876:
	movb %al,-9(%ebp)
	jmp .L859
	.align 16
.L548:
	incw -2(%ebp)
	jmp .L578
	.align 16
.L550:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%dx
	movzbw -5(%ebp),%ax
	addw %ax,%dx
	movl %edx,%ebx
	movb $0,%bh
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	jmp .L859
	.align 16
.L552:
	movzwl -2(%ebp),%eax
	movzbw memory(%eax),%dx
	movzbw -5(%ebp),%ax
	addw %ax,%dx
	movl %edx,%ebx
	movb $0,%bh
	movw %bx,-8(%ebp)
	incw -2(%ebp)
	movzwl -8(%ebp),%eax
	incb memory(%eax)
	movb memory(%eax),%al
	movb %al,N
	movb %al,Z
	jmp .L578
	.align 16
.L556:
	orb $8,regP
	jmp .L578
	.align 16
.L558:
	movzwl -2(%ebp),%eax
	movzbw -6(%ebp),%dx
#APP
	movw memory(%eax),%bx; addw %dx,%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L877
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L878
	.align 16
.L877:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L878:
	movb %al,-9(%ebp)
	jmp .L859
	.align 16
.L560:
	jmp .L578
	.align 16
.L564:
	addw $2,-2(%ebp)
	jmp .L578
	.align 16
.L566:
	movzwl -2(%ebp),%eax
	movzbw -5(%ebp),%dx
#APP
	movw memory(%eax),%bx; addw %dx,%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L879
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	jmp .L880
	.align 16
.L879:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
.L880:
	movb %al,-9(%ebp)
	jmp .L859
	.align 16
.L568:
	movzwl -2(%ebp),%eax
	movzbw -5(%ebp),%dx
#APP
	movw memory(%eax),%bx; addw %dx,%bx
#NO_APP
	movw %bx,-8(%ebp)
	addw $2,-2(%ebp)
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L881
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_GetByte
	addl $4,%esp
	movl %eax,%edx
	movb %dl,%al
	incb %al
	jmp .L882
	.align 16
.L881:
	movzwl -8(%ebp),%edx
	movb memory(%edx),%al
	incb %al
.L882:
	movb %al,%al
	movb %al,N
	movb %al,Z
	movzwl -8(%ebp),%eax
	cmpb $0,attrib(%eax)
	jne .L883
	movzwl -8(%ebp),%eax
	movb Z,%dl
	movb %dl,memory(%eax)
	jmp .L884
	.align 16
.L883:
	movzwl -8(%ebp),%eax
	cmpb $2,attrib(%eax)
	jne .L885
	movzbl Z,%eax
	pushl %eax
	movzwl -8(%ebp),%eax
	pushl %eax
	call Atari800_PutByte
	addl $8,%esp
	movl %eax,%eax
	testl %eax,%eax
	je .L886
	jmp .L56
	.align 16
.L886:
.L885:
.L884:
	jmp .L578
	.align 16
.L480:
	movzwl -2(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	incw -2(%ebp)
	movw -2(%ebp),%ax
	movw %ax,regPC
	movb -3(%ebp),%al
	movb %al,regS
	movb -4(%ebp),%al
	movb %al,regA
	movb -5(%ebp),%al
	movb %al,regX
	movb -6(%ebp),%al
	movb %al,regY
	call CPU_GetStatus
	movzbl -9(%ebp),%eax
	pushl %eax
	call Escape
	addl $4,%esp
	call CPU_PutStatus
	movw regPC,%ax
	movw %ax,-2(%ebp)
	movb regS,%al
	movb %al,-3(%ebp)
	movb regA,%al
	movb %al,-4(%ebp)
	movb regX,%al
	movb %al,-5(%ebp)
	movb regY,%al
	movb %al,-6(%ebp)
	incb -3(%ebp)
	movzbl -3(%ebp),%eax
	movb memory+256(%eax),%dl
	movb %dl,-9(%ebp)
	incb -3(%ebp)
	movzbl -3(%ebp),%eax
	movzbw memory+256(%eax),%dx
	movl %edx,%eax
	salw $8,%ax
	movzbw -9(%ebp),%dx
	orw %dx,%ax
	movl %eax,%ebx
	incw %bx
	movw %bx,-2(%ebp)
	jmp .L578
	.align 16
.L544:
	nop
.L570:
	movzwl -2(%ebp),%eax
	movb memory(%eax),%dl
	movb %dl,-9(%ebp)
	incw -2(%ebp)
	movw -2(%ebp),%ax
	movw %ax,regPC
	movb -3(%ebp),%al
	movb %al,regS
	movb -4(%ebp),%al
	movb %al,regA
	movb -5(%ebp),%al
	movb %al,regX
	movb -6(%ebp),%al
	movb %al,regY
	call CPU_GetStatus
	movzbl -9(%ebp),%eax
	pushl %eax
	call Escape
	addl $4,%esp
	call CPU_PutStatus
	movw regPC,%ax
	movw %ax,-2(%ebp)
	movb regS,%al
	movb %al,-3(%ebp)
	movb regA,%al
	movb %al,-4(%ebp)
	movb regX,%al
	movb %al,-5(%ebp)
	movb regY,%al
	movb %al,-6(%ebp)
	jmp .L578
	.align 16
.L64:
	nop
.L66:
	nop
.L74:
	nop
.L82:
	nop
.L90:
	nop
.L96:
	nop
.L98:
	nop
.L106:
	nop
.L114:
	nop
.L122:
	nop
.L128:
	nop
.L130:
	nop
.L138:
	nop
.L146:
	nop
.L154:
	nop
.L160:
	nop
.L162:
	nop
.L170:
	nop
.L178:
	nop
.L186:
	nop
.L192:
	nop
.L194:
	nop
.L202:
	nop
.L210:
	nop
.L218:
	nop
.L224:
	nop
.L226:
	nop
.L234:
	nop
.L242:
	nop
.L250:
	nop
.L256:
	nop
.L258:
	nop
.L266:
	nop
.L274:
	nop
.L282:
	nop
.L288:
	nop
.L290:
	nop
.L298:
	nop
.L306:
	nop
.L314:
	nop
.L322:
	nop
.L330:
	nop
.L338:
	nop
.L346:
	nop
.L352:
	nop
.L354:
	nop
.L362:
	nop
.L370:
	nop
.L372:
	nop
.L376:
	nop
.L378:
	nop
.L402:
	nop
.L416:
	nop
.L434:
	nop
.L450:
	nop
.L458:
	nop
.L466:
	nop
.L474:
	nop
.L482:
	nop
.L490:
	nop
.L498:
	nop
.L506:
	nop
.L514:
	nop
.L522:
	nop
.L530:
	nop
.L538:
	nop
.L546:
	nop
.L554:
	nop
.L562:
	movw -2(%ebp),%ax
	movw %ax,regPC
	movb -3(%ebp),%al
	movb %al,regS
	movb -4(%ebp),%al
	movb %al,regA
	movb -5(%ebp),%al
	movb %al,regX
	movb -6(%ebp),%al
	movb %al,regY
	movzwl -2(%ebp),%eax
	leal -1(%eax),%edx
	pushl %edx
	movzwl -2(%ebp),%eax
	movzbl memory-1(%eax),%edx
	pushl %edx
	pushl $.LC1
	pushl $_IO_stderr_
	call fprintf
	addl $16,%esp
	movl $1,8(%ebp)
	jmp .L56
	.align 16
.L694:
	movb regP,%al
	andb $8,%al
	testb %al,%al
	jne .L887
	movzbw -4(%ebp),%ax
	movzbw -9(%ebp),%dx
	addw %dx,%ax
	movzbw C,%dx
	addw %dx,%ax
	movl %eax,%edx
	movw %dx,-12(%ebp)
	movb %dl,%al
	movb %al,N
	movb %al,Z
	movw -12(%ebp),%ax
	shrw $8,%ax
	movb %al,C
	movb Z,%al
	xorb -4(%ebp),%al
	movb %al,%bl
	andb $128,%bl
	movb %bl,V
	movb Z,%al
	movb %al,-4(%ebp)
	jmp .L888
	.align 16
.L887:
	movzbl -4(%ebp),%eax
	movzbl BCDtoDEC(%eax),%ebx
	movl %ebx,-20(%ebp)
	movzbl -9(%ebp),%eax
	movzbl BCDtoDEC(%eax),%ebx
	movl %ebx,-24(%ebp)
	movzbl C,%eax
	movl %eax,%edx
	addl -24(%ebp),%edx
	addl %edx,-20(%ebp)
	movl -20(%ebp),%eax
	addl $DECtoBCD,%eax
	movb (%eax),%dl
	movb %dl,N
	movb %dl,Z
	cmpl $99,-20(%ebp)
	setg %al
	movb %al,C
	movb Z,%al
	xorb -4(%ebp),%al
	movb %al,%bl
	andb $128,%bl
	movb %bl,V
	movb Z,%al
	movb %al,-4(%ebp)
.L888:
	jmp .L578
	.align 16
.L859:
	movb regP,%al
	andb $8,%al
	testb %al,%al
	jne .L889
	movzbl -4(%ebp),%ebx
	movl %ebx,-148(%ebp)
	movzbl -9(%ebp),%eax
	subl %eax,-148(%ebp)
	cmpb $0,C
	jne .L890
	movl -148(%ebp),%edx
	decl %edx
	movl %edx,%eax
	jmp .L891
	.align 16
.L890:
	movw -148(%ebp),%ax
.L891:
	movw %ax,-12(%ebp)
	movb -12(%ebp),%al
	movb %al,N
	movb %al,Z
	movzbl -9(%ebp),%ebx
	movl %ebx,-152(%ebp)
	movzbl -4(%ebp),%ebx
	movl %ebx,-156(%ebp)
	cmpb $0,C
	jne .L892
	movl -152(%ebp),%eax
	incl %eax
	cmpl %eax,-156(%ebp)
	setge %al
	jmp .L893
	.align 16
.L892:
	movl -152(%ebp),%ebx
	cmpl %ebx,-156(%ebp)
	setge %al
.L893:
	movb %al,C
	movb Z,%al
	xorb -4(%ebp),%al
	movb %al,%bl
	andb $128,%bl
	movb %bl,V
	movb Z,%al
	movb %al,-4(%ebp)
	jmp .L894
	.align 16
.L889:
	movzbl -4(%ebp),%eax
	movzbl BCDtoDEC(%eax),%ebx
	movl %ebx,-24(%ebp)
	movzbl -9(%ebp),%eax
	movzbl BCDtoDEC(%eax),%ebx
	movl %ebx,-20(%ebp)
	movl -24(%ebp),%ebx
	subl -20(%ebp),%ebx
	movl %ebx,-160(%ebp)
	movl -160(%ebp),%eax
	cmpb $0,C
	jne .L895
	decl %eax
.L895:
	movl %eax,-24(%ebp)
	cmpl $0,-24(%ebp)
	jge .L896
	movl -24(%ebp),%eax
	negl %eax
	movl $100,%ebx
	subl %eax,%ebx
	movl %ebx,-24(%ebp)
.L896:
	movl -24(%ebp),%eax
	addl $DECtoBCD,%eax
	movb (%eax),%dl
	movb %dl,N
	movb %dl,Z
	movzbl -9(%ebp),%ebx
	movl %ebx,-164(%ebp)
	movzbl -4(%ebp),%ebx
	movl %ebx,-168(%ebp)
	cmpb $0,C
	jne .L897
	movl -164(%ebp),%eax
	incl %eax
	cmpl %eax,-168(%ebp)
	setge %al
	jmp .L898
	.align 16
.L897:
	movl -164(%ebp),%ebx
	cmpl %ebx,-168(%ebp)
	setge %al
.L898:
	movb %al,C
	movb Z,%al
	xorb -4(%ebp),%al
	movb %al,%bl
	andb $128,%bl
	movb %bl,V
	movb Z,%al
	movb %al,-4(%ebp)
.L894:
	jmp .L578
	.align 16
.L578:
	jmp .L55
	.align 16
	jmp .L55
	.align 16
.L56:
	movw -2(%ebp),%ax
	movw %ax,regPC
	movb -3(%ebp),%al
	movb %al,regS
	movb -4(%ebp),%al
	movb %al,regA
	movb -5(%ebp),%al
	movb %al,regX
	movb -6(%ebp),%al
	movb %al,regY
.L48:
	leal -184(%ebp),%esp
	popl %ebx
	popl %esi
	popl %edi
	movl %ebp,%esp
	popl %ebp
	ret
.Lfe8:
	.size	 GO,.Lfe8-GO
	.comm	regPC,2,2
	.comm	regA,1,1
	.comm	regP,1,1
	.comm	regS,1,1
	.comm	regY,1,1
	.comm	regX,1,1
	.comm	memory,65536,1
	.comm	IRQ,4,4
	.local	N
	.comm	N,1,1
	.local	Z
	.comm	Z,1,1
	.local	V
	.comm	V,1,1
	.local	C
	.comm	C,1,1
	.comm	count,1024,4
	.local	attrib
	.comm	attrib,65536,1
	.comm	BCDtoDEC,256,1
	.comm	DECtoBCD,256,1
	.ident	"GCC: (GNU) 2.7.2"
