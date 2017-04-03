/**
 * \file
 * \brief Timer driver
 * \author Erich Styger, erich.styger@hslu.ch
 *
 * This module implements the driver for all our timers.
  */

#include "Platform.h"
#if PL_CONFIG_HAS_TIMER
#include "Timer.h"
#if PL_CONFIG_HAS_EVENTS
  #include "Event.h"
#endif
#if PL_CONFIG_HAS_TRIGGER
  #include "Trigger.h"
#endif
#if PL_CONFIG_HAS_MOTOR_TACHO
  #include "Tacho.h"
#endif
#include "TMOUT1.h"

void TMR_OnInterrupt(void) {
#define BLINK_PERIOD_MS 1000


#if PLS_CONFIG_HAS_EVENTS
	if((cntr%(BLINKE_PERIOD_MS/TMR_TICK_MS))==0){
		EVNT_SetEvent(EVNT_LED_HEARTBEAT);
	}
#endif

  /* this one gets called from an interrupt!!!! */
  /*! \todo Add code for a blinking LED here */
	static unsigned int cntr = 0;
	cntr++;
	if (cntr > 100){
		EVNT_SetEvent(EVNT_TMR_OVERFLOW);
		cntr = 0;
	}
}

void TMR_Init(void) {
}

void TMR_Deinit(void) {
}

#endif /* PL_CONFIG_HAS_TIMER*/
