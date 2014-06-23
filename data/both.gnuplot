reset
set output "netfpga-both.jpeg"
set terminal jpeg font "/usr/local/lib/X11/fonts/webfonts/verdana.ttf"

set xrange [1:31]
set yrange [0:1200]
set xlabel "Test number"
set ylabel "MB (10^6)"

set grid
set title "NetFPGA driver comparison (FreeBSD/Linux)"

# Main one.
set style line 1 lt 1 lc rgb "red" lw 2
set style line 2 lt 1 lc rgb "green" lw 2

set style line 3 lt 1 lc rgb "blue" lw 2
set style line 4 lt 1 lc rgb "orange" lw 2

plot						\
	'linux.tcp' using 1:6	\
		with lines	\
		ls 1		\
		title 'NetFPGA/Linux/TCP'	\
	,					\
	'linux.udp' using 1:7	\
		with lines	\
		ls 2		\
		title 'NetFPGA/Linux/UDP'	\
	,					\
	'freebsd.tcp' using 1:6	\
		with lines	\
		ls 3		\
		title 'NetFPGA/FreeBSD/TCP'	\
	,					\
	'freebsd.udp' using 1:7	\
		with lines	\
		ls 4		\
		title 'NetFPGA/FreeBSD/UDP'
