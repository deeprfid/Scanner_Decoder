#include <stdio.h>
#include <rt_misc.h>

#pragma import(__use_no_semihosting_swi)


extern  int UART0_putchar(int ch);


struct __FILE { int handle; /* Add whatever you need here */ };
FILE __stdout;

/*
int fputc(int ch, FILE *f) {
//  return (UART0_putchar(ch));
	return ch;
}
*/

int ferror(FILE *f) {
  /* Your implementation of ferror */
  return EOF;
}


void _ttywrch(int ch) {
//  UART0_putchar(ch);
}


void _sys_exit(int return_code) {
label:  goto label;  /* endless loop */
}


