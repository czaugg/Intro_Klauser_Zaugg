/**
 * \file
 * \brief Module to handle the LCD display
 * \author Erich Styger, erich.styger@hslu.ch
 *
 * This module is driving the Nokia LCD Display.
 */

#include "Platform.h"
#if PL_CONFIG_HAS_LCD
#include "LCD.h"
#include "PDC1.h"
#include "GDisp1.h"
#include "GFONT1.h"
#include "FDisp1.h"
#include "Application.h"
#include "UTIL1.h"
#include "LCD_LED.h"
#include "Event.h"
#include "FRTOS1.h"
//#include "RApp.h"
#include "LCDMenu.h"

#if PL_CONFIG_HAS_SNAKE_GAME
#include "Snake.h"
#endif
#if PL_CONFIG_IS_SUMO_REMOTE
#include "RstdIO.h"
#endif

/*! \todo Add additional includes as needed */

/* status variables */
static bool LedBackLightisOn = TRUE;
static bool remoteModeIsOn = FALSE;
static bool requestLCDUpdate = FALSE;

#if PL_CONFIG_HAS_LCD_MENU
typedef enum {
	LCD_MENU_ID_NONE = LCDMENU_ID_NONE, /* special value! */
	LCD_MENU_ID_MAIN,
    LCD_MENU_ID_BACKLIGHT,
    LCD_MENU_ID_NUM_VALUE,
	LCD_MENU_ID_GAMES,
	LCD_MENU_ID_SNAKE,
	LCD_MENU_ID_SUMO,
	LCD_MENU_ID_SUMO_START,
	LCD_MENU_ID_SUMO_SET_DIR,
	LCD_MENU_ID_SUMO_SET_LINE,
	LCD_MENU_ID_SUMO_STOP
} LCD_MenuIDs;

static LCDMenu_StatusFlags ValueChangeHandler(const struct LCDMenu_MenuItem_ *item, LCDMenu_EventType event, void **dataP) {
  static int value = 0;
  static uint8_t valueBuf[16];
  LCDMenu_StatusFlags flags = LCDMENU_STATUS_FLAGS_NONE;

  (void)item;
  if (event==LCDMENU_EVENT_GET_TEXT) {
    UTIL1_strcpy(valueBuf, sizeof(valueBuf), (uint8_t*)"Val: ");
    UTIL1_strcatNum32s(valueBuf, sizeof(valueBuf), value);
    *dataP = valueBuf;
    flags |= LCDMENU_STATUS_FLAGS_HANDLED|LCDMENU_STATUS_FLAGS_UPDATE_VIEW;
  } else if (event==LCDMENU_EVENT_GET_EDIT_TEXT) {
    UTIL1_strcpy(valueBuf, sizeof(valueBuf), (uint8_t*)"[-] ");
    UTIL1_strcatNum32s(valueBuf, sizeof(valueBuf), value);
    UTIL1_strcat(valueBuf, sizeof(valueBuf), (uint8_t*)" [+]");
    *dataP = valueBuf;
    flags |= LCDMENU_STATUS_FLAGS_HANDLED|LCDMENU_STATUS_FLAGS_UPDATE_VIEW;
  } else if (event==LCDMENU_EVENT_DECREMENT) {
    value--;
    flags |= LCDMENU_STATUS_FLAGS_HANDLED|LCDMENU_STATUS_FLAGS_UPDATE_VIEW;
  } else if (event==LCDMENU_EVENT_INCREMENT) {
    value++;
    flags |= LCDMENU_STATUS_FLAGS_HANDLED|LCDMENU_STATUS_FLAGS_UPDATE_VIEW;
  }
  return flags;
}

