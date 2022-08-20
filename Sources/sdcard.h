/*
 * sdcard.h
 *
 *  Created on: 10/02/2022
 *      Author: 89247469
 */

#ifndef SOURCES_SDCARD_H_
#define SOURCES_SDCARD_H_


/*
 * sdcard.c
 *
 *  Created on: 10/02/2022
 *      Author: 89247469
 */

#include "Cpu.h"
#include "Events.h"
#include "clockMan1.h"
#include "pin_mux.h"
#include "osa1.h"
#include "gpio1.h"
#include "memoryCard1.h"
#include "fsl_sdhc1.h"

#include <string.h>
#include "ff.h"

#define SD_DRIVE_NUMBER     1

void append(char *, char *);

#endif /* SOURCES_SDCARD_H_ */
