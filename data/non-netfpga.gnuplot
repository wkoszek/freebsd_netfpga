reset
set output "netfpga-non.jpeg"
set terminal jpeg font "/usr/local/lib/X11/fonts/webfonts/verdana.ttf"

set xrange [1:31]
set yrange [0:1300]
set xlabel "Test number"
set ylabel "MB (10^6)"

set grid
set title "Broadcom (FreeBSD) to Intel (Linux) performance"

# Main one.
set style line 1 lt 1 lc rgb "red" lw 2
set style line 2 lt 1 lc rgb "blue" lw 2
set style line 3 lt 1 lc rgb "green" lw 2
set style line 4 lt 1 lc rgb "purple" lw 2

plot					\
	'NonNetFPGA.rx.tcp'		\
	using 1:6			\
	with lines 			\
	ls 1				\
	title 'Broadcom->Intel/TCP/RX'	\
	,				\
	'NonNetFPGA.rx.udp'		\
	using 1:7			\
	with lines 			\
	ls 2				\
	title 'Broadcom->Intel/UDP/RX'	\
	,				\
	'NonNetFPGA.tx.tcp'		\
	using 1:6			\
	with lines 			\
	ls 3				\
	title 'Broadcom->Intel/TCP/TX'	\
	,				\
	'NonNetFPGA.tx.udp'		\
	using 1:7			\
	with lines 			\
	ls 4				\
	title 'Broadcom->Intel/UDP/TX'
