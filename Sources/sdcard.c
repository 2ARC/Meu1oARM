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

#include "sdcard.h"
volatile bool cartao = false;
volatile bool led7 = false;
extern volatile bool cardInserted;

void append(char *fileNameAndPath, char *text)
{
    FATFS fileSystem;

    // mount SD card
    FRESULT fResult = f_mount(SD_DRIVE_NUMBER, &fileSystem);

    if (fResult == FR_OK)
    {
        fResult = f_chdrive(SD_DRIVE_NUMBER);

        if (fResult == FR_OK)
        {
            FIL file;
            static char pathWithDriveLetter[255];
            sprintf(pathWithDriveLetter, "%u:%s", SD_DRIVE_NUMBER, fileNameAndPath);

            /* Open a text file */
            fResult = f_open(&file, pathWithDriveLetter, FA_OPEN_ALWAYS | FA_WRITE);
            if (fResult == FR_OK)
            {
                /* Move to end of the file to append data */
                fResult = f_lseek(&file, file.fsize);

                /* Write to the file */
                uint32_t bytesWritten;
                fResult = f_write(&file, text, strlen(text), &bytesWritten);
            }
            /* Close the file */
            fResult = f_close(&file);
        }

        // unmount SD card
        fResult = f_mount(SD_DRIVE_NUMBER, NULL);
    }
}
