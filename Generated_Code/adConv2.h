/* ###################################################################
**     This component module is generated by Processor Expert. Do not modify it.
**     Filename    : adConv2.h
**     Project     : Meu1oARM
**     Processor   : MK64FX512VLL12
**     Component   : fsl_adc16
**     Version     : Component 1.3.0, Driver 01.00, CPU db: 3.00.000
**     Repository  : KSDK 1.3.0
**     Compiler    : GNU C Compiler
**     Date/Time   : 2021-05-22, 12:54, # CodeGen: 81
**     Contents    :
**         ADC16_DRV_GetAutoCalibrationParam     - adc16_status_t ADC16_DRV_GetAutoCalibrationParam(uint32_t...
**         ADC16_DRV_SetCalibrationParam         - adc16_status_t ADC16_DRV_SetCalibrationParam(uint32_t instance,const...
**         ADC16_DRV_StructInitUserConfigDefault - adc16_status_t ADC16_DRV_StructInitUserConfigDefault(adc16_converter_config_t...
**         ADC16_DRV_Init                        - adc16_status_t ADC16_DRV_Init(uint32_t instance,const...
**         ADC16_DRV_Deinit                      - adc16_status_t ADC16_DRV_Deinit(uint32_t instance);
**         ADC16_DRV_ConfigHwCompare             - adc16_status_t ADC16_DRV_ConfigHwCompare(uint32_t instance,const...
**         ADC16_DRV_ConfigHwAverage             - adc16_status_t ADC16_DRV_ConfigHwAverage(uint32_t instance,const...
**         ADC16_DRV_SetChnMux                   - void ADC16_DRV_SetChnMux(uint32_t instance,adc16_chn_mux_mode_t chnMuxMode);
**         ADC16_DRV_ConfigConvChn               - adc16_status_t ADC16_DRV_ConfigConvChn(uint32_t instance,uint32_t...
**         ADC16_DRV_WaitConvDone                - void ADC16_DRV_WaitConvDone(uint32_t instance,uint32_t chnGroup);
**         ADC16_DRV_PauseConv                   - void ADC16_DRV_PauseConv(uint32_t instance,uint32_t chnGroup);
**         ADC16_DRV_GetConvValueRAW             - uint16_t ADC16_DRV_GetConvValueRAW(uint32_t instance,uint32_t chnGroup);
**         ADC16_DRV_GetConvValueSigned          - int16_t ADC16_DRV_GetConvValueSigned(uint32_t instance,uint32_t chnGroup);
**         ADC16_DRV_GetConvFlag                 - bool ADC16_DRV_GetConvFlag(uint32_t instance,adc16_flag_t flag);
**         ADC16_DRV_GetChnFlag                  - bool ADC16_DRV_GetChnFlag(uint32_t instance,uint32_t chnGroup,adc16_flag_t...
**
**     Copyright : 1997 - 2015 Freescale Semiconductor, Inc. 
**     All Rights Reserved.
**     
**     Redistribution and use in source and binary forms, with or without modification,
**     are permitted provided that the following conditions are met:
**     
**     o Redistributions of source code must retain the above copyright notice, this list
**       of conditions and the following disclaimer.
**     
**     o Redistributions in binary form must reproduce the above copyright notice, this
**       list of conditions and the following disclaimer in the documentation and/or
**       other materials provided with the distribution.
**     
**     o Neither the name of Freescale Semiconductor, Inc. nor the names of its
**       contributors may be used to endorse or promote products derived from this
**       software without specific prior written permission.
**     
**     THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
**     ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
**     WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
**     DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
**     ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
**     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
**     LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
**     ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
**     (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
**     SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**     
**     http: www.freescale.com
**     mail: support@freescale.com
** ###################################################################*/
/*!
** @file adConv2.h
** @version 01.00
*/         
/*!
**  @addtogroup adConv2_module adConv2 module documentation
**  @{
*/         
#ifndef __adConv2_H
#define __adConv2_H
/* MODULE adConv2. */

/* Include inherited beans */
#include "clockMan1.h"

#include "Cpu.h"

/*! @brief Device instance number */
#define adConv2_IDX ADC1_IDX
/*! @brief Device instance number for backward compatibility */
#define FSL_ADCONV2 adConv2_IDX

/*! @brief ADC configuration declaration */
extern const adc16_converter_config_t adConv2_InitConfig0;
    
/*! @brief HW compare configuration declaration */
extern const adc16_hw_cmp_config_t adConv2_HwConfig0;
    
/*! @brief Channel configuration declaration */
extern const adc16_chn_config_t adConv2_ChnConfig0;

#endif
/* ifndef __adConv2_H */
/*!
** @}
*/
/*
** ###################################################################
**
**     This file was created by Processor Expert 10.5 [05.21]
**     for the Freescale Kinetis series of microcontrollers.
**
** ###################################################################
*/
