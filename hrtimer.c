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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>                // struct timespec
#include <sys/resource.h>        // getpriority(), setpriority()
#include <sys/mman.h>            // munmap()
#include <unistd.h>              // getpid()
#include <pthread.h>
#include <string.h>
#include "suppfunc.h"

#define  SHM_METRICS    "RT_METRICS"      // Shared memory file name

#define  TESTtime(sec)  (((sec) * 1000000) / RT_PERIOD)

metrics_t  *metrics_data;         // ToDo: Make own instance for each RT thread
int        UART_METRICS  =  0;    // Measure metrics using serial port loop back

int RT_PRIORITY   = 90;
int RT_PERIOD     = 2000;         // Unit:   [us]
int RT_POLICY     = SCHED_FIFO;   // Policy: SCHED_FIFO, SCHED_RR, SCHED_OTHER

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

//---------------------------------------------------------------------------

typedef struct
{
    int  thread_number;
} thread_args_t;


thread_args_t    thread_args;  // ToDo: Make own instance for each RT thread
int              shutdown;     // Write here non zero value to terminate RT thread
uint32_t         runtime;
struct timespec  last;
int              fd;           // UART
char             buf[100];


void periodic_application_code( void )
{
    *buf = 0x0f;   // LS bit first out

    write( fd, buf, 1 );
}


void * threadFunc( void *arg )
{
    #define  OFFSET_ns   0  //  100000

    thread_args_t *ta = arg;

    struct sched_param   param;

    struct timespec  now, next, remain;
    int              latency_us;
    int              flags = TIMER_ABSTIME;    // 0=relative or TIMER_ABSTIME, TIMER_REALTIME

    clock_gettime( CLOCK_MONOTONIC, &now );
    now.tv_nsec = now.tv_nsec - (now.tv_nsec % (1000 * RT_PERIOD)) + OFFSET_ns;

    metrics_data->start = now;

    next = now;
    while ( !shutdown )
    {
        next = tsAddus( next, RT_PERIOD );

        // Note this simple example do not handle "remain"
        // if delay end before elapsed time (like signal etc.. case).
        int err = clock_nanosleep( CLOCK_MONOTONIC, flags, &next, &remain );

        clock_gettime( CLOCK_MONOTONIC, &now );
        latency_us = tsDiffus( next, now );
        last = next;

        periodic_application_code();
        if ( !UART_METRICS ) {
            update_metrics( metrics_data, latency_us, now );
        }

        if ( runtime ) {
            if ( --runtime == 0 ) {
                shutdown = 1;
                periodic_application_code();   // Send one character to unblock receiver
            }
        }
    }
    return NULL;
}


#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

void * threadUartRx( char *tty_dev )
{
    #define TTY_DEV  "/dev/ttyUSB0"
//  #define TTY_DEV  "/dev/ttyS5"

    fd = open( TTY_DEV, O_RDWR | O_NOCTTY | O_SYNC );
//  fd = open( TTY_DEV, O_RDWR | O_NOCTTY | O_NDELAY );

    #if 0
    if ( set_interface_attribs(fd, B115200, 0) )
//  if ( InitCOM(fd, B115200, 0) )
    {
        exit -1;
    }
    set_blocking (fd, 1);
    #endif

    char buffer[1000];

    read( fd, buffer, sizeof(buffer) );   // flush buffer

    while ( !shutdown )
    {
        struct timespec  now;
        int              latency_us;

        read( fd, buffer, 1 );

        clock_gettime( CLOCK_MONOTONIC, &now );
        latency_us = tsDiffus( last, now );

        if ( UART_METRICS ) {
            update_metrics( metrics_data, latency_us, now );
        }
    }
}


int start_RT_thread( int rt_policy, int rt_priority, void *thread_func, void *arg )
{
    struct sched_param  parm;
    pthread_attr_t      attr;

    pthread_attr_init( &attr );
    pthread_attr_setinheritsched( &attr, PTHREAD_EXPLICIT_SCHED );
    pthread_attr_setschedpolicy(  &attr, rt_policy );
    parm.sched_priority = rt_priority;
    pthread_attr_setschedparam( &attr, &parm );

    pthread_t  threadId;
    int        err;

    err = pthread_create( &threadId, &attr, thread_func, arg );
    if ( err ) {
         printf("ERROR to create thread\n");
         return -1;
    }
    return threadId;
}


int run_RT_threads( int seconds )
{
    runtime = TESTtime( seconds );
    metrics_data->reset = 1;

    lock_memory();

    pthread_t  threadId[10];
    int        err = 0;

    threadId[1] = start_RT_thread( RT_POLICY, RT_PRIORITY-1, &threadUartRx, NULL );
    usleep( 100000 );   // Give time to flush serial port buffer
    threadId[0] = start_RT_thread( RT_POLICY, RT_PRIORITY-0, &threadFunc,  &thread_args );  thread_args.thread_number++;

//  threadId[1] = start_RT_thread( RT_POLICY, RT_PRIORITY-1, &threadFunc,  &thread_args );  thread_args.thread_number++;
//  threadId[2] = start_RT_thread( RT_POLICY, RT_PRIORITY-2, &threadFunc,  &thread_args );  thread_args.thread_number++;

    err |= pthread_join( threadId[0], NULL );
    err |= pthread_join( threadId[1], NULL );
//  err |= pthread_join( threadId[2], NULL );

    if ( err ) {
         printf("ERROR to join thread\n");
         return -1;
    }
    return 0;
}

//---------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
    int    status          = 0;
    int    seconds         = 0;    // Default (==0) run for ever....

//  pid_t  pid             = getpid();
//  int    currentPriority = getpriority( PRIO_PROCESS, pid );
//  int    newPriority     = PRIORITY;

//  setpriority( PRIO_PROCESS, pid, newPriority );

    check_root();
    metrics_data = shmOpen( "", SHM_METRICS, sizeof(metrics_t) );
    if ( !metrics_data ) {
        printf("ERROR: Can not open shared memory\n");
        exit( -1 );
    }

    if ( argc > 1 ) {

        int  sec = atoi( argv[1] );

        if ( sec > 0 ) {
            seconds = sec;;
        }
        else if ( !strcmp(argv[1],"-s") ) {
            UART_METRICS = 1;
        }
        else if ( !strcmp(argv[1],"-r") ) {
            metrics_data->reset = 1;
            goto done;
        }
        else if ( !strcmp(argv[1],"-p") ) {
            print_metrics( metrics_data );
            goto done;
        }
        else {
            printf("ERROR Unknown command line argument: %s\n", argv[1]);
            goto done;
        }
    }
    status = run_RT_threads( seconds );

done:
    munmap( metrics_data, sizeof(metrics_t) );

    #if 0 //FALSE
    // We leave shared memory files open for other processes!
    // Close hared memory file(s): /dev/shm/....
//  shm_unlink(SHM_METRICS);
    #endif

    return status;
}
