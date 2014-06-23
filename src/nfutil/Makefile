.PATH:					\
	${.CURDIR}/../libcla		\
	${.CURDIR}/../libnetfpga	\
	${.CURDIR}/../libxbf

CFLAGS+=-g -ggdb
CFLAGS+=-O0
CFLAGS+=		\
	-I../libcla     \
	-I../libnetfpga \
	-I../libxbf

PROG=   nfutil

SRCS+=				\
        cla.c			\
        netfpga.c		\
        netfpga_dummy.c		\
        netfpga_linux.c		\
        netfpga_freebsd.c	\
        xbf.c			\
        nfutil.c

NO_MAN= yes

.include <bsd.prog.mk>


