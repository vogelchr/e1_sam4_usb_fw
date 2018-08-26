#include "gps_steer.h"
#include "sam4s_clock.h"
#include "sam4s_timer.h"
#include "sam4s_dac.h"

#include <sam4s4c.h>

#include <unistd.h>
#include <stdio.h>

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#define GPS_STEER_NOMINAL_CLKS     (F_MCK_HZ / 2)
#define GPS_STEER_DAC_PERIODS      4
#define GPS_STEER_DAC_RANGE       (SAM4S_DAC_RANGE*1024)
#define GPS_STEER_DAC_MAXVAL      (GPS_STEER_DAC_RANGE-1)


enum gps_steer_mode {
	GPS_STEER_INIT,
	GPS_STEER_DAC_MIN,
	GPS_STEER_DAC_MAX,
	GPS_STEER_FREQ_DISCIPLINE,
	GPS_STEER_PHASE_DISCIPLINE
};

static const char * gps_steer_mode_names[] = {
	"INIT", "DAC_MIN", "DAC_MAX", "FREQ", "PHASE"
};

static enum gps_steer_mode gps_steer_mode;
static unsigned int gps_steer_mode_cnt;

#define GPS_STEER_HAVE_DAC_CALIB (1<<0)

static uint32_t gps_steer_flags = 0; // GPS_STEER_HAVE_DAC_CALIB;

/* statistics */
static uint32_t gps_steer_last_ts_tick;
static uint32_t gps_steer_last_ts_capt;
static int32_t  gps_steer_last_ts_offs;

/* DAC parameters */
/* offset of capture clocks from GPS_STEER_NOMINAL_CLKS summed over GPS_STEER_DAC_PERIODS */
static int32_t gps_steer_dac_min_offs;  /* sum of offsets when DAC at min */
static int32_t gps_steer_dac_max_offs;  /* sum of offsets when DAC at max */
static uint32_t gps_steer_dac;          /* current DAC value */
static int32_t gps_steer_dac_per_offs;  /* DAC correction per capture offset */
static uint32_t gps_steer_dac_center;   /* integrator for DAC center value */

static void
gps_steer_reset()
{
	gps_steer_mode = GPS_STEER_INIT;
	gps_steer_mode_cnt = 2;  /* first two timestamps are shite */

	gps_steer_last_ts_tick=sam4s_clock_tick;
	gps_steer_last_ts_capt=0;
}

static void
gps_steer_dac_set(uint32_t v)
{
	if (v >= GPS_STEER_DAC_MAXVAL)
		v = GPS_STEER_DAC_MAXVAL;
	gps_steer_dac = v;
	sam4s_dac_update(0, v >> 10);
}

void
gps_steer_init()
{
	sam4s_dac_init();
	sam4s_timer_init();
	gps_steer_dac_set(GPS_STEER_DAC_RANGE / 2);
	gps_steer_reset();
}

