
#ifndef __SPI_H
#define __SPI_H

#include  "type.h"
#include "hc32_ddl.h"


#define HIGH	1
#define LOW		0



void WIZ_SPI_Init(void);
void w5100s_cs_select(void); //select chip
void w5100s_cs_deselect(void); //deselect chip
uint8_t w5100s_spi_readbyte(void);
void w5100s_spi_writebyte(uint8_t wb);


int w5100s_network_info_init(void);
void w5100s_network_info_show(void);
void Reset_W5100S(void);
int select_socks(int lsn, int csn, int *outsns);

#endif
