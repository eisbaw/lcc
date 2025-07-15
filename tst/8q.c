.bss
.align 4
.lcomm .LC2,4
.data
.align 4
.LC3:
.long 0
.align 4
.LC4:
.space 4
.long 0
.data
.align 4
.LC1:
.long 0
.long 1
.long .LC2
.long .LC4
.long .LC3
.long 0
.text
.ident "LCC: 4.2"
