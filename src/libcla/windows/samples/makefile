SS=..\..\cla.c ..\..\samples\\

LS=..\..
CFLAGS=/I$(LS)

examples: 		\
	sample_0.exe	\
	sample_1.exe	\
	sample_2.exe	\
	sample_3.exe

sample_0.exe: $(SS)$*.c
	$(CC) $(CFLAGS) /Fe$@ $?

sample_1.exe: $(SS)$*.c $(LS)\cla.c
	$(CC) $(CFLAGS) /Fe$@ $?

sample_2.exe: $(SS)$*.c $(LS)\cla.c
	$(CC) $(CFLAGS) /Fe$@ $?

sample_3.exe: $(SS)$*.c $(LS)\cla.c
	$(CC) $(CFLAGS) /Fe$@ $?

clean:
	del *.exe *.obj
