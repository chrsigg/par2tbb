##  This file is part of par2cmdline (a PAR 2.0 compatible file verification and
##  repair tool). See http://parchive.sourceforge.net for details of PAR 2.0.
##
##  Copyright (c) 2003 Peter Brian Clements
##  Modifications for specific OS support (Mac, Linux, FreeBSD), and x86/x86_64
##  assembly code building (c) 2008 Vincent Tan.
##
##  par2cmdline is free software; you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation; either version 2 of the License, or
##  (at your option) any later version.
##
##  par2cmdline is distributed in the hope that it will be useful,
##  but WITHOUT ANY WARRANTY; without even the implied warranty of
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
##  GNU General Public License for more details.
##
##  You should have received a copy of the GNU General Public License
##  along with this program; if not, write to the Free Software
##  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

bin_PROGRAMS = par2

if AMD64
ARCH_SCALAR = x86_64
ARCH_MMX = x86_64
endif
if X86MMX
ARCH_SCALAR = i386
ARCH_MMX = i686
else
if X86CPU
ARCH_SCALAR = i386
ARCH_MMX = i686
endif
endif
if PPC64
ARCH_SCALAR = ppc64
endif
if PPC32
ARCH_SCALAR = ppc
endif

if PLATFORM_DARWIN
PLATFORM = darwin
FLAGS_ARCH = -arch $(ARCH_SCALAR)
ASMSOURCES_PLATFORM = 
CXXFLAGS_DARWIN =                                 #  because the functions in the .s files process RS data
#CXXFLAGS_DARWIN = -fomit-frame-pointer           <= if you want GCC to produce slightly better scalar x86 code
#CXXFLAGS_DARWIN = -mmmx -fomit-frame-pointer     <= if you want GCC to produce SIMD code using instrinsic fns
else
PLATFORM = posix
FLAGS_ARCH =
ASMSOURCES_PLATFORM = detect-mmx.s
endif

if AMD64
ASMSOURCES = reedsolomon-$(ARCH_SCALAR)-scalar-$(PLATFORM).s reedsolomon-$(ARCH_MMX)-mmx-$(PLATFORM).s
endif
if X86CPU
ASMSOURCES = reedsolomon-$(ARCH_SCALAR)-scalar-$(PLATFORM).s reedsolomon-$(ARCH_MMX)-mmx-$(PLATFORM).s $(ASMSOURCES_PLATFORM)
endif

if GPGPU_CUDA
AM_CXXFLAGS_GPGPU = # -I/usr/local/cuda/include
AM_LDFLAGS_GPGPU = -L/usr/local/cuda/lib -lcudart
if PLATFORM_DARWIN
if X86CPU
#par2_DEPENDENCIES = /Developer/CUDA/lib/libpar2_cuda.dylib
AM_LDFLAGS_GPGPU += -Wl,-rpath -Wl,/usr/local/cuda/lib -L/Developer/CUDA/lib -lpar2_cuda
GPGPU_SOURCES = cuda.cpp cuda.h
else
# ERROR: Darwin builds of par2 only supports CUDA on the i686 architecture (because CUDA ships as a 32-bit dylib that requires 10.5.2)
endif
endif
else
AM_CXXFLAGS_GPGPU =
AM_LDFLAGS_GPGPU =
GPGPU_SOURCES = 
endif

.s.o:
	$(CCASCOMPILE) $(FLAGS_ARCH) -c -o $@ $<

par2_SOURCES = par2cmdline.cpp par2cmdline.h \
	buffer.cpp buffer.h \
	commandline.cpp commandline.h \
	crc.cpp crc.h \
	creatorpacket.cpp creatorpacket.h \
	criticalpacket.cpp criticalpacket.h \
	datablock.cpp datablock.h \
	descriptionpacket.cpp descriptionpacket.h \
	diskfile.cpp diskfile.h \
	filechecksummer.cpp filechecksummer.h \
	galois.cpp galois.h \
	letype.h \
	mainpacket.cpp mainpacket.h \
	md5.cpp md5.h \
	par1fileformat.cpp par1fileformat.h \
	par1repairer.cpp par1repairer.h \
	par1repairersourcefile.cpp par1repairersourcefile.h \
	par2creator.cpp par2creator.h \
	par2creatorsourcefile.cpp par2creatorsourcefile.h \
	par2fileformat.cpp par2fileformat.h \
	par2repairer.cpp par2repairer.h \
	par2repairersourcefile.cpp par2repairersourcefile.h \
	pipeline.cpp pipeline.h \
	recoverypacket.cpp recoverypacket.h \
	reedsolomon.cpp reedsolomon.h \
	verificationhashtable.cpp verificationhashtable.h \
	verificationpacket.cpp verificationpacket.h \
	$(ASMSOURCES) $(GPGPU_SOURCES)

