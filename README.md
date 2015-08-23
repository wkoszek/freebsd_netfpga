# The FreeBSD driver for the Stanford NetFPGA 1G card

My FreeBSD + NetFPGA work done over a 01.07.2009--31.08.2009 duration. In
2015 in hope to keep parts of this project alive I wired it to Travis, so
that most of the software still builds with new tools with no warnings.
Changes I made on MacOSX/GNU Linux, without the actual hardware connected,
since I no longer have an access to the desktop machines.

Early versions of this directory were self-contained and had everything
what's necessary to get the driver working and the card programmed and were
proved to compile and work on FreeBSD. In case my 2015 fixes broke
something, so `git checkout` the initial code import.

# Introduction

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

# How to build

To compile the driver, use the following commands:


	git clone https://github.com/wkoszek/freebsd_netfpga.git
	cd src/nfutil/
	make

The clone of the repository, as a result of having modules, will pull 2 more
libraries:

	https://github.com/wkoszek/libcla
	https://github.com/wkoszek/libxbf

Since the `nfutil` requires to have `libcla` for command line parsing and
`libxbf` for bitfile reading.

# How to run

Usage is as follows:

	$ nfutil -h

	nfutil
		reg
			read <reg>
			write <reg> <value>
			list
		cpci
			write <file>
			info
		cnet
			write <file>
			info

First you'll have to program the CPCI bridge with CPCI firmware. Then you
can program the CNET FPGA:

	nfutil cpci write <file>
	nfutil cnet write <file>
