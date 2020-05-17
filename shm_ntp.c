#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define NTPD_BASE	0x4e545030	/* "NTP0" */
#define SHM_UNIT	0	/* SHM driver unit number (0..3) */
#define PPS_MIN_FIXES	3	/* # fixes to wait for before shipping PPS */

// to debug, try looking at the live segments this way
// *  ipcs -m
// cat /proc/sysvipc/shm

struct shmTime
{
    int mode;	/* 0 - if valid set
    		 *       use values,
                 *       clear valid
     		 * 1 - if valid set
     		 *       if count before and after read of values is equal,
     		 *         use values
     		 *       clear valid
     		 */
    volatile int count;
    time_t clockTimeStampSec;
    int clockTimeStampUSec;
    time_t receiveTimeStampSec;
    int receiveTimeStampUSec;
    int leap;		/* not leapsecond offset, a notification code */
    int precision;	/* log(2) of source jitter */
    int nsamples;	/* not used */
    volatile int valid;
    unsigned clockTimeStampNSec;     /* Unsigned ns timestamps */
    unsigned receiveTimeStampNSec;   /* Unsigned ns timestamps */
    int dummy[8];
};
    