LDADD = -lstdc++ -ltbb -lrt -L.
if PLATFORM_DARWIN
AM_CXXFLAGS = -Wall -I$(top_srcdir)/../tbb22_20090809oss_src/include -gfull -O3 -fvisibility=hidden -fvisibility-inlines-hidden $(CXXFLAGS_DARWIN) $(AM_CXXFLAGS_GPGPU) $(FLAGS_ARCH)
if AMD64
# AM_CXXFLAGS += -mmacosx-version-min=10.5 -isysroot /Developer/SDKs/MacOSX10.5.sdk
AM_LDFLAGS = # -mmacosx-version-min=10.5
endif
if X86CPU
if GPGPU_CUDA    # CUDA mandates 10.5 so the two are mutally exclusive: either CUDA or 10.4u support but not both
AM_CXXFLAGS += -mmacosx-version-min=10.5 -isysroot /Developer/SDKs/MacOSX10.5.sdk
AM_LDFLAGS = -mmacosx-version-min=10.5 $(AM_LDFLAGS_GPGPU)
else
# because MacOSX10.4u.sdk requires g++ 4.0, you must invoke configure with: CXX=g++-4.0
AM_CXXFLAGS += -mmacosx-version-min=10.4 -isysroot /Developer/SDKs/MacOSX10.4u.sdk
AM_LDFLAGS = -mmacosx-version-min=10.4
endif
endif
if PPC32
# because MacOSX10.4u.sdk requires g++ 4.0, you must invoke configure with: CXX=g++-4.0
AM_CXXFLAGS += -mmacosx-version-min=10.4 -isysroot /Developer/SDKs/MacOSX10.4u.sdk
AM_LDFLAGS = -mmacosx-version-min=10.4
endif
endif

if PLATFORM_FREEBSD
AM_CXXFLAGS = -Wall -I/usr/local/include $(AM_CXXFLAGS_GPGPU) $(FLAGS_ARCH)
AM_CCASFLAGS = -Wa,-I$(top_srcdir)
AM_LDFLAGS = -L/usr/local/lib $(AM_LDFLAGS_GPGPU)
endif
if PLATFORM_LINUX
AM_CXXFLAGS = -Wall -I$(top_srcdir)/../tbb22_20090809oss/include $(AM_CXXFLAGS_GPGPU) $(FLAGS_ARCH)
AM_CCASFLAGS = -Wa,-I$(top_srcdir)
AM_LDFLAGS = '-Wl,-R,$$ORIGIN' $(AM_LDFLAGS_GPGPU)
endif

EXTRA_DIST = PORTING ROADMAP par2cmdline.sln par2cmdline.vcproj \
	testdata.tar.gz pretest test1 test2 test3 test4 test5 test6 \
	posttest \
	detect-mmx.s \
	reedsolomon-i386-scalar-darwin.s \
	reedsolomon-i386-scalar-posix.s \
	reedsolomon-i386-scalar.s \
	reedsolomon-i686-mmx-darwin.s \
	reedsolomon-i686-mmx-posix.s \
	reedsolomon-i686-mmx.s \
	reedsolomon-x86_64-scalar-darwin.s \
	reedsolomon-x86_64-scalar-posix.s \
	reedsolomon-x86_64-scalar.s \
	reedsolomon-x86_64-mmx-darwin.s \
	reedsolomon-x86_64-mmx-posix.s \
	reedsolomon-x86_64-mmx.s

TESTS = pretest test1 test2 test3 test4 test5 test6 posttest

install-exec-hook :
	ln -f $(DESTDIR)$(bindir)/par2$(EXEEXT) $(DESTDIR)$(bindir)/par2create$(EXEEXT)
	ln -f $(DESTDIR)$(bindir)/par2$(EXEEXT) $(DESTDIR)$(bindir)/par2verify$(EXEEXT)
	ln -f $(DESTDIR)$(bindir)/par2$(EXEEXT) $(DESTDIR)$(bindir)/par2repair$(EXEEXT)

uninstall-hook :
	rm -f $(DESTDIR)$(bindir)/par2create$(EXEEXT)
	rm -f $(DESTDIR)$(bindir)/par2verify$(EXEEXT)
	rm -f $(DESTDIR)$(bindir)/par2repair$(EXEEXT)

