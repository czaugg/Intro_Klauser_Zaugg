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
#include "Buzzer.h"
#include "Distance.h"

#define DIR_LEFT 1
#define DIR_RIGHT 2
#define SPEED 4000
#define MAXSPEED 10000
#define TURN 0.7f
#define TASKMS 10
#define ESCAPE_TURN_ANGLE 130
bool handleLine = TRUE;


typedef enum {
  SUMO_STATE_IDLE,
  SUMO_STATE_DRIVING,
  SUMO_STATE_TURNING,
  SUMO_STATE_SEARCHING,
  SUMO_STATE_ATTACK,
  SUMO_STATE_COUNTDOWN,
  SUMO_STATE_WIGGLE
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
SUMO_Turn_t StartTurn = SUMO_TURN_LEFT;

typedef enum {
	OPP_LOST,
	OPP_LEFT,
	OPP_RIGHT,
	OPP_CENTER
} OPP_POS_t;

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


static void SumoCountdown(void){
	uint8_t i;
	for (i = 0; i < 5; i++){
		BUZ_Beep(500,200);
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

static bool SUMO_IsRunningSumo(void) {
  return sumoState!=SUMO_STATE_IDLE;
}

void SUMO_StartSumo(void) {
	SumoCountdown();
    (void)xTaskNotify(sumoTaskHndl, SUMO_START_SUMO, eSetBits);
}

void SUMO_StopSumo(void) {
  (void)xTaskNotify(sumoTaskHndl, SUMO_STOP_SUMO, eSetBits);
}

void SUMO_StartStopSumo(void) {
  if (SUMO_IsRunningSumo()) {
      (void)xTaskNotify(sumoTaskHndl, SUMO_STOP_SUMO, eSetBits);
  } else {
	 SumoCountdown();
    (void)xTaskNotify(sumoTaskHndl, SUMO_START_SUMO, eSetBits);
  }
}

static SUMO_Turn_t SumoCheckLine(uint16_t lineThreshold){
	SUMO_Turn_t line = SUMO_TURN_NOT;
	uint16_t refSens[REF_NOF_SENSORS];
	REF_GetSensorValues(refSens, REF_NOF_SENSORS);

	//LED_Off(1);
	//LED_Off(2);

	if (refSens[0] < lineThreshold){
		//(void)xTaskNotify(sumoTaskHndl, SUMO_ALARM_LINE & SUMO_LINE_LEFT, eSetBits);
		//LED_On(1);
		line = SUMO_TURN_LEFT;
	}
	if ((refSens[REF_NOF_SENSORS-1]) < lineThreshold){
		//(void)xTaskNotify(sumoTaskHndl, SUMO_ALARM_LINE & SUMO_LINE_RIGHT, eSetBits);
		//LED_On(2);
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
	return line;
}

static void SumoFSM_Brick(void) {
  uint32_t notify;
  SUMO_Turn_t turn;

  /* Handle Line */
  if (sumoState != SUMO_STATE_TURNING){
	  turn = SumoCheckLine(500);
  }
  if ((turn != SUMO_TURN_NOT) && (sumoState != SUMO_STATE_IDLE)){
	  SumoEscapeLine(turn);
	  sumoState = SUMO_STATE_IDLE;
  }

  /* Check Stop Flag */
  if (notify & SUMO_STOP_SUMO) {
     DRV_SetMode(DRV_MODE_STOP);
     sumoState = SUMO_STATE_IDLE;
  }

  /* Get Flags */
  (void)xTaskNotifyWait(0UL, SUMO_START_SUMO|SUMO_STOP_SUMO|SUMO_ALARM_LINE, &notify, 0); /* check flags */

  /* State Machine */
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


static OPP_POS_t SumoSearch(void) {
	uint16_t left, right;
	OPP_POS_t opp;

	left = DIST_GetDistance(DIST_SENSOR_LEFT);
	right = DIST_GetDistance(DIST_SENSOR_RIGHT);

	if (left != 0xFFFF){
		if (right != 0xFFFF){
			opp = OPP_CENTER;
			LED_On(1);
			LED_On(2);
		} else {
			opp = OPP_LEFT;
			LED_On(1);
			LED_Off(2);
		}
	} else {
		if (right != 0xFFFF){
			opp = OPP_RIGHT;
			LED_On(2);
			LED_Off(1);
		} else {
			opp = OPP_LOST;
			LED_Off(1);
			LED_Off(2);
		}
	}
	return opp;
}

static SUMO_Turn_t SumoFindOpp(SUMO_Turn_t turn){
	static uint16_t count = 0;
	SUMO_Turn_t newTurn = turn;
	DRV_SetMode(DRV_MODE_SPEED);

	if (turn == SUMO_TURN_LEFT){
		DRV_SetSpeed(-SPEED, SPEED);
		count++;
		if (count >= (1000 / TASKMS)) {
			count = 0;
			newTurn = SUMO_TURN_NOT;
		}
	} else if (turn == SUMO_TURN_RIGHT) {
		DRV_SetSpeed(SPEED, -SPEED);
		count++;
		if (count >= (1000 / TASKMS)) {
			count = 0;
			newTurn = SUMO_TURN_NOT;
		}
	} else {
		DRV_SetSpeed(2*SPEED, 2*SPEED);
		count++;
		if (count >= (1000 / TASKMS)) {
			count = 0;
			newTurn = SUMO_TURN_LEFT;
		}
	}
	return newTurn;
}

static void SumoAttack(OPP_POS_t opp){
	switch(opp){
		case OPP_CENTER:
			DRV_SetSpeed(MAXSPEED, MAXSPEED);
			break;
		case OPP_LEFT:
			DRV_SetSpeed(MAXSPEED * TURN, MAXSPEED / TURN);
			break;
		case OPP_RIGHT:
			DRV_SetSpeed(MAXSPEED / TURN, MAXSPEED * TURN);
			break;
	}
}

static void SumoFSM_Attack(void){
	uint32_t notify;
	OPP_POS_t opp;
	SUMO_Turn_t turn;


	  /* Handle Line */
	if (handleLine) {
		if (sumoState != SUMO_STATE_TURNING) {
			turn = SumoCheckLine(500);
		}
		if ((turn != SUMO_TURN_NOT) && (sumoState != SUMO_STATE_IDLE)) {
			SumoEscapeLine(turn);
			sumoState = SUMO_STATE_SEARCHING;
			opp = OPP_LOST;
			BUZ_Beep(1000,1000);
		}
	}

	/* Get Flags */
	(void) xTaskNotifyWait(0UL, SUMO_START_SUMO | SUMO_STOP_SUMO | SUMO_ALARM_LINE, &notify, 0); /* check flags */


	  /* Check Stop Flag */
	  if (notify & SUMO_STOP_SUMO) {
	     DRV_SetMode(DRV_MODE_STOP);
	     sumoState = SUMO_STATE_IDLE;
	  }

	/* State Machine */
	for (;;) { /* breaks */
		switch (sumoState) {
		case SUMO_STATE_IDLE:
			if ((notify & SUMO_START_SUMO)) {
				sumoState = SUMO_STATE_SEARCHING;
				break; /* handle next state */
			}
			return;
/*		case SUMO_STATE_COUNTDOWN:
			SumoCountdown();
			sumoState = SUMO_STATE_SEARCHING;
			break;*/
		case SUMO_STATE_SEARCHING:
			opp = SumoSearch();
			if (opp == OPP_LOST){
				StartTurn = SumoFindOpp(StartTurn);
				return;
			} else {
				sumoState = SUMO_STATE_ATTACK;
				BUZ_Beep(500,200);
				DRV_SetMode(DRV_MODE_SPEED);
				break;
			}
		case SUMO_STATE_ATTACK:
			opp = SumoSearch();
			if (opp != OPP_LOST) {
				SumoAttack(opp);
			} else {
				sumoState = SUMO_STATE_SEARCHING;
				BUZ_Beep(1000, 200);
				break;
			}
			return;
		default: /* should not happen? */
			sumoState = SUMO_STATE_IDLE;
			return;
		} /* switch */
	} /* for */
}

static void SumoTask(void* param) {
  sumoState = SUMO_STATE_IDLE;

  for(;;) {

	  SumoFSM_Attack();
	  //SumoFSM_Brick();

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
  } else if (UTIL1_strcmp(cmd, "sumo startt")==0) {
    *handled = TRUE;
    SUMO_StartSumo();
  } else if (UTIL1_strcmp(cmd, "sumo stopp")==0) {
    *handled = TRUE;
    SUMO_StopSumo();
  } else if (UTIL1_strcmp(cmd, "sumo line")==0) {
	*handled = TRUE;
	handleLine = TRUE;
	} else if (UTIL1_strcmp(cmd, "sumo noline") == 0) {
		*handled = TRUE;
		handleLine = FALSE;
	} else if (UTIL1_strcmp(cmd, "sumo left") == 0) {
		*handled = TRUE;
		StartTurn = SUMO_TURN_LEFT;
	} else if (UTIL1_strcmp(cmd, "sumo right") == 0) {
		*handled = TRUE;
		StartTurn = SUMO_TURN_RIGHT;
	} else if (UTIL1_strcmp(cmd, "sumo center") == 0) {
		*handled = TRUE;
		StartTurn = SUMO_TURN_NOT;
	}

  return ERR_OK;
}

void SUMO_Init(void) {
  if (xTaskCreate(SumoTask, "Sumo", 500/sizeof(StackType_t), NULL, tskIDLE_PRIORITY+4, &sumoTaskHndl) != pdPASS) {
    for(;;){} /* error case only, stay here! */
  }
}

void SUMO_Deinit(void) {
}

#endif /* PL_CONFIG_HAS_SUMO */
