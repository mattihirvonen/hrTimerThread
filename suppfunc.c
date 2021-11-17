//
// File:  suppfunc.c
//
// Various application support functions
//

#include <stdint.h>
#include <stdlib.h>         // exit()
#include <stdio.h>          // perror()
#include <error.h>          // errno
#include <time.h>           // struct timespec
#include <sys/mman.h>       // mlockall(), munlockall

#include "suppfunc.h"


void lock_memory( void )
{

	/* lock all memory (prevent swapping) */
	if (mlockall(MCL_CURRENT|MCL_FUTURE) == -1) {
		perror("mlockall");
		exit( -1 );
	}
}

//---------------------------------------------------------------------------

// call this function to start a nanosecond-resolution timer
struct timespec timer_start(){
    struct timespec start_time;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_time);
    return start_time;
}

// call this function to end a timer, returning nanoseconds elapsed as a long
long timer_end(struct timespec start_time){
    struct timespec end_time;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_time);
    long diffInNanos = (end_time.tv_sec - start_time.tv_sec) * (long)1e9 + (end_time.tv_nsec - start_time.tv_nsec);
    return diffInNanos;
}

//---------------------------------------------------------------------------

struct timespec  diff( struct timespec start, struct timespec end )
{
    struct timespec temp;
    if ((end.tv_nsec-start.tv_nsec)<0) {
        temp.tv_sec = end.tv_sec-start.tv_sec-1;
        temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec-start.tv_sec;
        temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
    return temp;
}

struct timespec addus( struct timespec timestamp, int us )
{
    timestamp.tv_nsec += 1000 * us;
    if ( timestamp.tv_nsec >= 1000000000 ) {
         timestamp.tv_nsec -= 1000000000;
         timestamp.tv_sec++;
    }
    return timestamp;
}

int diffus( struct timespec start, struct timespec end )
{
    struct timespec diff_tv = diff( start, end );

    return (1000000 * diff_tv.tv_sec) + (diff_tv.tv_nsec / 1000);
}

//---------------------------------------------------------------------------

