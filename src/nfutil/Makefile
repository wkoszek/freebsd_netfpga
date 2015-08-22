SRCS=	\
	../libnetfpga/netfpga.c \
	../libnetfpga/netfpga_dummy.c \
	../libnetfpga/netfpga_linux.c \
	../libnetfpga/netfpga_freebsd.c \
	../../../libxbf/xbf.c \
	../../../libcla/cla.c	\
	nfutil.c

CFLAGS+= -I../../../libxbf
CFLAGS+= -I../../../libcla
CFLAGS+= -I../libnetfpga

CFLAGS+= -g -ggdb -Wall -O2

nfutil: $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o nfutil
