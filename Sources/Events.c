/* ###################################################################

**     Filename    : Events.c
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
** @file Events.c
** @version 01.00
** @brief
**         This is user's event module.
**         Put your event handler code here.
*/         
/*!
**  @addtogroup Events_module Events module documentation
**  @{
*/         
/* MODULE Events */

#include "Cpu.h"
#include "Events.h"

#ifdef __cplusplus
extern "C" {
#endif 

/* User includes (#include below this line is not maintained by Processor Expert) */
#include "httpd.h"
#include "sdcard.h"

extern volatile bool cardInserted; /* Flag to indicate a card has been inserted */
extern volatile int valorADC11P;
extern volatile bool buzzer;
extern volatile bool cartao;
extern volatile bool led7;

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
void PIT0_IRQHandler(void)
{
  /* Clear interrupt flag.*/
  PIT_HAL_ClearIntFlag(g_pitBase[pitTimer1_IDX], pitTimer1_CHANNEL);
  /* Write your code here ... */
  NVIC_DisableIRQ(PIT0_IRQn);
  GPIO_DRV_TogglePinOutput(LED6);
  NVIC_EnableIRQ(PIT0_IRQn);
}
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
void ADC0_IRQHandler(void)
{
  /* Write your code here ... */
}

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
void PIT1_IRQHandler(void)
{
  /* Clear interrupt flag.*/
  PIT_HAL_ClearIntFlag(g_pitBase[pitTimer2_IDX], pitTimer2_CHANNEL);
  /* Write your code here ... */
  NVIC_DisableIRQ(PIT1_IRQn);
  ADC16_DRV_ConfigConvChn(1,0,&adConv2_ChnConfig0);
  ADC16_DRV_WaitConvDone(1,0);
  valorADC11P=ADC16_DRV_GetConvValueRAW(1,0);

  if (buzzer == true)
  {
	  buzzer = false;
	  GPIO_DRV_WritePinOutput(BUZZER, 1);
  }
  else
  {
	  GPIO_DRV_WritePinOutput(BUZZER, 0);
  }

  // Watchdog refresh
  WDOG_REFRESH = 0xA602;
  WDOG_REFRESH = 0xB480;

  NVIC_EnableIRQ(PIT1_IRQn);
}
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
void PORTB_IRQHandler(void)
{
  /* Clear interrupt flag.*/
  PORT_HAL_ClearPortIntFlag(PORTB_BASE_PTR);
  /* Write your code here ... */
  NVIC_DisableIRQ(PORTB_IRQn);
  if (!GPIO_DRV_ReadPinInput(INT0))
  {
	  GPIO_DRV_WritePinOutput(ON_BOARD_RELAY, 1);
  }
  if (!GPIO_DRV_ReadPinInput(INT1))
  {
	  GPIO_DRV_WritePinOutput(ON_BOARD_RELAY, 0);
  }
  NVIC_EnableIRQ(PORTB_IRQn);
}

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
void PORTD_IRQHandler(void)
{
  /* Clear interrupt flag.*/
  PORT_HAL_ClearPortIntFlag(PORTD_BASE_PTR);
  /* Write your code here ... */
  NVIC_DisableIRQ(PORTD_IRQn);
  if (!GPIO_DRV_ReadPinInput(INT2))
  {
	  buzzer = true;
  }
  NVIC_EnableIRQ(PORTD_IRQn);
}

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
void ADC1_IRQHandler(void)
{
  /* Write your code here ... */
}

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
void fsl_sdhc1_OnCardDetect0(bool inserted)
{
  /* Write your code here ... */
}

/*
** ===================================================================
**     Callback    : fsl_sdhc1_OnCardInterrupt0
**     Description : This callback function is called when card
**     interrupt occurs.
**     Parameters  : None
**     Returns : Nothing
** ===================================================================
*/
void fsl_sdhc1_OnCardInterrupt0(void)
{
  /* Write your code here ... */
}

/*
** ===================================================================
**     Callback    : fsl_sdhc1_OnCardBlockGap0
**     Description : This callback function is called when card block
**     gap occurs.
**     Parameters  : None
**     Returns : Nothing
** ===================================================================
*/
void fsl_sdhc1_OnCardBlockGap0(void)
{
  /* Write your code here ... */
}

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
void PORTE_IRQHandler(void)
{
	PORT_HAL_ClearPortIntFlag(PORTE_BASE_PTR);
	NVIC_DisableIRQ(PORTE_IRQn);
	if (!GPIO_DRV_ReadPinInput(SD_CARD_DETECT))
	{
		check_card_inserted();
	}
	cartao = true;
	NVIC_EnableIRQ(PORTE_IRQn);
}

void check_card_inserted(void)
{
    uint32_t state = GPIO_DRV_ReadPinInput(SD_CARD_DETECT);
    uint32_t matchState = -1;

    // Debounce input
    do
    {
        for (int i = 0; i < 0xFFFF; i++)
        {
            __asm("nop");
        }
        matchState = state;
        state = GPIO_DRV_ReadPinInput(SD_CARD_DETECT);
    }
    while (state != matchState);

    // Set card state
    cardInserted = (state == 1);

    fsl_sdhc1_OnCardDetect0(cardInserted);
}

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
void PIT2_IRQHandler(void)
{
  /* Clear interrupt flag.*/
  PIT_HAL_ClearIntFlag(g_pitBase[pitTimer3_IDX], pitTimer3_CHANNEL);
  /* Write your code here ... */
  NVIC_DisableIRQ(PIT2_IRQn);

  if (cartao == true)
  {
	  cartao = false;
	  GPIO_DRV_WritePinOutput(LED5, 1);
  }
  else
  {
	  GPIO_DRV_WritePinOutput(LED5, 0);
  }

  if (led7 == true)
  {
	  led7 = false;
	  GPIO_DRV_WritePinOutput(LED7, 1);
  }
  else
  {
	  GPIO_DRV_WritePinOutput(LED7, 0);
  }
  NVIC_EnableIRQ(PIT2_IRQn);
}
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
void PIT3_IRQHandler(void)
{
  /* Clear interrupt flag.*/
  PIT_HAL_ClearIntFlag(g_pitBase[pitTimer4_IDX], pitTimer4_CHANNEL);
  /* Write your code here ... */
}
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

/*!
** @}z
*/
/*
** ###################################################################
**
**     This file was created by Processor Expert 10.5 [05.21]
**     for the Freescale Kinetis series of microcontrollers.
**
** ###################################################################
*/
