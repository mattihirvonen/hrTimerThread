//
// File:  suppfunc.h
//
// Various application support functions
//


#ifndef  SUPPFUNC_H
#define  SUPPFUNC_H

#ifdef __cplusplus
extern "C" {
#endif


#define HISTOSIZE  1001

typedef struct  {
	int  reset;
	//
    int  histogram[HISTOSIZE];
    int  long_sum_us;
    int  long_count;
    int  max_lat;
    int  sum_us;
    int  counter;
    //
    struct timespec start, stop;
} metrics_t;


void lock_memory( void );

struct timespec  diff(  struct timespec start, struct timespec end );
struct timespec  addus( struct timespec timestamp, int us );

int diffus( struct timespec start, struct timespec end );


#ifdef __cplusplus
}
#endif

#endif // SUPPFUNC_H
