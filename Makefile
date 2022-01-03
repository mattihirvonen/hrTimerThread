APP=  hrtimer
SRC=  suppfunc.c  setTermIo.c
HDR=  suppfunc.h

all:  $(APP)


hrtimer: $(HDR)  $(APP).c  $(SRC)  Makefile
	gcc -O2  $(APP).c  $(SRC)  -o $(APP)  -lrt  -lpthread
	
