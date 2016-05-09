//  This file is part of par2cmdline (a PAR 2.0 compatible file verification and
//  repair tool). See http://parchive.sourceforge.net for details of PAR 2.0.
//
//  Copyright (c) 2003 Peter Brian Clements
//
//  par2cmdline is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  par2cmdline is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//  Modifications for concurrent processing, Unicode support, and hierarchial
//  directory support are Copyright (c) 2007-2009 Vincent Tan.
//  Search for "#if WANT_CONCURRENT" for concurrent code.
//  Concurrent processing utilises Intel Thread Building Blocks 2.0,
//  Copyright (c) 2007 Intel Corp.

#include "par2cmdline.h"
#include <backward/auto_ptr.h>

#ifdef _MSC_VER
#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif
#endif

void banner(void)
{
  string version = PACKAGE " version " VERSION;

  cout << version << ", Copyright (C) 2003 Peter Brian Clements." << endl
#if WANT_CONCURRENT
       << "Modifications for concurrent processing, Unicode support, and hierarchial" << endl
       << "directory support are Copyright (c) 2007-2009 Vincent Tan." << endl
       << "Concurrent processing utilises Intel Thread Building Blocks 2.0," << endl
       << "Copyright (c) 2007-2008 Intel Corp." << endl
  #if __x86_64__ || defined(WIN64)
       << "Executing using the 64-bit x86 (AMD64) instruction set." << endl
  #elif __i386__ || defined(WIN32)
       << "Executing using the 32-bit x86 (IA32) instruction set." << endl
  #elif __ppc64__
       << "Executing using the 64-bit PowerPC (PPC64) instruction set." << endl
  #elif __ppc__
       << "Executing using the 32-bit PowerPC (PPC32) instruction set." << endl
//#elif __alpha__
//     << "Executing using the 32-bit Alpha (ALPHA) instruction set." << endl
//#elif __mips__
//     << "Executing using the 32-bit MIPS (MIPS) instruction set." << endl
  #else
       << "Executing using an unknown instruction set." << endl
  #endif
#endif
       << endl
       << "par2cmdline comes with ABSOLUTELY NO WARRANTY." << endl
       << endl
       << "This is free software, and you are welcome to redistribute it and/or modify" << endl
       << "it under the terms of the GNU General Public License as published by the" << endl
       << "Free Software Foundation; either version 2 of the License, or (at your" << endl
       << "option) any later version. See COPYING for details." << endl
       << endl;
}

#ifdef UNICODE
int wmain(int argc, wchar_t *argv[])
#else
int main(int argc, char *argv[])
#endif
{
#if WANT_CONCURRENT
    tbb::task_scheduler_init init(tbb::task_scheduler_init::deferred);
#endif

#ifdef _MSC_VER
  // Memory leak checking
  _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_ALLOC_MEM_DF | /*_CRTDBG_CHECK_CRT_DF | */_CRTDBG_DELAY_FREE_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

  // Parse the command line
  std::auto_ptr<CommandLine> commandline(new CommandLine);

  Result result = eInvalidCommandLineArguments;
  
  if (!commandline->Parse(argc, argv))
  {
    banner();
    CommandLine::usage();
  }
  else
  {
    if (commandline->GetNoiseLevel() > CommandLine::nlSilent)
      banner();
      
#if WANT_CONCURRENT
    int p = commandline->GetNumThreads();
    if (p > 0)
    {
      cout << "Number of threads: " << p << endl;
      init.initialize(p);
    }
    else
      init.initialize();
#endif

    // Which operation was selected
    switch (commandline->GetOperation())
    {
    case CommandLine::opCreate:
      {
        // Create recovery data

        Par2Creator *creator = new Par2Creator;
        result = creator->Process(*commandline);
        delete creator;
      }
      break;
    case CommandLine::opVerify:
      {
        // Verify damaged files
        switch (commandline->GetVersion())
        {
        case CommandLine::verPar1:
          {
            Par1Repairer *repairer = new Par1Repairer;
            result = repairer->Process(*commandline, false);
            delete repairer;
          }
          break;
        case CommandLine::verPar2:
          {
            Par2Repairer *repairer = new Par2Repairer;
            result = repairer->Process(*commandline, false);
            delete repairer;
          }
          break;
        case CommandLine::opNone:
          break;
        }
      }
      break;
    case CommandLine::opRepair:
      {
        // Repair damaged files
        switch (commandline->GetVersion())
        {
        case CommandLine::verPar1:
          {
            Par1Repairer *repairer = new Par1Repairer;
            result = repairer->Process(*commandline, true);
            delete repairer;
          }
          break;
        case CommandLine::verPar2:
          {
            Par2Repairer *repairer = new Par2Repairer;
            result = repairer->Process(*commandline, true);
            delete repairer;
          }
          break;
        case CommandLine::opNone:
          break;
        }
      }
      break;
    case CommandLine::opNone:
      break;
    }
  }

  return result;
}
