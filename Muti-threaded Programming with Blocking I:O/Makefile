# Makefile, ECE252  
# Yiqing Huang

CC = gcc       # compiler
CFLAGS = -Wall -g -std=c99 # compilation flags
LD = gcc       # linker
LDFLAGS = -g   # debugging symbols in build
LDLIBS = -lz -lcurl -pthread  # link with libz

# For students 
LIB_UTIL = zutil.o crc.o lab_png.o
SRCS   =  paster.c crc.c zutil.c lab_png.c
OBJS1  =  paster.o $(LIB_UTIL)

TARGETS= paster

all: ${TARGETS}

paster: $(OBJS1) 
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) 

%.o: %.c 
	$(CC) $(CFLAGS) -c $< 

%.d: %.c
	gcc -MM -MF $@ $<

-include $(SRCS:.c=.d)

.PHONY: clean
clean:
	rm -f *~ *.d *.o $(TARGETS) *.png *.txt 