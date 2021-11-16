
all: hrtimer

hrtimer: hrtimer.c kerneltricks.c Makefile
	gcc -O2 hrtimer.c kerneltricks.c -o hrtimer -lrt -lpthread

