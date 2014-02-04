#  This file is part of par2cmdline (a PAR 2.0 compatible file verification and
#  repair tool). See http://parchive.sourceforge.net for details of PAR 2.0.
#
#  Copyright (c) 2007-2008 Vincent Tan.
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
# reedsolomon-x86_64-scalar.s
#

#
# void rs_process_x86_64_scalar(void* dst, const void* src, size_t size, const u32* LH);
#
	push	%r8
	push	%rbp

#	push	%rsi
#	push	%rdi

#	push	%rdx
#	push	%rcx
	push	%rbx
#	push	%rax

	mov		%rcx, %r8				# LH
	mov		%rdx, %rcx				# cnt
	add		%rcx, %rdi				# dst-end
	add		%rcx, %rsi				# src-end
	neg		%rcx					# -cnt

	.align	4
loop:
#  do {
#    u32 s = *src++;
	mov		0(%rsi, %rcx, 1), %edx

    // Use the two lookup tables computed earlier
#	u16 sw = s >> 16;
	mov		%edx, %eax 
	shr		$0x10, %eax
#    u32 d  = (L+256)[u8(sw >> 0)]; // use pre-shifted entries
#        d ^= (H+256)[u8(sw >> 8)]; // use pre-shifted entries
#        d ^= *dst ^ (L[u8(       s  >>  0)]      )
#                  ^ (H[u8(((u16) s) >>  8)]      )
#                  ; // <- one shift instruction eliminated
	movzx	%ah, %ebx
	mov		0x0400(%r8, %rbx, 4), %ebx		# H[u8(sw >> 8)]
	shl		$16, %ebx
	movzx	%dh, %ebp 
	xor		0x0400(%r8, %rbp, 4), %ebx		# H[u8(s >> 8)]
	movzx	%al, %eax
	mov		0x0000(%r8, %rax, 4), %ebp		# L[u8(sw >> 0)]
	shl		$16, %ebp
	xor		%ebp, %ebx
	mov		0(%rdi, %rcx, 1), %ebp
	movzx	%dl, %edx
	xor		0x0000(%r8, %rdx, 4), %ebx		# L[u8(s >> 0)]
	xor		%ebp, %ebx
	mov		%ebx, 0(%rdi, %rcx, 1)
#    dst++;
	add		$4, %rcx
#  } while (src < end);
	jnz		loop
#
# end of loop: restore stack/regs, exit
#
#	pop		%rax
	pop		%rbx
#	pop		%rcx
#	pop		%rdx
#	pop		%rdi
#	pop		%rsi
	pop		%rbp
	pop		%r8
	ret
