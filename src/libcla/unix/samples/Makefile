SS=../../samples
LS=../..
CFLAGS+= -g -ggdb -O2 -Wall -pedantic -std=c99 -I${SS} -I${LS}


all:	samples

samples: 		\
	sample_0	\
	sample_1	\
	sample_2	\
	sample_3	\
	sample_4	\
	sample_5

sample_0: ${SS}/sample_0.c ${LS}/cla.c
	${CC} ${CFLAGS} -o $@ $?

sample_1: ${SS}/sample_1.c ${LS}/cla.c
	${CC} ${CFLAGS} -o $@ $?

sample_2: ${SS}/sample_2.c ${LS}/cla.c
	${CC} ${CFLAGS} -o $@ $?

sample_3: ${SS}/sample_3.c ${LS}/cla.c
	${CC} ${CFLAGS} -o $@ $?

sample_4: ${SS}/sample_4.c ${LS}/cla.c
	${CC} ${CFLAGS} -o $@ $?

sample_5: ${SS}/sample_5.c ${LS}/cla.c
	${CC} ${CFLAGS} -o $@ $?

clean:
	rm -rf sample_*
