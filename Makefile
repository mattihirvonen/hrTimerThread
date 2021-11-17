
SRC= suppfunc.c
HDR= suppfunc.h

all:  hrtimer  hrprint


hrtimer:  $(HDR) $(SRC)  hrtimer.c  Makefile
	gcc -O2  $(SRC)  hrtimer.c  -o hrtimer  -lrt  -lpthread
	

hrprint:  $(HDR) $(SRC)  hrprint.c  Makefile
	gcc -O2  $(SRC)  hrprint.c  -o hrprint  -lrt  -lpthread

