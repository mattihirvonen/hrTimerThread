
all: hrtimer

hrtimer:  hrtimer.c  Makefile
	gcc -O2  hrtimer.c  -o hrtimer  -lrt  -lpthread

