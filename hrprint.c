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

#define  SHM_METRICS   "RT_METRICS"      // Shared memory file name

int  RT_PERIOD = 1000;


int main( int argc, char *argv[] )
{
    metrics_t  *metrics_data;
    
    check_root();
    metrics_data = shmOpen( "", SHM_METRICS, sizeof(metrics_t) );
    if ( !metrics_data ) {
        printf("ERROR: Can not open shared memory\n");
        exit( -1 );
    }

    if ( argc > 1 ) {
        if ( !strcmp(argv[1],"-r") ) {
            metrics_data->reset = 1;
        }
        else {
            printf("Unknown command line arg: %s\n", argv[1]);
        }
    }
    else {
        print_metrics( metrics_data );
    }
    munmap( metrics_data, sizeof(metrics_t) );

    #if 0 //FALSE
    // We leave shared memory files open for other processes!
    // Close hared memory file(s): /dev/shm/....
//  shm_unlink(SHM_METRICS);
    #endif

    return 0;
}
