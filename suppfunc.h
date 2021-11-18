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

int             tsDiffus( struct timespec start, struct timespec end );
struct timespec  tsDiff(  struct timespec start, struct timespec end );
struct timespec  tsAddus( struct timespec timestamp, int us );
struct timespec  tsSubus( struct timespec timestamp, int us );

void update_metrics( metrics_t *metrics, int latency_us, struct timespec now );
void print_metrics(  metrics_t *metrics );

void *shmOpen( char *txt, char *shmName, size_t shmSize );


#ifdef __cplusplus
}
#endif

#endif // SUPPFUNC_H
