/* ###################################################################
**     Filename    : Events.h
**     Project     : Meu1oARM
**     Processor   : MK64FX512VLL12
**     Component   : Events
**     Version     : Driver 01.00
**     Compiler    : GNU C Compiler
**     Date/Time   : 2021-02-19, 23:41, # CodeGen: 0
**     Abstract    :
**         This is user's event module.
**         Put your event handler code here.
**     Settings    :
**     Contents    :
**         No public methods
**
** ###################################################################*/
/*!
** @file Events.h
** @version 01.00
** @brief
**         This is user's event module.
**         Put your event handler code here.
*/         
/*!
**  @addtogroup Events_module Events module documentation
**  @{
*/         

#ifndef __Events_H
#define __Events_H
/* MODULE Events */

#include "fsl_device_registers.h"
#include "clockMan1.h"
#include "pin_mux.h"
#include "osa1.h"
#include "gpio1.h"
#include "memoryCard1.h"
#include "fsl_sdhc1.h"
#include "pitTimer1.h"
#include "eNet1.h"
#include "KSDK1.h"
#include "pitTimer4.h"
#include "adConv1.h"
#include "pitTimer2.h"
#include "pitTimer3.h"
#include "adConv2.h"

#ifdef __cplusplus
extern "C" {
#endif 


#ifdef pitTimer1_IDX
/*
** ===================================================================
**     Interrupt handler : PIT0_IRQHandler
**
**     Description :
**         User interrupt service routine. 
**     Parameters  : None
**     Returns     : Nothing
** ===================================================================
*/
void PIT0_IRQHandler(void);
#else
  /* This IRQ handler is not used by pitTimer1 component. The purpose may be
   * that the component has been removed or disabled. It is recommended to 
   * remove this handler because Processor Expert cannot modify it according to 
   * possible new request (e.g. in case that another component uses this
   * interrupt vector). */
  #warning This IRQ handler is not used by pitTimer1 component.\
           It is recommended to remove this because Processor Expert cannot\
           modify it according to possible new request.
#endif


/*
** ===================================================================
**     Interrupt handler : ADC0_IRQHandler
**
**     Description :
**         User interrupt service routine. 
**     Parameters  : None
**     Returns     : Nothing
** ===================================================================
*/
void ADC0_IRQHandler(void);

#ifdef pitTimer2_IDX
/*
** ===================================================================
**     Interrupt handler : PIT1_IRQHandler
**
**     Description :
**         User interrupt service routine. 
**     Parameters  : None
**     Returns     : Nothing
** ===================================================================
*/
void PIT1_IRQHandler(void);
#else
  /* This IRQ handler is not used by pitTimer2 component. The purpose may be
   * that the component has been removed or disabled. It is recommended to 
   * remove this handler because Processor Expert cannot modify it according to 
   * possible new request (e.g. in case that another component uses this
   * interrupt vector). */
  #warning This IRQ handler is not used by pitTimer2 component.\
           It is recommended to remove this because Processor Expert cannot\
           modify it according to possible new request.
#endif

/*
** ===================================================================
**     Interrupt handler : PORTB_IRQHandler
**
**     Description :
**         User interrupt service routine. 
**     Parameters  : None
**     Returns     : Nothing
** ===================================================================
*/
void PORTB_IRQHandler(void);

/*
** ===================================================================
**     Interrupt handler : PORTD_IRQHandler
**
**     Description :
**         User interrupt service routine. 
**     Parameters  : None
**     Returns     : Nothing
** ===================================================================
*/
void PORTD_IRQHandler(void);

/*
** ===================================================================
**     Interrupt handler : ADC1_IRQHandler
**
**     Description :
**         User interrupt service routine. 
**     Parameters  : None
**     Returns     : Nothing
** ===================================================================
*/
void ADC1_IRQHandler(void);

/*
** ===================================================================
**     Callback    : fsl_sdhc1_OnCardDetect0
**     Description : This callback function is called when card is
**     inserted or removed.
**     Parameters  :
**       inserted - Specifies if card was inserted or removed.
**     Returns : Nothing
** ===================================================================
*/
void fsl_sdhc1_OnCardDetect0(bool inserted);

/*
** ===================================================================
**     Callback    : fsl_sdhc1_OnCardInterrupt0
**     Description : This callback function is called when card
**     interrupt occurs.
**     Parameters  : None
**     Returns : Nothing
** ===================================================================
*/
void fsl_sdhc1_OnCardInterrupt0(void);

/*
** ===================================================================
**     Callback    : fsl_sdhc1_OnCardBlockGap0
**     Description : This callback function is called when card block
**     gap occurs.
**     Parameters  : None
**     Returns : Nothing
** ===================================================================
*/
void fsl_sdhc1_OnCardBlockGap0(void);

/*
** ===================================================================
**     Interrupt handler : PORTE_IRQHandler
**
**     Description :
**         User interrupt service routine. 
**     Parameters  : None
**     Returns     : Nothing
** ===================================================================
*/
void PORTE_IRQHandler(void);
void check_card_inserted(void);

#ifdef pitTimer3_IDX
/*
** ===================================================================
**     Interrupt handler : PIT2_IRQHandler
**
**     Description :
**         User interrupt service routine. 
**     Parameters  : None
**     Returns     : Nothing
** ===================================================================
*/
void PIT2_IRQHandler(void);
#else
  /* This IRQ handler is not used by pitTimer3 component. The purpose may be
   * that the component has been removed or disabled. It is recommended to 
   * remove this handler because Processor Expert cannot modify it according to 
   * possible new request (e.g. in case that another component uses this
   * interrupt vector). */
  #warning This IRQ handler is not used by pitTimer3 component.\
           It is recommended to remove this because Processor Expert cannot\
           modify it according to possible new request.
#endif

#ifdef pitTimer4_IDX
/*
** ===================================================================
**     Interrupt handler : PIT3_IRQHandler
**
**     Description :
**         User interrupt service routine. 
**     Parameters  : None
**     Returns     : Nothing
** ===================================================================
*/
void PIT3_IRQHandler(void);
#else
  /* This IRQ handler is not used by pitTimer4 component. The purpose may be
   * that the component has been removed or disabled. It is recommended to 
   * remove this handler because Processor Expert cannot modify it according to 
   * possible new request (e.g. in case that another component uses this
   * interrupt vector). */
  #warning This IRQ handler is not used by pitTimer4 component.\
           It is recommended to remove this because Processor Expert cannot\
           modify it according to possible new request.
#endif

/* END Events */

#ifdef __cplusplus
}  /* extern "C" */
#endif 

#endif 
/* ifndef __Events_H*/
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
