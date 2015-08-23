SRCS=	\
	../libnetfpga/netfpga.c \
	../libnetfpga/netfpga_dummy.c \
	../libnetfpga/netfpga_linux.c \
	../libnetfpga/netfpga_freebsd.c \
	../../contrib/libcla/cla.c \
	../../contrib/libxbf/xbf.c \
	nfutil.c

CFLAGS+= -I../../contrib/libxbf
CFLAGS+= -I../../contrib/libcla
CFLAGS+= -I../libnetfpga

CFLAGS+= -g -ggdb -Wall -O2

nfutil: $(SRCS) Makefile
	$(CC) $(CFLAGS) $(SRCS) -o nfutil

clean:
	rm -rf *.o *.dSYM nfutil
