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


#define HISTOSIZE  2001

typedef struct  {
    int  reset;     // Write non zero value resets metrics
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

void check_root( void );
void lock_memory( void );

struct timespec  diff_ts(  struct timespec start, struct timespec end );
struct timespec  addus( struct timespec timestamp, int us );
struct timespec  subus( struct timespec timestamp, int us );

int diffus( struct timespec start, struct timespec end );

void update_metrics( metrics_t *metrics, int latency_us, struct timespec now );
void print_metrics(  metrics_t *metrics );

void *shmOpen( char *txt, char *shmName, size_t shmSize );


#ifdef __cplusplus
}
#endif

#endif // SUPPFUNC_H
