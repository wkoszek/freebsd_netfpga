# The FreeBSD drive for the Stanford NetFPGA 1G card

My FreeBSD + NetFPGA work done over a 01.07.2009--31.08.2009 duration. This
directory is self-contained and should have everything what's necessary to
get the driver working and the card programmed.

The command line handling library is in `src/libcla` (it has since been
ported to a separate repository https://github.com/wkoszek/libcla too).
The card requires a command line utility `nfutil` for card programming, and
its sources are in `src/nfutil`. The utility is written in a modular way,
and it dependent on the library `libnetfpga`, which provides the tool
majority of the functionality. For the utility to perform the actual
programming, necessary was also a library for reading Xilinx FPGA `.bit` files.
For this purposes the library called `libxbf` has been created. It is based on the
open-source data from the Internet on how to do basic decoding of `.bit`
header and data. It has no other knowledge on bitfiles.

The actual FreeBSD kernel driver is in `src/netfpga_kmod`.

I had to write some header files, but the actual header files with necessary
card register offsets are coming from Stanford NetFPGA. Thanks to Stanford
people we got the files open-sourced under the BSD license. For these header
files, look in `include/`.

Additional documentation is in `man` and `doc`. The later has PDF files used
for presentation at HIIT, Ericsson and  the FreeBSD DevSummit conference.

Some measurement data are in `data`. Provided are also other educational
programs `contrib/cmd_static`, which was an experiment to implement the
static structures for the command line handling. The `contrib/multisysctl`
was implemented to help me understand SYSCTLs better.


Wojciech A. Koszek
wkoszek@FreeBSD.org
http://FreeBSD.czest.pl/~wkoszek/