void
gps_steer_poll()
{
	uint32_t ts_now_tick =  sam4s_clock_tick;
	uint32_t ts_delta_tick = ts_now_tick - gps_steer_last_ts_tick;
	uint32_t ts_capt_rise;
	int32_t  ts_capt_delta_offs;
	unsigned int flags;

	int32_t dacv;

	flags = sam4s_timer_capt_poll(&ts_capt_rise, NULL);
	/* no timestamp received? Check for timeout. Then just return. */
	if (!(flags & SAM4S_TIMER_CAPT_RISING)) {
		if (ts_delta_tick > 125) {
			gps_steer_reset();
			printf("gps_steer: no pps!\r\n");
		}
		return;
	}

	if (ts_delta_tick < 75) {
		gps_steer_reset();
		printf("gps_steer: runt pulse!\r\n");
		return;
	}

	/* modes use this counter differently, but all expect it to count
	   down, one per captured pulse */
	if (gps_steer_mode_cnt > 0)
		gps_steer_mode_cnt--;

	/* delta between captures, and offset from GPS_STEER_NOMINAL CLKS */
	ts_capt_delta_offs = (int)(ts_capt_rise - gps_steer_last_ts_capt)
		- GPS_STEER_NOMINAL_CLKS;

	/* Update "last" timestamp to be used for next pulse, depending on mode.
	   In phase disciplining we just advance by the nominal clk rate. */
	if (gps_steer_mode == GPS_STEER_PHASE_DISCIPLINE)
		gps_steer_last_ts_capt += GPS_STEER_NOMINAL_CLKS;
	else
		gps_steer_last_ts_capt = ts_capt_rise;

	/* wall clock time */
	gps_steer_last_ts_tick = ts_now_tick;

	if (gps_steer_mode != GPS_STEER_INIT)
		printf("GPS:%s(%d) cnt=%d delta=%+5ld dac=%8ld (dac-center=%ld)",
			gps_steer_mode_names[gps_steer_mode], gps_steer_mode,
			gps_steer_mode_cnt, ts_capt_delta_offs, gps_steer_dac,
			gps_steer_dac - gps_steer_dac_center);

	switch (gps_steer_mode) {

	/* just throw away two pulses to get timestamp calculations correct */
	case GPS_STEER_INIT:
		if (!gps_steer_mode_cnt) {
			if (gps_steer_flags) {
				gps_steer_mode = GPS_STEER_FREQ_DISCIPLINE;
				gps_steer_dac_set(gps_steer_dac_center);
			} else {
				gps_steer_mode = GPS_STEER_DAC_MIN;
				gps_steer_mode_cnt=GPS_STEER_DAC_PERIODS+2;
			}
		}
		break;

	/* move the DAC to minimum value (for calibration) */
	case GPS_STEER_DAC_MIN:
		if (gps_steer_mode_cnt >= GPS_STEER_DAC_PERIODS) {
			gps_steer_dac_set(0);
			gps_steer_dac_min_offs = 0;
			break;
		}
		gps_steer_dac_min_offs += ts_capt_delta_offs;
		if (!gps_steer_mode_cnt) {
			gps_steer_mode = GPS_STEER_DAC_MAX;
			gps_steer_mode_cnt=GPS_STEER_DAC_PERIODS+2;
		}
		break;

	/* move the DAC to maximum value */
	case GPS_STEER_DAC_MAX:
		if (gps_steer_mode_cnt >= GPS_STEER_DAC_PERIODS) {
			gps_steer_dac_set(GPS_STEER_DAC_MAXVAL);
			gps_steer_dac_max_offs = 0;
			break;
		}
		gps_steer_dac_max_offs += ts_capt_delta_offs;
		if (!gps_steer_mode_cnt) {
			int32_t dac_pps_offs_span;

			/* gps_steer_dac_min_offs must be < 0 and
			   gps_steer_dac_max_offs must be > 0 !!! */

			printf("\r\n\r\n# After integrating over %d periods ech:\r\n",
				GPS_STEER_DAC_PERIODS);
			printf("# PPS offset: %ld counts (sum) at DAC min.\r\n",
				gps_steer_dac_min_offs);
			printf("# PPS offset: %ld counts (sum) at DAC max.\r\n",
				gps_steer_dac_max_offs);

			dac_pps_offs_span = gps_steer_dac_max_offs - gps_steer_dac_min_offs;

			printf("# -> Total span %ld counts.\r\n",
				dac_pps_offs_span);

			/* DAC steps per offset */
			gps_steer_dac_per_offs = GPS_STEER_DAC_PERIODS *
				GPS_STEER_DAC_RANGE / dac_pps_offs_span;

			printf("# -> %ld DAC counts per pps offset.\r\n",
				gps_steer_dac_per_offs);

			/* target to steer the DAC to right now */
			gps_steer_dac_center = gps_steer_dac_per_offs *
				- gps_steer_dac_min_offs / GPS_STEER_DAC_PERIODS;

			gps_steer_dac = gps_steer_dac_center;
			printf("# -> DAC target %lu digits\r\n\r\n", gps_steer_dac);

			gps_steer_dac_set(gps_steer_dac_center);
			gps_steer_mode = GPS_STEER_FREQ_DISCIPLINE;
		}
		break;

	case GPS_STEER_FREQ_DISCIPLINE:
		/* if we are out of "find adjustment" territory bump up
		   mode cycle counter */
		if (ts_capt_delta_offs > 5 || ts_capt_delta_offs < -5)
			gps_steer_mode_cnt = 30;

		/* integration part is 25% or proportional part */
		gps_steer_dac_center -= ts_capt_delta_offs * gps_steer_dac_per_offs / 4;
		dacv = gps_steer_dac_center - gps_steer_dac_per_offs * ts_capt_delta_offs * 3 / 4;

		gps_steer_dac_set(
			MAX((int)0, MIN((int)GPS_STEER_DAC_MAXVAL, (int)dacv)));

		if (!gps_steer_mode_cnt)
			gps_steer_mode = GPS_STEER_PHASE_DISCIPLINE;
		break;
	/* phase disciplining */
	case GPS_STEER_PHASE_DISCIPLINE:
		/* if phase offset goes out of range, resort back to freq disciplining */
		if (ts_capt_delta_offs > 50 || ts_capt_delta_offs < -50) {
			gps_steer_mode = GPS_STEER_FREQ_DISCIPLINE;
			gps_steer_mode_cnt = 30;
			break;
		}

		gps_steer_dac_center -= ts_capt_delta_offs * gps_steer_dac_per_offs / 4;
		dacv = gps_steer_dac_center - gps_steer_dac_per_offs * ts_capt_delta_offs * 3 / 4;
		dacv = MAX((int)0, MIN((int)GPS_STEER_DAC_MAXVAL, (int)dacv));

		gps_steer_dac_set(dacv);
	}

	gps_steer_last_ts_offs = ts_capt_delta_offs;

	printf("\r\n");
}
