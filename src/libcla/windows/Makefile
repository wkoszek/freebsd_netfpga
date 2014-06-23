LS=..
INC=..

all:	winvs

winvs:	libcla.0.dll cla_dispatch.exe examples

libcla.0.dll: $(LS)\cla.c
	@cl /LD /I$(INC) /Fe$@ /Fo$*.lib $?

cla_dispatch.exe: $(LS)\cla.c
	@cl /DCLA_TEST_PROG /I$(INC) /Fe$@ $?

examples:
	cd samples && nmake

clean:
	del cla_dispatch.exe
	del *.lib *.obj *.dll
