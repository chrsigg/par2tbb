#  This file is part of par2cmdline (a PAR 2.0 compatible file verification and
#  repair tool). See http://parchive.sourceforge.net for details of PAR 2.0.
#
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
# reedsolomon-x86_64-scalar-darwin.s
#
# void rs_process_x86_64_scalar(void* dst, const void* src, size_t size, const u32* LH);
#
	.private_extern _rs_process_x86_64_scalar
	.text
_rs_process_x86_64_scalar:

#include "reedsolomon-x86_64-scalar.s"
