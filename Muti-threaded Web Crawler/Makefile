# Makefile, ECE252  
# Yiqing Huang <yqhuang@uwaterloo.ca>

CC = gcc 
CFLAGS_XML2 = $(shell xml2-config --cflags)
CFLAGS_CURL = $(shell curl-config --cflags)
CFLAGS = -Wall $(CFLAGS_XML2) $(CFLAGS_CURL) -std=gnu99 -g -DDEBUG1_
LD = gcc
LDFLAGS = -std=gnu99 -g 
LDLIBS_XML2 = $(shell xml2-config --libs)
LDLIBS_CURL = $(shell curl-config --libs)
LDLIBS = $(LDLIBS_XML2) $(LDLIBS_CURL) -pthread -lz

LIB_UTIL = zutil.o crc.o lab_png.o
SRCS   = findpng2.c crc.c zutil.c lab_png.c
OBJS3  = findpng2.o $(LIB_UTIL)
TARGETS= findpng2

all: ${TARGETS}

findpng2: $(OBJS3) 
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) 

%.o: %.c 
	$(CC) $(CFLAGS) -c $< 

%.d: %.c
	gcc -MM -MF $@ $<

-include $(SRCS:.c=.d)

.PHONY: clean
clean:
	rm -f *~ *.d *.o $(TARGETS) *.png *.html *.txt *.dat
