#
# Makefile for building with either the Microsoft Visual
# C++ or the Microsoft Platform SDK nmake tool. Intended
# to build the 64-bit version of the par2-tbb program.
#
# 2010/02/03 Vincent Tan.
# This file is copyright 2010 Vincent Tan and released
# under the GPL v2.
#
# Example usage: Start Menu -> Microsoft Visual Studio
# -> Visual Studio Tools
# -> Visual Studio x64 Cross Tools Command Prompt
# and then enter nmake
#
# The TBB library that is linked against is named
# tbb_<platform>_<config>.lib and can be either a
# static (COFF) library or an import library (for DLLs).
#
# The compiler/linker options below are set up for
# linking against the C runtime library as a static
# library (/MT). Change the /MT to /MD for the DLL
# version of the CRT library.
#
# The compiler/linker options below are set up for
# linking against the TBB library as a static
# library (/D__TBB_NO_IMPLICIT_LINKAGE=1). Remove
# the "/D__TBB_NO_IMPLICIT_LINKAGE=1" option for
# linking against it as a DLL.
#
# To build the TBB as a static library that itself
# uses the CRT library as a static library, modify
# these files in the TBB source distribution:
#
# in <tbb_src>/build/Makefile.tbb, change:
#   LIB_LINK_FLAGS += $(EXPORT_KEY)tbb.def
# to
#   LIB_LINK_FLAGS += $(EXPORT_KEY)tbb.lib
#
# in <tbb_src>/build/windows.cl.inc, change these lines:
#   LIB_LINK_FLAGS=/link /nologo /DLL /MAP /DEBUG /fixed:no /INCREMENTAL:NO
#   MS_CRT_KEY = /MD$(if $(findstring debug,$(cfg)),d)
#   EXPORT_KEY = /DEF:
# to
#   LIB_LINK_FLAGS=/link /nologo /MAP /DEBUG /fixed:no /INCREMENTAL:NO
#   MS_CRT_KEY = /MT$(if $(findstring debug,$(cfg)),d)
#   EXPORT_KEY = /OUT:
#
# and add this line:
#   LIB_LINK_CMD = lib /nologo # lib.exe creates static libraries
#
# before the "COMPILE_ONLY = /c" line, add:
#   CPLUS_FLAGS += /D__TBB_NO_IMPLICIT_LINKAGE=1 /D__TBB_TASK_CPP_DIRECTLY_INCLUDED=1
#
# When building the TBB, use GNU make and pass "arch=intel64" to it
# when building the x86_64/AMD64 version. Don't be surprised if the
# TBB does not build all of its libraries. All you need is the main
# TBB libraries (debug and release) to be built. From the directories
# in the "build" directory, copy out the tbb.lib files to the root
# level of the TBB source distribution and rename them to
# tbb_<platform>_<config>.lib
#
!include <win32.mak>

MY_TBB=..\tbb22_20090809oss_src
!IF "$(PLATFORM)" == "x86"  # "$(MSVCVER)" == "Win32"
MY_LD_FLAGS=/DYNAMICBASE /NXCOMPAT
MY_LD_FILES=$(MY_TBB)/tbb_Win32_Release.lib
MY_EXE=par2_win32.exe
!ELSEIF "$(PLATFORM)" == "x64"  # "$(MSVCVER)" == "Win64"
MY_CC_FLAGS=/DWIN64 /D__TBB_NO_IMPLICIT_LINKAGE=1
MY_LD_FILES=$(MY_TBB)/tbb_Win64_Release.lib
MY_EXE=par2_win64.exe
!ELSE
#  !ERROR unknown architecture
!ENDIF

all: $(MY_EXE)

$(MY_EXE): verificationhashtable.obj verificationpacket.obj reedsolomon.obj recoverypacket.obj pipeline.obj par2repairersourcefile.obj par2fileformat.obj par2repairer.obj par2creatorsourcefile.obj par2creator.obj par2cmdline.obj par1repairersourcefile.obj par1repairer.obj par1fileformat.obj md5.obj mainpacket.obj galois.obj filechecksummer.obj diskfile.obj datablock.obj descriptionpacket.obj criticalpacket.obj creatorpacket.obj crc.obj commandline.obj buffer.obj
  $(link) $(ldebug) $(conflags) /LTCG /OPT:REF /OPT:ICF /LTCG $(MY_LD_FLAGS) -out:$(MY_EXE) $** $(conlibs) kernel32.lib $(MY_LD_FILES)

CPPFLAGS=/Ox /GL /GF /EHsc /MT /GS- /GR- $(MY_CC_FLAGS) /DWIN32 /DNDEBUG /DUNICODE /D_UNICODE /D_CONSOLE /DPACKAGE=\"par2cmdline\" /DVERSION=\"0.4\" /I$(MY_TBB)\include

clean:
	del /A-R *.obj $(MY_EXE)
