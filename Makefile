
SRC= hrtimer.c suppfunc.c

HDR= suppfunc.h

all: hrtimer

hrtimer:  $(HDR) $(SRC)  Makefile
	gcc -O2  $(SRC)  -o hrtimer  -lrt  -lpthread

