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
#include <sys/mman.h>       // mlockall(), munlockall, mmap(), munmap()
#include <string.h>         // memset()
#include <unistd.h>         // getuid()   

//#include <sys/types.h>
//#include <sys/stat.h>
#include <fcntl.h>          // O_CREAT,...

#include "suppfunc.h"

extern int  RT_PERIOD;                  // RT_PERIOD units [us]
static int  TRESHOLD = (HISTOSIZE-1);   // TRESHOLD  units [us]

//---------------------------------------------------------------------------

void check_root( void )
{
    uid_t  uid = getuid();

    if ( uid ) {
        printf("\nApplication must run with \"root\" privileges\n\n");
        exit( 1 );
    }
}


void lock_memory( void )
{

	/* lock all memory (prevent swapping) */
	if (mlockall(MCL_CURRENT|MCL_FUTURE) == -1) {
		perror("mlockall");
		exit( -1 );
	}
}

//---------------------------------------------------------------------------
// Following two example codes are from web sources.
// Function "timer_end" is suspious!
//
// "end_time.tv_nsec"  can be less than  "start_time.tv_nsec"  when
// "end_time.tv_sec"   is greater than   "start_time.tv_sec"


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
// NOTE:
// - diff()  function "start" must be < "end"
// - addus() function operates only with small time differencies (< 1 second)

struct timespec  diff( struct timespec start, struct timespec end )
{
    struct timespec temp;
    if ((end.tv_nsec-start.tv_nsec)<0) {
        temp.tv_sec  = end.tv_sec-start.tv_sec-1;
        temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
    } else {
        temp.tv_sec  = end.tv_sec-start.tv_sec;
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

void update_metrics( metrics_t *metrics, int latency_us, struct timespec now )
{
    static int flagPERIOD = 0;
    static int flagPRINT  = 0;

    if ( metrics->reset ) {
		 memset( metrics, 0, sizeof(metrics_t) );
		 flagPERIOD = 0;
		 flagPRINT  = 0;
		 
		 // Following makes "turns calculation result bit less... xx.9???
		 struct timespec  timestamp;
		 clock_gettime( CLOCK_MONOTONIC, &timestamp );
		 metrics->start = timestamp;
	}

    metrics->counter++;
    metrics->sum_us += latency_us;

    if ( (latency_us < HISTOSIZE)  &&  (latency_us > 0) ) {
         metrics->histogram[latency_us]++;
    }
    else {
         metrics->histogram[0]++;
    }
    if ( latency_us < RT_PERIOD ) {
         flagPERIOD = 0;
    }
    if ( latency_us >= RT_PERIOD && !flagPERIOD ) {
         flagPERIOD = 1;
         metrics->long_count++;
         metrics->long_sum_us += latency_us - RT_PERIOD;
    }
    if ( latency_us < TRESHOLD ) {
         flagPRINT  = 0;
    }
    if ( latency_us >= TRESHOLD && !flagPRINT ) {
         #define SPACE 0x20
         flagPRINT = 1;
//       printf("%d/%d\n", metrics->counter, latency_us );
         printf("%8d /%2d.%03d %c\n", metrics->counter, latency_us / 1000, latency_us % 1000,
                (latency_us > RT_PERIOD) ? '*' : SPACE );
    }
    if (  metrics->max_lat < latency_us ) {
          metrics->max_lat = latency_us;
    }
	metrics->stop = now;
}


void print_metrics( metrics_t *metrics )
{
    int us       = diffus( metrics->start, metrics->stop );
    float turns  = us;
          turns /= RT_PERIOD;

    printf("# Histogram: [us] [count]\n");
    for ( int ix = 0; ix < HISTOSIZE; ix++ ) {
        printf("%06d %06d\n", ix, metrics->histogram[ix] );
    }
    printf("#\n");
    printf("# max  latency = %d\n", metrics->max_lat );
    printf("# awg  latency = %d\n", metrics->sum_us / metrics->counter );
    printf("# long count   = %d\n", metrics->long_count );
    printf("# long [ms]    = %d.%03d\n", metrics->long_sum_us / 1000, metrics->long_sum_us % 1000 );
    //
    printf("# turns        = %f\n", turns );
}

//---------------------------------------------------------------------------

void *shmOpen( char *txt, char *shmName, size_t shmSize )
{
    int  fd = shm_open(shmName, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
    if ( fd <= 0 ) {
        printf("ERROR: Open shared memory: %s\n", shmName);
        exit( 1 );
    }
    printf("Shared Memory %s (fd=%d) %s\n", txt, fd, shmName);
    if ( ftruncate(fd,shmSize) != 0 ) {
        printf("- ERROR: ftruncate size=%d\n", shmSize);
        exit( 1 );
    }
    void *pMem = mmap(0, shmSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if ( !pMem ) {
        printf("- ERROR: mmap size=%d\n", shmSize);
        exit( 1 );
    }
    close(fd);
    return pMem;
}

