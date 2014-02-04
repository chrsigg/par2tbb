#  This file is part of par2cmdline (a PAR 2.0 compatible file verification and
#  repair tool). See http://parchive.sourceforge.net for details of PAR 2.0.
#
#  Based on code by Paul Houle (paulhoule.com) March 22, 2008.
#  Copyright (c) 2008 Paul Houle
#  Copyright (c) 2008 Vincent Tan.
#
#  par2cmdline is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  par2cmdline is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
#  Modifications for concurrent processing, Unicode support, and hierarchial
#  directory support are Copyright (c) 2007-2008 Vincent Tan.
#  Search for "#if WANT_CONCURRENT" for concurrent code.
#  Concurrent processing utilises Intel Thread Building Blocks 2.0,
#  Copyright (c) 2007 Intel Corp.
#

#
# reedsolomon-i686-mmx.s
#

#
# void rs_process_i686_mmx(void* dst, const void* src, size_t size, unsigned* LH);
#
	push		%ebp
	push		%esi
	push		%edi
	push		%ebx

	mov			0x14(%esp), %edi				# destination
	mov			0x18(%esp), %esi				# source
	mov			0x1C(%esp), %ecx				# number of bytes to process (multiple of 8)
	mov			0x20(%esp), %ebp				# combined multiplication table

	mov			(%esi), %edx					# load 1st 8 source bytes
	movd		4(%esi), %mm4

	sub			$8, %ecx						# reduce # of loop iterations by 1
	jz			last8
	add			%ecx, %esi						# point to last set of 8-bytes of input
	add			%ecx, %edi						# point to last set of 8-bytes of output
	neg			%ecx							# convert byte size to count-up

	.align	4
loop:
	movzx		%dl, %eax
	movzx		%dh, %ebx
	movd		0x0000(%ebp, %eax, 4), %mm0
	shr			$16, %edx
	movd		0x0400(%ebp, %ebx, 4), %mm1
	movzx		%dl, %eax
	movq		0(%edi, %ecx, 1), %mm5
	movzx		%dh, %ebx
	movd		0x0000(%ebp, %eax, 4), %mm2
	movd		%mm4, %edx
	movq		8(%esi, %ecx, 1), %mm4			# read-ahead next 8 source bytes
	movzx		%dl, %eax
	movd		0x0400(%ebp, %ebx, 4), %mm3
	movzx		%dh, %ebx
	shr			$16, %edx
	punpckldq	0x0000(%ebp, %eax, 4), %mm0
	movzx		%dl, %eax
	punpckldq	0x0400(%ebp, %ebx, 4), %mm1
	movzx		%dh, %ebx
	punpckldq	0x0000(%ebp, %eax, 4), %mm2
	pxor		%mm0, %mm1
	punpckldq	0x0400(%ebp, %ebx, 4), %mm3
	movd		%mm4, %edx						# prepare src bytes 3-0 for next loop
	pxor		%mm5, %mm1
	pxor		%mm2, %mm3
	psllq		$16, %mm3
	psrlq		$32, %mm4						# align src bytes 7-4 for next loop
	pxor		%mm3, %mm1
	movq		%mm1, 0(%edi, %ecx, 1)

	add			$8, %ecx
	jnz			loop

	#
	# handle final iteration separately (so that a read beyond the end of the input/output buffer is avoided)
	#
last8:
	movzx		%dl, %eax
	movzx		%dh, %ebx
	movd		0x0000(%ebp, %eax, 4), %mm0
	shr			$16, %edx
	movd		0x0400(%ebp, %ebx, 4), %mm1
	movzx		%dl, %eax
	movq		0(%edi, %ecx, 1), %mm5
	movzx		%dh, %ebx
	movd		0x0000(%ebp, %eax, 4), %mm2
	movd		%mm4, %edx
#	movq		8(%esi, %ecx, 1), %mm4			# read-ahead next 8 source bytes
	movzx		%dl, %eax
	movd		0x0400(%ebp, %ebx, 4), %mm3
	movzx		%dh, %ebx
	shr			$16, %edx
	punpckldq	0x0000(%ebp, %eax, 4), %mm0
	movzx		%dl, %eax
	punpckldq	0x0400(%ebp, %ebx, 4), %mm1
	movzx		%dh, %ebx
	punpckldq	0x0000(%ebp, %eax, 4), %mm2
	pxor		%mm0, %mm1
	punpckldq	0x0400(%ebp, %ebx, 4), %mm3
#	movd		%mm4, %edx						# prepare src bytes 3-0 for next loop
	pxor		%mm5, %mm1
	pxor		%mm2, %mm3
	psllq		$16, %mm3
#	psrlq		$32, %mm4						# align src bytes 7-4 for next loop
	pxor		%mm3, %mm1
	movq		%mm1, 0(%edi, %ecx, 1)

	#
	# done: exit MMX mode, restore regs/stack, exit
	#
	emms
	pop			%ebx
	pop			%edi
	pop			%esi
	pop			%ebp
	ret
