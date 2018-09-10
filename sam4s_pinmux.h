#ifndef SAM4S_PINMUX_H
#define SAM4S_PINMUX_H

/* macros for pin numbers on ports A, B, C */
#define SAM4S_PINMUX_PA(n) n
#define SAM4S_PINMUX_PB(n) ((n)+32)

#if 0
#define SAM4S_PINMUX_PC(n) ((n)+64)
#endif

extern void
sam4s_pinmux_init();

enum sam4s_pinmux_function {
	SAM4S_PINMUX_A = 0, /* to match pio->PIO_ABCDSR[0] and ...[1] */
	SAM4S_PINMUX_B = 1,
	SAM4S_PINMUX_C = 2,
	SAM4S_PINMUX_D = 3,
	SAM4S_PINMUX_GPIO
};

/* set pin function to GPIO or special function A..D */
extern void
sam4s_pinmux_function(int pin, enum sam4s_pinmux_function func);

enum sam4s_pinmux_pull {
	SAM4S_PINMUX_PULLNONE,
	SAM4S_PINMUX_PULLUP,
	SAM4S_PINMUX_PULLDOWN
};

/* enable pullup/pulldown on pin */
extern void sam4s_pinmux_pull(int pin, enum sam4s_pinmux_pull what);

/* configure pin as open drain */
extern void sam4s_pinmux_open_drain(int pin, int onoff);

/* set output state */
extern void sam4s_pinmux_gpio_set(int pin, int val);

/* set output enable */
extern void sam4s_pinmux_gpio_oe(int pin, int oe);

/* read pin input */
extern int sam4s_pinmux_gpio_get(int pin);

#endif
