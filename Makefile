
SRC= suppfunc.c
HDR= suppfunc.h

all:  hrtimer


hrtimer:  $(HDR) $(SRC)  hrtimer.c  Makefile
	gcc -O2  $(SRC)  hrtimer.c  -o hrtimer  -lrt  -lpthread
	