static LCDMenu_StatusFlags BackLightMenuHandler(const struct LCDMenu_MenuItem_ *item, LCDMenu_EventType event, void **dataP) {
  LCDMenu_StatusFlags flags = LCDMENU_STATUS_FLAGS_NONE;

  (void)item;
  if (event==LCDMENU_EVENT_GET_TEXT && dataP!=NULL) {
    if (LedBackLightisOn) {
      *dataP = "Backlight is ON";
    } else {
      *dataP = "Backlight is OFF";
    }
    flags |= LCDMENU_STATUS_FLAGS_HANDLED|LCDMENU_STATUS_FLAGS_UPDATE_VIEW;
  } else if (event==LCDMENU_EVENT_ENTER) { /* toggle setting */
    LedBackLightisOn = !LedBackLightisOn;
    flags |= LCDMENU_STATUS_FLAGS_HANDLED|LCDMENU_STATUS_FLAGS_UPDATE_VIEW;
  }
  return flags;
}


static LCDMenu_StatusFlags SumoHandler(const struct LCDMenu_MenuItem_ *item, LCDMenu_EventType event, void **dataP) {
	static int8_t dir = -1; /* Direction -1 = left, 0 = center, 1 = right */
	static bool line = TRUE;
	LCDMenu_StatusFlags flags = LCDMENU_STATUS_FLAGS_NONE;
	char* cmd1 = NULL;
	char* cmd2 = NULL;
	char* cmd3 = NULL;
	unsigned char buf[32];

	if ((event == LCDMENU_EVENT_RIGHT) || (event == LCDMENU_EVENT_ENTER)) {
		switch(item->id){
			case LCD_MENU_ID_SUMO_START:
				cmd1 = "sumo start";
				switch (dir){
					case -1:
						cmd2 = " left";
						break;
					case 0:
						cmd2 = " center";
						break;
					case 1:
						cmd2 = " right";
						break;
				}
				if (line){
					cmd3 = " line";
				} else {
					cmd3 = " noline";
				}
				break;
			case LCD_MENU_ID_SUMO_SET_DIR:
				switch (dir){
					case -1:
						dir = 0;
						break;
					case 0:
						dir = 1;
						break;
					case 1:
						dir = -1;
						break;
					default:
						dir = -1;
						break;
				}
				break;
			case LCD_MENU_ID_SUMO_SET_LINE:
				line = !line;
				break;
			case LCD_MENU_ID_SUMO_STOP:
				cmd1 = "sumo stop";
				break;
		}
		if (cmd1 != NULL) {
			UTIL1_strcpy(buf, sizeof(buf), cmd1);
			/*if (cmd2 != NULL) {
				UTIL1_strcat(buf, sizeof(buf), cmd2);
			}
			if (cmd3 != NULL) {
				UTIL1_strcat(buf, sizeof(buf), cmd3);
			}*/
			UTIL1_strcat(buf, sizeof(buf), "\n");
	    	RSTDIO_SendToTxStdio(RSTDIO_QUEUE_TX_IN, buf, UTIL1_strlen((char*)buf));
		}
	    flags |= LCDMENU_STATUS_FLAGS_HANDLED|LCDMENU_STATUS_FLAGS_UPDATE_VIEW;

	} else if (event==LCDMENU_EVENT_GET_TEXT && dataP!=NULL) {
		switch(item->id){
			case LCD_MENU_ID_SUMO_SET_DIR:
				if (dir == -1){
					*dataP = "Start Left";
				} else if (dir == 0) {
					*dataP = "Start Center";
				} else if (dir == 1) {
					*dataP = "Start Right";
				}
				break;
			case LCD_MENU_ID_SUMO_SET_LINE:
				if (line){
					*dataP = "Detect Line";
				} else {
					*dataP = "No Line";
				}
				break;
		}
		flags |= LCDMENU_STATUS_FLAGS_HANDLED|LCDMENU_STATUS_FLAGS_UPDATE_VIEW;
	}

	return flags;
}

static LCDMenu_StatusFlags SnakeGameHandler(const struct LCDMenu_MenuItem_*, LCDMenu_EventType, void **);

