/*
 * snake.c
 *
 *  Created on: 26.04.2017
 *      Author: Chrigu
 */

#include "Platform.h"
#include "PDC1.h"
#include "WAIT1.h"
#include "GDisp1.h"
#include "FDisp1.h"
#include "GFONT1.h"
#include "UTIL1.h"
#include "FRTOS1.h"
#include "Event.h"

#define SNAKE_MAX_LEN 100
#define SNAKE_INIT_LEN 10

#define DISPLAY_HEIGTH GDisp1_GetHeight()
#define DISPLAY_WIDTH GDisp1_GetWidth()

#define SW_UP EVNT_EventIsSetAutoClear(EVNT_SW5_PRESSED)
#define	SW_DOWN EVNT_EventIsSetAutoClear(EVNT_SW3_PRESSED)
#define	SW_LEFT EVNT_EventIsSetAutoClear(EVNT_SW2_PRESSED)
#define	SW_RIGTH EVNT_EventIsSetAutoClear(EVNT_SW1_PRESSED)
#define	SW_MIDDLE EVNT_EventIsSetAutoClear(EVNT_SW4_PRESSED)

typedef struct{
	bool transparentWalls;
	bool transparentSnake;
	uint8_t foodSize;
	uint8_t grow;
} SnakeSettings_t;

typedef union {
	struct {
		int8_t x;
		int8_t y;
	} pos;
	uint16_t val;
} Position_t;

typedef void (*PutFunc)(void*, Position_t);
typedef Position_t (*GetFunc)(void*);

typedef struct{
	Position_t body[SNAKE_MAX_LEN];
	Position_t head;
	Position_t dir;
	Position_t food[2];
	uint8_t grow;
	uint8_t points;
	uint8_t indexHead;
	uint8_t indexTail;
	uint8_t maxLength;
	uint8_t length;
	uint16_t time;
	SnakeSettings_t rules;
	PutFunc Put;
	GetFunc Get;
} SnakeBuf_t;

void AddToBuf(void* self, Position_t new){
	SnakeBuf_t* buf = (SnakeBuf_t*)self;
	if (buf->length < (buf->maxLength - 1)){
		buf->length++;
		buf->indexHead++;
		if (buf->indexHead >= buf->maxLength) buf->indexHead = 0;
		buf->body[buf->indexHead] = new;
	}
}

Position_t GetFromBuf(void* self){
	SnakeBuf_t* buf = (SnakeBuf_t*)self;
	Position_t res;
	res.val = -1;
	if (buf->length > 0){
		buf->length--;
		buf->indexTail++;
		if (buf->indexTail >= buf->maxLength) buf->indexTail = 0;
		res = buf->body[buf->indexTail];
	}
	return res;
}

SnakeBuf_t snake;

void SnakeInit(void){
	snake.maxLength = SNAKE_MAX_LEN;
	snake.length = SNAKE_INIT_LEN - 1;
	snake.head.pos.x = DISPLAY_WIDTH / 2;
	snake.head.pos.y = DISPLAY_HEIGTH / 2;
	snake.indexHead = snake.length - 1;
	snake.indexTail = 0;

	snake.rules.transparentWalls = TRUE;
	snake.rules.transparentSnake = FALSE;

	snake.Get = GetFromBuf;
	snake.Put = AddToBuf;

	for (uint8_t i = 0; i < snake.length - 1; i++){
		snake.body[i].pos.x = snake.head.pos.x - SNAKE_INIT_LEN + i;
		snake.body[i].pos.y = snake.head.pos.y;
	}
}

void SnakeCheckInput(void){
	if(SW_UP && (snake.dir.pos.y == 1)){
		snake.dir.pos.y = -1;
		snake.dir.pos.x = 0;
	} else if (SW_DOWN && (snake.dir.pos.y == -1)){
		snake.dir.pos.y = 1;
		snake.dir.pos.x = 0;
	} else if (SW_LEFT && (snake.dir.pos.x == 1)){
		snake.dir.pos.x = -1;
		snake.dir.pos.y = 0;
	} else if (SW_RIGTH && (snake.dir.pos.x == -1)){
		snake.dir.pos.x = 1;
		snake.dir.pos.y = 0;
	}

}

void GameOver(void){

}

static uint8_t random(uint8_t min, uint8_t max) {
	TickType_t cnt;

	cnt = xTaskGetTickCount();
	cnt &= 0xff; /* reduce to 8 bits */
	if (max>64) {
		cnt >>= 1;
	} else {
		cnt >>= 2;
	}
	if (cnt<min) {
		cnt = min;
	}
	if (cnt>max) {
		cnt = max/2;
	}
	return cnt;
}

void SnakeMove(void){
	snake.Put(&snake, snake.head);
	if (snake.grow == 0){
		snake.Get(&snake);
	} else {
		snake.grow--;
	}
	snake.head.val += snake.dir.val;

	if (snake.rules.transparentWalls){
		if (snake.head.pos.x < 0) snake.head.pos.x = DISPLAY_WIDTH;
		if (snake.head.pos.x > DISPLAY_WIDTH) snake.head.pos.x = 0;
		if (snake.head.pos.y < 0) snake.head.pos.y = DISPLAY_HEIGTH;
		if (snake.head.pos.y > DISPLAY_HEIGTH) snake.head.pos.y = 0;
	} else {
		GameOver();
	}

	if(!snake.rules.transparentSnake){
		for (uint8_t i = 0; i < snake.length; i++){
			if (snake.body[i].val == snake.head.val){
				GameOver();
			}
		}
	}
}

void PlaceFood(void){
	snake.food[1].pos.x = random(0, DISPLAY_WIDTH);
	snake.food[1].pos.y = random(0, DISPLAY_HEIGTH);
	snake.food[2].pos.x = snake.food[1].pos.x + snake.rules.foodSize;
	snake.food[2].pos.y = snake.food[1].pos.y + snake.rules.foodSize;
}

void CheckFood(void){
	if (snake.head.pos.x >= snake.food[1].pos.x){
		if (snake.head.pos.x <= snake.food[2].pos.x){
			if (snake.head.pos.y >= snake.food[1].pos.y){
				if (snake.head.pos.y <= snake.food[2].pos.y){
					snake.grow = snake.rules.grow;
					snake.points++;
					PlaceFood();
				}
			}
		}
	}
}

void MySnakeTask(void){
	SnakeInit();

	while(1){
		SnakeCheckInput();
		SnakeMove();
		CheckFood();
		vTaskDelay(snake.time/portTICK_PERIOD_MS) ;

	}

}




