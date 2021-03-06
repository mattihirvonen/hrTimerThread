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
// NOTE(s):
// - tsDiff() function: "start" must be < "end"
// - some ARM cores do not have hardware division command

struct timespec  tsDiff( struct timespec start, struct timespec end )
{
    struct timespec temp;

    if ( (end.tv_nsec - start.tv_nsec) < 0 ) {
         temp.tv_sec  = end.tv_sec  - start.tv_sec  - 1;
         temp.tv_nsec = end.tv_nsec - start.tv_nsec + 1000000000;
    } else {
         temp.tv_sec  = end.tv_sec  - start.tv_sec;
         temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return temp;
}


struct timespec tsAddus( struct timespec timestamp, int us )
{
    // Optimize speed for forward where "us" > 0

    if ( us < 0 ) {
         return tsSubus( timestamp, -us );
    }
    if ( us >= 1000000 ) {            // Try to avoid division
         int sec = us / 1000000;
         timestamp.tv_sec += sec;
         us = us - (sec * 1000000);   // Multiplication is faster than division
    }
    timestamp.tv_nsec += 1000 * us;
    if ( timestamp.tv_nsec >= 1000000000 ) {
         timestamp.tv_nsec -= 1000000000;
         timestamp.tv_sec++;
    }
    return timestamp;
}


struct timespec tsSubus( struct timespec timestamp, int us )
{
    if ( us < 0 ) {
         return tsAddus( timestamp, -us );
    }
    if ( us >= 1000000 ) {             // Try to avoid division
         int sec = us / 1000000;
         timestamp.tv_sec  -= sec;
         us = us - (sec * 1000000);    // Multiplication is faster than division
    }
    timestamp.tv_nsec -= 1000 * us;
    if ( timestamp.tv_nsec  < 0 ) {
         timestamp.tv_nsec += 1000000000;
         timestamp.tv_sec--;
    }
    return timestamp;
}


int64_t ts2us( struct timespec timestamp )
{
    int64_t us;

    us   = timestamp.tv_sec;
    us  *= 1000000;
    us  += timestamp.tv_nsec / 1000;
    return us;
}


int64_t tsDiffus( struct timespec start, struct timespec end )
{
#if 1
    int64_t  usStart = ts2us( start );
    int64_t  usEnd   = ts2us( end   );

    return ( usEnd - usStart );
#else
    struct  timespec diff = tsDiff( start, end );
    int64_t          temp;

    temp  = diff.tv_sec;
    temp *= 1000000;
    #if 0
    // Optimize speed with cost of small error (error is 2.4 %)
    #warning "Function diffus() optimize speed with small error"
    temp += diff.tv_nsec >> 10;
    #else
    temp += diff.tv_nsec / 1000;
    #endif
    return  temp;
#endif
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

         // Following is enough accurate
         struct timespec  timestamp;
         clock_gettime( CLOCK_MONOTONIC, &timestamp );
         metrics->start = tsSubus( timestamp, RT_PERIOD+latency_us );
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
         metrics->late_count++;
         metrics->late_sum_us += latency_us - RT_PERIOD;
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
    double  runtime  = tsDiffus( metrics->start, metrics->stop );
    double  rounds   = runtime / RT_PERIOD;

    double  awg_ms   = metrics->sum_us;
            awg_ms  /= metrics->counter;
            awg_ms  /= 1000.0;

    double  max_lat_ms  = metrics->max_lat;
            max_lat_ms /= 1000.0;

    // Cumulative sum of overflow RT_PERIOD
    double  late_sum_ms  = metrics->late_sum_us;
            late_sum_ms /= 1000.0;

    printf("# Histogram: [us] [count]\n");
    for ( int ix = 0; ix < HISTOSIZE; ix++ ) {
        if ( metrics->histogram[ix] ) {
            printf("%06d %06d\n", ix, metrics->histogram[ix] );
        }
    }
    printf("#\n");
    printf("# run  time [s] = %-20.3f\n", runtime / 1000000.0 );
    printf("# rt   counter  = %d\n",      metrics->counter );
    printf("# rt   period   = %-20.3f\n", (float)RT_PERIOD / 1000.0 );
//  printf("# max  latency  = %d\n",      metrics->max_lat );
    printf("# max  latency  = %-20.3f\n", max_lat_ms );
    printf("# awg  latency  = %-20.3f\n", awg_ms );
    printf("# late count    = %d\n",      metrics->late_count );
//  printf("# late [ms]     = %d.%03d\n", metrics->late_sum_us / 1000, metrics->late_sum_us % 1000 );
    printf("# late sum      = %-20.3f\n", late_sum_ms );
    printf("# hist.overflow = %d\n",      metrics->histogram[0] );
    printf("#\n");
//  printf("# rounds        = %-20.3f\n", rounds );
    //
    #if 0
    struct  timespec past, now;
    clock_gettime( CLOCK_MONOTONIC, &now );
    past = now;
    past = tsSubus( past, 100000100 );
//    past = tsSubus( past,  99999900 );
//    past = tsSubus( past,    999900 );
    now  = tsAddus( now,        100 );

    printf("# pos:           %-12lld\n", tsDiffus(past, now)  );
    printf("# neg:          %-12lld\n",  tsDiffus(now,  past) );
    #endif
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

//================================================================================================
