all:
	@echo "make [freebsd_to_linux|freebsd|linux]"

freebsd_to_linux_rx:
	../contrib/bench/netfpga_bench.sh NonNetFPGA.rx

freebsd_rx:
	../contrib/bench/netfpga_bench.sh FreeBSD.rx

linux_rx:
	../contrib/bench/netfpga_bench.sh Linux.rx

freebsd_to_linux_tx:
	../contrib/bench/netfpga_bench.sh NonNetFPGA.tx

freebsd_tx:
	../contrib/bench/netfpga_bench.sh FreeBSD.tx 

linux_tx:
	../contrib/bench/netfpga_bench.sh Linux.tx


netfpga-non.jpeg: non-netfpga.gnuplot
	cat non-netfpga.gnuplot | gnuplot

netfpga-linux.jpeg: linux.gnuplot
	cat linux.gnuplot | gnuplot

netfpga-freebsd.jpeg: freebsd.gnuplot
	cat freebsd.gnuplot | gnuplot

netfpga-both.jpeg: both.gnuplot
	cat both.gnuplot | gnuplot

all-jpeg: netfpga-non.jpeg netfpga-linux.jpeg
