//  gcc hrtimer.c -O2 -o hrtimer -lrt -lpthread
//
// https://stackoverflow.com/questions/6749621/how-to-create-a-high-resolution-timer-in-linux-to-measure-program-performance
// https://hugh712.gitbooks.io/embeddedsystem/content/real-time_application_development.html
//
// An existing program can be started in a specific scheduling class with
// a specific priority using the chrt command line tool
//
// Example:  chrt -f 80 ./hrtimer -f: SCHED_FIFO -r: SCHED_RR
//
// Linux/Posix real time operations based on:
//
//      /usr/lib/rtkit/rtkit-daemon

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>

int RT_PRIORITY   = 90;
int RT_PERIOD     = 1000;         // Unit:   [us]
int RT_POLICY     = SCHED_FIFO;   // Policy: SCHED_FIFO, SCHED_RR, SCHED_OTHER

void init_kernel_tricks( void );
void restore_kernel_tricks( void );

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

struct timespec diff( struct timespec start, struct timespec end )
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

int diffus( struct timespec start, struct timespec end )
{
    struct timespec diff_tv = diff( start, end );

    return (1000000 * diff_tv.tv_sec) + (diff_tv.tv_nsec / 1000);
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

//---------------------------------------------------------------------------
//
// https://events.static.linuxfound.org/sites/events/files/slides/cyclictest.pdf
// https://man7.org/linux/man-pages/man3/pthread_setschedparam.3.html
//
// int pthread_setschedparam(pthread_t thread, int policy, const struct sched_param *param);
// int pthread_getschedparam(pthread_t thread, int *restrict policy, struct sched_param *restrict param);
//
// NOTE: Raise only stepper driver thread's priority!
//
/*
    Processes with numerically higher priority values are scheduled
    before processes with numerically lower priority values.  Thus,
    the value returned by sched_get_priority_max() will be greater
    than the value returned by sched_get_priority_min().

    Linux allows the static priority range 1 to 99 for the SCHED_FIFO
    and SCHED_RR policies, and the priority 0 for the remaining
    policies.  Scheduling priority ranges for the various policies
    are not alterable.
*/

#define HISTOSIZE      1001
#define TESTtime(sec)  (((sec) * 1000000) / RT_PERIOD)

int TRESHOLD_us = 2000;          // HISTOSIZE ?


struct metrics_t {
    int  histogram[HISTOSIZE];
    int  long_sum_us;
    int  long_count;
    int  max_lat;
    int  sum_us;
    int  counter;
    //
    struct timespec start, stop;
} metrics;


void update_metrics( int latency_us )
{
    static int flagPERIOD = 0;
    static int flagPRINT  = 0;

    metrics.counter++;
    metrics.sum_us += latency_us;

    if ( (latency_us < HISTOSIZE)  &&  (latency_us > 0) ) {
         metrics.histogram[latency_us]++;
    }
    else {
         metrics.histogram[0]++;
    }
    if ( latency_us < RT_PERIOD ) {
         flagPERIOD = 0;
    }
    if ( latency_us >= RT_PERIOD && !flagPERIOD ) {
         flagPERIOD = 1;
         metrics.long_count++;
         metrics.long_sum_us += latency_us - RT_PERIOD;
    }
    if ( latency_us < TRESHOLD_us ) {
         flagPRINT  = 0;
    }
    if ( latency_us >= TRESHOLD_us && !flagPRINT ) {
         #define SPACE 0x20
         flagPRINT = 1;
//       printf("%d/%d\n", metrics.counter, latency_us );
         printf("%8d /%2d.%03d %c\n", metrics.counter, latency_us / 1000, latency_us % 1000,
                (latency_us > RT_PERIOD) ? '*' : SPACE );
    }
    if (  metrics.max_lat < latency_us ) {
          metrics.max_lat = latency_us;
    }
}


void print_metrics( void )
{
    int us       = diffus( metrics.start, metrics.stop );
    float turns  = us;
          turns /= RT_PERIOD;

    printf("# Histogram: [us] [count]\n");
    for ( int ix = 0; ix < HISTOSIZE; ix++ ) {
        printf("%06d %06d\n", ix, metrics.histogram[ix] );
    }
    printf("#\n");
    printf("# max  latency = %d\n", metrics.max_lat );
    printf("# awg  latency = %d\n", metrics.sum_us / metrics.counter );
    printf("# long count   = %d\n", metrics.long_count );
    printf("# long [ms]    = %d.%03d\n", metrics.long_sum_us / 1000, metrics.long_sum_us % 1000 );
    //
    printf("# turns        = %f\n", turns );
}

//---------------------------------------------------------------------------

typedef struct
{
    int dummy_sample;
} thread_args_t;

thread_args_t  thread_args;


void periodic_application_code( void )
{
}


void * threadFunc( void *arg )
{
    #define  OFFSET_ns   0  //  100000

//  printf("max=%d  min=%d\n", sched_get_priority_max(SCHED_FIFO), sched_get_priority_min(SCHED_FIFO) );

    struct sched_param   param;

//  param.sched_priority = RT_PRIORITY;
//  pthread_setschedparam( pthread_self(), RT_POLICY, &param );

    struct timespec  now, next, remain;
    int              latency_us;
    int              shutdown;
    int              flags = TIMER_ABSTIME;    // 0=relative or TIMER_ABSTIME, TIMER_REALTIME

    clock_gettime( CLOCK_MONOTONIC, &now );
    now.tv_nsec = now.tv_nsec - (now.tv_nsec % (1000 * RT_PERIOD)) + OFFSET_ns;

    metrics.start = now;

    next = now;
//  for ( shutdown = 0; !shutdown; )
    for ( int counter = 0; counter < TESTtime(100); counter++)
    {
        next = addus( next, RT_PERIOD );

        // Note this simple example do not handle "remain"
        // if delay end before elapsed time (like signal etc.. case).
        int err = clock_nanosleep( CLOCK_MONOTONIC, flags, &next, &remain );

        clock_gettime( CLOCK_MONOTONIC, &now );
        latency_us = diffus( next, now );

        periodic_application_code();
        update_metrics( latency_us );
    }
    metrics.stop = now;

    return NULL;
}


int main( void )
{
    pid_t  pid             = getpid();
    int    currentPriority = getpriority( PRIO_PROCESS, pid );
//  int    newPriority     = PRIORITY;

//  setpriority( PRIO_PROCESS, pid, newPriority );

//  init_kernel_tricks();

    struct sched_param parm;
    pthread_attr_t attr;
    
    pthread_attr_init( &attr );
    pthread_attr_setinheritsched( &attr, PTHREAD_EXPLICIT_SCHED );
    pthread_attr_setschedpolicy( &attr, RT_POLICY );
    parm.sched_priority = RT_PRIORITY;
    pthread_attr_setschedparam( &attr, &parm );

    pthread_t  threadId;
    int        err;

    err = pthread_create( &threadId, &attr, &threadFunc, &thread_args );
    if ( err ) {
         printf("ERROR to create thread\n");
         return -1;
    }
    err = pthread_join( threadId, NULL );
    if ( err ) {
         printf("ERROR to join thread\n");
         return -1;
    }
    print_metrics();

//  restore_kernel_tricks();

    return 0;
}
