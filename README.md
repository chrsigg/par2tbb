par2tbb
======

[`par2`](http://parchive.sourceforge.net) creates redundancy to verify
and repair errors in
files. "[par2tbb](http://chuchusoft.com/par2_tbb/)" uses the Intel
[Threading Building Blocks](https://www.threadingbuildingblocks.org/)
library to parallelize this computation.

The TBB documentation
[recommends](http://www.threadingbuildingblocks.org/docs/help/reference/task_scheduler/task_scheduler_init_cls.htm)
that managing the number of threads should be left to the library, and
not be done explicitly by the user. Because the default strategy is to
spawn as many threads as possible, a high thermal load results for
long running computations. The resulting fan activity is distracting
if `par2` is run in the background while the computer is also used
interactively.

This version of "par2tbb" has been modified to provide an additional
command line argument `-p<n>` to specify the number of threads explicitly.
Choosing a small number of threads keeps the fan noise to a minimum and
avoids CPU throttling.