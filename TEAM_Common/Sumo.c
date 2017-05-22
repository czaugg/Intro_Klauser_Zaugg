/*
 * Sumo.c
 *
 *  Created on: 16.05.2017
 *      Author: Erich Styger
 */
#include "Platform.h"

#if PL_CONFIG_HAS_SUMO
#include "Sumo.h"
#include "FRTOS1.h"
#include "Drive.h"
#include "Reflectance.h"
#include "Turn.h"
#include "CLS1.h"
#include "LED.h"

#define DIR_LEFT 1
#define DIR_RIGHT 2
#define SPEED 2500
#define ESCAPE_TURN_ANGLE 130

typedef enum {
  SUMO_STATE_IDLE,
  SUMO_STATE_DRIVING,
  SUMO_STATE_TURNING,
} SUMO_State_t;

typedef enum {
	SUMO_STRATEGY_BRICK,
	SUMO_STRATEGY_ATTACK
} SUMO_Strategy_t;

typedef enum {
	SUMO_TURN_NOT,
	SUMO_TURN_LEFT,
	SUMO_TURN_RIGHT
} SUMO_Turn_t;

static SUMO_State_t sumoState = SUMO_STATE_IDLE;
static SUMO_State_t sumoStrategy = SUMO_STRATEGY_BRICK;
static TaskHandle_t sumoTaskHndl;

/* direct task notification bits */
#define SUMO_START_SUMO (1<<0)  /* start sumo mode */
#define SUMO_STOP_SUMO  (1<<1)  /* stop stop sumo */
#define SUMO_ALARM_LINE (1<<2)  /* white line detected */
#define SUMO_LINE_LEFT  (1<<3)
#define SUMO_LINE_RIGHT (1<<4)
#define SUMO_RUN		(1<<5)
#define SUMO_ALL_FLAGS  (0xFFFF)

static bool SUMO_IsRunningSumo(void) {
  return sumoState!=SUMO_STATE_IDLE;
}

void SUMO_StartSumo(void) {
  (void)xTaskNotify(sumoTaskHndl, SUMO_START_SUMO, eSetBits);
}

void SUMO_StopSumo(void) {
  (void)xTaskNotify(sumoTaskHndl, SUMO_STOP_SUMO, eSetBits);
}

void SUMO_StartStopSumo(void) {
  if (SUMO_IsRunningSumo()) {
    (void)xTaskNotify(sumoTaskHndl, SUMO_STOP_SUMO, eSetBits);
  } else {
    (void)xTaskNotify(sumoTaskHndl, SUMO_START_SUMO, eSetBits);
  }
}

static SUMO_Turn_t SumoCheckLine(uint16_t lineThreshold){
	SUMO_Turn_t line = SUMO_TURN_NOT;
	uint16_t refSens[REF_NOF_SENSORS];
	REF_GetSensorValues(refSens, REF_NOF_SENSORS);

	LED_Off(1);
	LED_Off(2);

	if (refSens[0] < lineThreshold){
		(void)xTaskNotify(sumoTaskHndl, SUMO_ALARM_LINE & SUMO_LINE_LEFT, eSetBits);
		LED_On(1);
		line = SUMO_TURN_LEFT;
	}
	if ((refSens[REF_NOF_SENSORS-1]) < lineThreshold){
		(void)xTaskNotify(sumoTaskHndl, SUMO_ALARM_LINE & SUMO_LINE_RIGHT, eSetBits);
		LED_On(2);
		line = SUMO_TURN_RIGHT;
	}
	return line;
}

static bool SumoEscapeLine(SUMO_Turn_t turn){
	uint32_t notify;

	bool line = FALSE;
	DRV_SetSpeed(-SPEED, -SPEED);
	vTaskDelay(200/portTICK_PERIOD_MS);
	DRV_SetMode(DRV_MODE_STOP);

	if (turn == SUMO_TURN_LEFT){
		TURN_TurnAngle(ESCAPE_TURN_ANGLE, NULL);
		line = TRUE;
	} else if (turn == SUMO_TURN_RIGHT){
		TURN_TurnAngle(-ESCAPE_TURN_ANGLE, NULL);
		line = TRUE;
	}

	SUMO_StartSumo();
	(void)xTaskNotifyWait(0UL, SUMO_ALARM_LINE|SUMO_LINE_LEFT|SUMO_LINE_RIGHT, &notify, 0); /* check flags */
	return line;
}

