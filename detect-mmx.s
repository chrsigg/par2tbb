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
#  detect-mmx.s
#
    .globl detect_mmx
    .text
detect_mmx:
	push	%ebx

    pushf                                 # try to flip "ID" bit in eflags
    pop     %eax
    btc     $21, %eax
    push    %eax
    popf 
    pushf 
    pop     %ecx
    xor     %eax, %ecx                    # bit 21 == 0 if "ID" bit changed
    xor     %eax, %eax
    bt      $21, %ecx
    jc      fset                          # jmp= no CPUID, no advanced opcodes
    cpuid                                 # eax= highest supported CPUID function
    test    %eax, %eax
    jz      fset                          # jmp= can't get features
    mov     $1, %eax                      # get features
    cpuid
    xor     %eax, %eax
    test    $1, %edx                      # floating point?
    jz      fset                          # jmp= no, so no MMX
    bt      $23, %edx                     # MMX support?
    jnc     fset                          # jmp= nope
    inc     %eax                          # indicate MMX
fset:
    pop     %ebx
    ret
