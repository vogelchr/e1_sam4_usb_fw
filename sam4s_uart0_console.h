#ifndef SAM4S_UART0_CONSOLE_H
#define SAM4S_UART0_CONSOLE_H

extern void sam4s_uart0_console_init();
extern void sam4s_uart0_console_tx(unsigned char c);
extern int sam4s_uart0_console_rx();

#endif