static void SumoFSM_Brick(void) {
  uint32_t notify;
  SUMO_Turn_t turn;

  if (sumoState != SUMO_STATE_TURNING){
	  turn = SumoCheckLine(500);
  }
  if ((turn != SUMO_TURN_NOT) && (sumoState != SUMO_STATE_IDLE)){
	  SumoEscapeLine(turn);
	  sumoState = SUMO_STATE_IDLE;
  }

  (void)xTaskNotifyWait(0UL, SUMO_START_SUMO|SUMO_STOP_SUMO|SUMO_ALARM_LINE, &notify, 0); /* check flags */

  if (notify & SUMO_STOP_SUMO) {
     DRV_SetMode(DRV_MODE_STOP);
     sumoState = SUMO_STATE_IDLE;
  }


  for(;;) { /* breaks */
    switch(sumoState) {
      case SUMO_STATE_IDLE:
        if ((notify & SUMO_START_SUMO)) {
          DRV_SetSpeed(SPEED, SPEED);
          DRV_SetMode(DRV_MODE_SPEED);
          sumoState = SUMO_STATE_DRIVING;
          break; /* handle next state */
        }
        return;
      case SUMO_STATE_DRIVING:
    	return;
        break; /* handle next state */

      default: /* should not happen? */
        return;
    } /* switch */
  } /* for */
}

static void SumoTask(void* param) {
  sumoState = SUMO_STATE_IDLE;
  for(;;) {

	  SumoFSM_Brick();

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

static uint8_t SUMO_PrintHelp(const CLS1_StdIOType *io) {
  CLS1_SendHelpStr("sumo", "Group of sumo commands\r\n", io->stdOut);
  CLS1_SendHelpStr("  help|status", "Print help or status information\r\n", io->stdOut);
  CLS1_SendHelpStr("  start|stop", "Start and stop Sumo mode\r\n", io->stdOut);
  return ERR_OK;
}

/*!
 * \brief Prints the status text to the console
 * \param io StdIO handler
 * \return ERR_OK or failure code
 */
static uint8_t SUMO_PrintStatus(const CLS1_StdIOType *io) {
  CLS1_SendStatusStr("sumo", "\r\n", io->stdOut);
  if (sumoState==SUMO_STATE_IDLE) {
    CLS1_SendStatusStr("  running", "no\r\n", io->stdOut);
  } else {
    CLS1_SendStatusStr("  running", "yes\r\n", io->stdOut);
  }
  return ERR_OK;
}

uint8_t SUMO_ParseCommand(const unsigned char *cmd, bool *handled, const CLS1_StdIOType *io) {
  if (UTIL1_strcmp((char*)cmd, CLS1_CMD_HELP)==0 || UTIL1_strcmp((char*)cmd, "sumo help")==0) {
    *handled = TRUE;
    return SUMO_PrintHelp(io);
  } else if (UTIL1_strcmp((char*)cmd, CLS1_CMD_STATUS)==0 || UTIL1_strcmp((char*)cmd, "sumo status")==0) {
    *handled = TRUE;
    return SUMO_PrintStatus(io);
  } else if (UTIL1_strcmp(cmd, "sumo start")==0) {
    *handled = TRUE;
    SUMO_StartSumo();
  } else if (UTIL1_strcmp(cmd, "sumo stop")==0) {
    *handled = TRUE;
    SUMO_StopSumo();
  }
  return ERR_OK;
}

void SUMO_Init(void) {
  if (xTaskCreate(SumoTask, "Sumo", 500/sizeof(StackType_t), NULL, tskIDLE_PRIORITY+2, &sumoTaskHndl) != pdPASS) {
    for(;;){} /* error case only, stay here! */
  }
}

void SUMO_Deinit(void) {
}

#endif /* PL_CONFIG_HAS_SUMO */