static const LCDMenu_MenuItem menus[] =
{/* id,                                     grp, 	pos,   up,                     	down,                          	text,           callback                   	flags                  */
      {LCD_MENU_ID_MAIN,                  	0,   	0,   	LCD_MENU_ID_NONE,      	LCD_MENU_ID_BACKLIGHT,         	"General",      NULL,                     	LCDMENU_MENU_FLAGS_NONE},
      {LCD_MENU_ID_BACKLIGHT,              	1,   	0,   	LCD_MENU_ID_MAIN,      	LCD_MENU_ID_NONE,              	NULL,           BackLightMenuHandler,      	LCDMENU_MENU_FLAGS_NONE},
      {LCD_MENU_ID_NUM_VALUE,              	1,   	1,   	LCD_MENU_ID_MAIN,      	LCD_MENU_ID_NONE,              	NULL,           ValueChangeHandler,        	LCDMENU_MENU_FLAGS_EDITABLE},
#if PL_CONFIG_HAS_SNAKE_GAME
	  {LCD_MENU_ID_GAMES,					0,		1,		LCD_MENU_ID_NONE, 		LCD_MENU_ID_SNAKE,				"Games",		NULL,						LCDMENU_MENU_FLAGS_NONE},
	  {LCD_MENU_ID_SNAKE,					2,   	0,		LCD_MENU_ID_MAIN,		LCD_MENU_ID_NONE,				"Snake",		SnakeGameHandler, 		  	LCDMENU_MENU_FLAGS_NONE},
#endif
#if PL_CONFIG_IS_SUMO_REMOTE
	  {LCD_MENU_ID_SUMO,					0,		2,		LCD_MENU_ID_NONE, 		LCD_MENU_ID_SUMO_STOP,			"Sumo",			NULL,						LCDMENU_MENU_FLAGS_NONE},
	  {LCD_MENU_ID_SUMO_STOP,				3,		0,		LCD_MENU_ID_SUMO,		LCD_MENU_ID_NONE,				"Stop Sumo",	SumoHandler,				LCDMENU_MENU_FLAGS_NONE},
	  {LCD_MENU_ID_SUMO_START,				3,		1,		LCD_MENU_ID_SUMO,		LCD_MENU_ID_NONE,				"Start Sumo",	SumoHandler,				LCDMENU_MENU_FLAGS_NONE},
	  {LCD_MENU_ID_SUMO_SET_DIR,			3,		2,		LCD_MENU_ID_SUMO,		LCD_MENU_ID_NONE,				NULL,			SumoHandler,				LCDMENU_MENU_FLAGS_NONE},
	  {LCD_MENU_ID_SUMO_SET_LINE,			3,		3,		LCD_MENU_ID_SUMO,		LCD_MENU_ID_NONE,				NULL,			SumoHandler,				LCDMENU_MENU_FLAGS_NONE},
#endif
};


static LCDMenu_StatusFlags SnakeGameHandler(const struct LCDMenu_MenuItem_ *item, LCDMenu_EventType event, void **dataP) {
	  LCDMenu_StatusFlags flags = LCDMENU_STATUS_FLAGS_NONE;
	  xTaskHandle snakeHandle = NULL;

	  switch(event){
		  case LCDMENU_EVENT_ENTER:
		  case LCDMENU_EVENT_RIGHT:
			  SNAKE_Init();
			  snakeHandle = SNAKE_RunTask();
			  do {
				  vTaskDelay(50 / portTICK_RATE_MS);
			  } while (eTaskGetState(snakeHandle) != eSuspended);
			  vTaskDelete(snakeHandle);
			  LCDMenu_OnEvent(LCDMENU_EVENT_DRAW, NULL);
			  break;
	  }
	  return flags;
}


uint8_t LCD_HandleRemoteRxMessage(RAPP_MSG_Type type, uint8_t size, uint8_t *data, RNWK_ShortAddrType srcAddr, bool *handled, RPHY_PacketDesc *packet) {
  (void)size;
  (void)packet;
  switch(type) {
     default:
      break;
  } // switch
  return ERR_OK;
}

#endif /* PL_CONFIG_HAS_LCD_MENU */


static void ShowTextOnLCD(unsigned char *text) {
  FDisp1_PixelDim x, y;

  GDisp1_Clear();
  x = 0;
  y = 10;
  FDisp1_WriteString(text, GDisp1_COLOR_BLACK, &x, &y, GFONT1_GetFont());
  GDisp1_UpdateFull();
}

