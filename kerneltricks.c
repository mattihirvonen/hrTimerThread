//  gcc hrtimer.c -O2 -o hrtimer -lrt -lpthread
//
// #include <iostream>
// #include <time.h>
// using namespace std;
//
// https://stackoverflow.com/questions/6749621/how-to-create-a-high-resolution-timer-in-linux-to-measure-program-performance
//

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>

void init_kernel_tricks( void );
void restore_kernel_tricks( void );

//---------------------------------------------------------------------------
// Tricks from "cyclictest.c"

#include <string.h>
#include <sys/stat.h>      // open()
#include <fcntl.h>         // O_READONLY, etc....
#include <errno.h>
#include <sys/utsname.h>   // uname()
#include <sys/mman.h>      // mlockall(), munlockall()

#define KVARS		32
#define KVARNAMELEN	32
#define KVALUELEN		32
#define MAX_PATH		256

enum kernelversion {
	KV_NOT_SUPPORTED,
	KV_26_LT18,
	KV_26_LT24,
	KV_26_33,
	KV_30
};

static char *procfileprefix = "/proc/sys/kernel/";
static char *fileprefix;

static char functiontracer[MAX_PATH];
static char traceroptions[MAX_PATH];
static int  kernelversion;
static int  lockall = 0;
static int  laptop  = 0;

static int     latency_target_fd    = -1;
static int32_t latency_target_value =  0;


/* Backup of kernel variables that we modify */
static struct kvars {
	char name[KVARNAMELEN];
	char value[KVALUELEN];
} kv[KVARS];


static void warn( char *txt )
{
    printf("WARNING: %s", txt );
}


//static void err_msg_n( int errorNr, char *txt, ... )

static void err_msg_n( int errorNr, char *txt, int value )
{
    printf( txt, value );
    perror( txt );
    exit( -1 );
}


static void err_msg_2( int errorNr, char *txt )
{
    perror( txt );
}


/* Latency trick
 * if the file /dev/cpu_dma_latency exists,
 * open it and write a zero into it. This will tell
 * the power management system not to transition to
 * a high cstate (in fact, the system acts like idle=poll)
 * When the fd to /dev/cpu_dma_latency is closed, the behavior
 * goes back to the system default.
 *
 * Documentation/power/pm_qos_interface.txt
 */
static void set_latency_target(void)
{
	struct stat s;
	int err;

	if (laptop) {
		warn("not setting cpu_dma_latency to save battery power\n");
		return;
	}

	errno = 0;
	err = stat("/dev/cpu_dma_latency", &s);
	if (err == -1) {
		err_msg_2(errno, "WARN: stat /dev/cpu_dma_latency failed");
		return;
	}

	errno = 0;
	latency_target_fd = open("/dev/cpu_dma_latency", O_RDWR);
	if (latency_target_fd == -1) {
		err_msg_2(errno, "WARN: open /dev/cpu_dma_latency");
		return;
	}

	errno = 0;
	err = write(latency_target_fd, &latency_target_value, 4);
	if (err < 1) {
		err_msg_n(errno, "# error setting cpu_dma_latency to %d!", latency_target_value);
		close(latency_target_fd);
		return;
	}
	printf("# /dev/cpu_dma_latency set to %dus\n", latency_target_value);
}


static int check_kernel(void)
{
	struct utsname kname;
	int maj, min, sub, kv, ret;

	ret = uname(&kname);
	if (ret) {
		fprintf(stderr, "uname failed: %s. Assuming not 2.6\n",
				strerror(errno));
		return KV_NOT_SUPPORTED;
	}
	sscanf(kname.release, "%d.%d.%d", &maj, &min, &sub);
	if (maj == 2 && min == 6) {
		if (sub < 18)
			kv = KV_26_LT18;
		else if (sub < 24)
			kv = KV_26_LT24;
		else if (sub < 28) {
			kv = KV_26_33;
			strcpy(functiontracer, "ftrace");
			strcpy(traceroptions, "iter_ctrl");
		} else {
			kv = KV_26_33;
			strcpy(functiontracer, "function");
			strcpy(traceroptions, "trace_options");
		}
	} else if (maj >= 3) {
		kv = KV_30;
		strcpy(functiontracer, "function");
		strcpy(traceroptions, "trace_options");

	} else
		kv = KV_NOT_SUPPORTED;

	return kv;
}


