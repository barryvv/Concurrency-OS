
# Makefile, ECE252  
# Yiqing Huang, 2019/06/12

CC = gcc       # compiler
CFLAGS = -Wall -g  -std=gnu99# compilation flg 
LD = gcc       # linker
LDFLAGS = -g   # debugging symbols in build
LDLIBS = -lpthread -lz -lcurl   # link with libz

# For students 
LIB_UTIL = zutil.o crc.o lab_png.o shm_stack.o
SRCS   = paster.c shm_stack.c crc.c zutil.c lab_png.c 
OBJS1   = paster.o $(LIB_UTIL) 

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
	rm -f *.d *.o *~ $(TARGETS) 
