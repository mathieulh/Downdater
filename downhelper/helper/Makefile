TARGET = downhelper
OBJS = main.o 

INCDIR = 
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LIBS = -lpspnand_driver
LDFLAGS =

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Downdater Helper 

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