static int kernvar(int mode, const char *name, char *value, size_t sizeofvalue)
{
	char filename[128];
	int retval = 1;
	int path;
	size_t len_prefix = strlen(fileprefix), len_name = strlen(name);

	if (len_prefix + len_name + 1 > sizeof(filename)) {
		errno = ENOMEM;
		return 1;
	}

	memcpy(filename, fileprefix, len_prefix);
	memcpy(filename + len_prefix, name, len_name + 1);

printf("KERNVAR: %s= %d / %s\n", filename, mode, value );  return 1;
	path = open(filename, mode);
	if (path >= 0) {
		if (mode == O_RDONLY) {
			int got;
			if ((got = read(path, value, sizeofvalue)) > 0) {
				retval = 0;
				value[got-1] = '\0';
			}
		} else if (mode == O_WRONLY) {
			if (write(path, value, sizeofvalue) == sizeofvalue)
				retval = 0;
		}
		close(path);
	}
	return retval;
}


static void setkernvar(const char *name, char *value)
{
	int i;
	char oldvalue[KVALUELEN];

	if (kernelversion < KV_26_33) {
		if (kernvar(O_RDONLY, name, oldvalue, sizeof(oldvalue)))
			fprintf(stderr, "could not retrieve %s\n", name);
		else {
			for (i = 0; i < KVARS; i++) {
				if (!strcmp(kv[i].name, name))
					break;
				if (kv[i].name[0] == '\0') {
					strncpy(kv[i].name, name,
						sizeof(kv[i].name));
					strncpy(kv[i].value, oldvalue,
					    sizeof(kv[i].value));
					break;
				}
			}
			if (i == KVARS)
				fprintf(stderr, "could not backup %s (%s)\n",
					name, oldvalue);
		}
	}
	if (kernvar(O_WRONLY, name, value, strlen(value)))
		fprintf(stderr, "could not set %s to %s\n", name, value);

}


static void restorekernvars(void)
{
	int i;

	for (i = 0; i < KVARS; i++) {
		if (kv[i].name[0] != '\0') {
			if (kernvar(O_WRONLY, kv[i].name, kv[i].value,
			    strlen(kv[i].value)))
				fprintf(stderr, "could not restore %s to %s\n",
					kv[i].name, kv[i].value);
		}
	}
}

//---------------------------------------------------------------------------

void init_kernel_tricks( void )
{
	lockall    = 1;
	fileprefix = procfileprefix;

	/* lock all memory (prevent swapping) */
	if (lockall) {
		if (mlockall(MCL_CURRENT|MCL_FUTURE) == -1) {
			perror("mlockall");
			goto out;
		}
	}
	/* use the /dev/cpu_dma_latency trick if it's there */
//	set_latency_target();

	kernelversion = check_kernel();

	if (kernelversion == KV_NOT_SUPPORTED) {
		warn("Running on unknown kernel version...YMMV\n");
	}
	setkernvar("preempt_thresh", "0");
	setkernvar("preempt_max_latency", "0");

	return;

out:	exit( -1 );
}


void restore_kernel_tricks( void )
{
   /* unlock everything */
    if (lockall) {
	munlockall();
    }
    /* Be a nice program, cleanup */
    if (kernelversion < KV_26_33) {
	restorekernvars();
    }
    /* close the latency_target_fd if it's open */
    if (latency_target_fd >= 0) {
	close(latency_target_fd);
    }
}
