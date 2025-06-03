#To run, delete the other file and take the "2" out of the name for this file
#Alternatively, this can be ran by entering the make command with "-f Makefile2" at the end


ROOTNAME=fsshell
HW=
FOPTION=
RUNOPTIONS=test.file 9994240 512
CC=gcc
CFLAGS= -g -I.
LIBS =pthread
DEPS = 
# Add any additional objects to this list
ADDOBJ= fsInit.o fsLow.o printIndent.o vcb.o freeMap.o entry.o parsePath.o splitPath.o fsdir.o fs_mkdir.o fs_stat.o fs_mv.o newEntry.o b_io.c fs_mkdir.o fs_mv.o
ARCH = $(shell uname -m)

ifeq ($(ARCH), aarch64)
	ARCHOBJ=fsLowM1.o
else
	ARCHOBJ=fsLow.o
endif

OBJ = $(ROOTNAME)$(HW)$(FOPTION).o $(ADDOBJ) $(ARCHOBJ)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) 

$(ROOTNAME)$(HW)$(FOPTION): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) -lm -l readline -l $(LIBS)

clean:
	rm $(ROOTNAME)$(HW)$(FOPTION).o $(ADDOBJ) $(ROOTNAME)$(HW)$(FOPTION)

run: $(ROOTNAME)$(HW)$(FOPTION)
	./$(ROOTNAME)$(HW)$(FOPTION) $(RUNOPTIONS)

vrun: $(ROOTNAME)$(HW)$(FOPTION)
	valgrind ./$(ROOTNAME)$(HW)$(FOPTION) $(RUNOPTIONS)


