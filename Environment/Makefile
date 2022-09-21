# Makefile, ECE252  
# Yiqing Huang

CC = gcc       # compiler
CFLAGS = -Wall -g -std=c99 # compilation flags
LD = gcc       # linker
LDFLAGS = -g   # debugging symbols in build
LDLIBS = -lz   # link with libz

# For students 
LIB_UTIL = zutil.o crc.o lab_png.o
SRCS   =  catpng.c findpng.c pnginfo.c crc.c zutil.c lab_png.c
OBJS1  =  catpng.o $(LIB_UTIL)
OBJS2  =  findpng.o $(LIB_UTIL) 
OBJS3  =  pnginfo.o $(LIB_UTIL) 

TARGETS= findpng catpng pnginfo

all: ${TARGETS}

catpng: $(OBJS1) 
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) 

findpng: $(OBJS2) 
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS)

pnginfo: $(OBJS3) 
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS)

%.o: %.c 
	$(CC) $(CFLAGS) -c $< 

%.d: %.c
	gcc -MM -MF $@ $<

-include $(SRCS:.c=.d)

.PHONY: clean
clean:
	rm -f *.d *.o $(TARGETS) 
	 