void DrawLines(){
	GDisp1_DrawLine(0, 0, PDC1_HW_WIDTH, PDC1_HW_HEIGHT, GDisp1_COLOR_BLACK);
}

void DrawCircles(){
	GDisp1_DrawCircle(PDC1_HW_WIDTH / 2, PDC1_HW_HEIGHT/2, 10, GDisp1_COLOR_BLACK);
}

static void LCD_Task(void *param) {
  (void)param; /* not used */
#if 0
  ShowTextOnLCD("Press a key!");
  //DrawText();
  /* \todo extend */
  //DrawFont();
  DrawLines(); /*! \todo */
  DrawCircles();
  GDisp1_UpdateFull();
#endif
#if PL_CONFIG_HAS_LCD_MENU
  LCDMenu_InitMenu(menus, sizeof(menus)/sizeof(menus[0]), 1);
  LCDMenu_OnEvent(LCDMENU_EVENT_DRAW, NULL);
#endif
  for(;;) {
    if (LedBackLightisOn) {
      LCD_LED_On(); /* LCD backlight on */
    } else {
      LCD_LED_Off(); /* LCD backlight off */
    }
#if PL_CONFIG_HAS_LCD_MENU
    if (requestLCDUpdate) {
      requestLCDUpdate = FALSE;
      LCDMenu_OnEvent(LCDMENU_EVENT_DRAW, NULL);
    }
#if 1 /*! \todo Change this to for your own needs, or use direct task notification */
    if (EVNT_EventIsSetAutoClear(EVNT_SW2_PRESSED)) { /* left */
      LCDMenu_OnEvent(LCDMENU_EVENT_LEFT, NULL);
//      ShowTextOnLCD("left");
    }
    if (EVNT_EventIsSetAutoClear(EVNT_SW1_PRESSED)) { /* right */
      LCDMenu_OnEvent(LCDMENU_EVENT_RIGHT, NULL);
//      ShowTextOnLCD("right");
    }
    if (EVNT_EventIsSetAutoClear(EVNT_SW5_PRESSED)) { /* up */
      LCDMenu_OnEvent(LCDMENU_EVENT_UP, NULL);
//      ShowTextOnLCD("up");
    }
    if (EVNT_EventIsSetAutoClear(EVNT_SW3_PRESSED)) { /* down */
      LCDMenu_OnEvent(LCDMENU_EVENT_DOWN, NULL);
//      ShowTextOnLCD("down");
    }
    if (EVNT_EventIsSetAutoClear(EVNT_SW4_PRESSED)) { /* center */
      LCDMenu_OnEvent(LCDMENU_EVENT_ENTER, NULL);
//      ShowTextOnLCD("center");
    }
    if (EVNT_EventIsSetAutoClear(EVNT_SW6_PRESSED)) { /* side up */
      LCDMenu_OnEvent(LCDMENU_EVENT_UP, NULL);
//      ShowTextOnLCD("side up");
    }
    if (EVNT_EventIsSetAutoClear(EVNT_SW7_PRESSED)) { /* side down */
      LCDMenu_OnEvent(LCDMENU_EVENT_DOWN, NULL);
//      ShowTextOnLCD("side down");
    }
#endif
#endif /* PL_CONFIG_HAS_LCD_MENU */
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}



void LCD_Deinit(void) {
#if PL_CONFIG_HAS_LCD_MENU
  LCDMenu_Deinit();
#endif
}

void LCD_Init(void) {
  LedBackLightisOn =  TRUE;
  if (xTaskCreate(LCD_Task, "LCD", configMINIMAL_STACK_SIZE+100, NULL, tskIDLE_PRIORITY, NULL) != pdPASS) {
    for(;;){} /* error! probably out of memory */
  }
#if PL_CONFIG_HAS_LCD_MENU
  LCDMenu_Init();
#endif
}
#endif /* PL_CONFIG_HAS_LCD */
