#ifndef COMMON_HEXP_
#define COMMON_HEXP_

//#define UART_DEBUG
//#define BTMSG_DEBUG
#define MAIN_DEBUG


/*
#define E(x)  \
	err = x;   \
	if (err < 0)	\
	{ \
		goto Fin;	\
	} \
		  \

*/
#define InitErr int err = 0
#define FinErr  \
	Fin:  \
	return err; \
	  \

#endif
