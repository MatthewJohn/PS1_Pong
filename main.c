/*
 * main.c
 *
 *  Created on: 3 Sep 2020
 *      Author: matt
 */

#include <stdlib.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>

#include <libetc.h>
#include <libpad.h>

#define OT_LENGTH 1
#define PACKETMAX 18
#define __ramsize   0x00200000
#define __stacksize 0x00004000

#define DEBUG 1

int SCREEN_WIDTH, SCREEN_HEIGHT;

//Set the screen mode to either SCREEN_MODE_PAL or SCREEN_MODE_NTSC
void setScreenMode(int mode) {

}

//Initialize screen,
void initializeScreen() {
	if (*(char *)0xbfc7ff52=='E') { // SCEE string address
    	// PAL MODE
    	SCREEN_WIDTH = 320;
    	SCREEN_HEIGHT = 256;
    	if (DEBUG) printf("Setting the PlayStation Video Mode to (PAL %dx%d)\n",SCREEN_WIDTH,SCREEN_HEIGHT,")");
    	SetVideoMode(1);
    	if (DEBUG) printf("Video Mode is (%d)\n",GetVideoMode());
   	} else {
     	// NTSC MODE
     	SCREEN_WIDTH = 320;
     	SCREEN_HEIGHT = 240;
     	if (DEBUG) printf("Setting the PlayStation Video Mode to (NTSC %dx%d)\n",SCREEN_WIDTH,SCREEN_HEIGHT,")");
     	SetVideoMode(0);
     	if (DEBUG) printf("Video Mode is (%d)\n",GetVideoMode());
   }
	GsInitGraph(SCREEN_WIDTH, SCREEN_HEIGHT, GsINTER|GsOFSGPU, 1, 0);
	GsDefDispBuff(0, 0, 0, SCREEN_HEIGHT);



	GsInitGraph(SCREEN_WIDTH, SCREEN_HEIGHT, GsINTER|GsOFSGPU, 1, 0); //Set up interlation..
	GsDefDispBuff(0, 0, 0, SCREEN_HEIGHT);	//..and double buffering.
}

void initializeDebugFont() {
	FntLoad(960, 256);
	SetDumpFnt(FntOpen(5, 20, 320, 240, 0, 512)); //Sets the dumped font for use with FntPrint();
}

void initializeOrderingTable(GsOT* orderingTable){
	GsClearOt(0,0,&orderingTable[GsGetActiveBuff()]);
}



GsOT orderingTable[2];
short currentBuffer;
char fullText[100] = "Current loop: ";
int loopCounter = 0;


// ----- Poly struct --------------------

typedef struct object_inf {
	POLY_F4 *poly;
	int x;
	int y;
} object_inf;


// ----- PADDLE INFO  --------------------
#define PADDLE_COUNT 2
#define PADDLE_HEIGHT 30
#define PADDLE_WIDTH 8
#define PADDLE_L_R 20
#define PADDLE_L_G 120
#define PADDLE_L_B 67
#define PADDLE_R_R 80
#define PADDLE_R_G 200
#define PADDLE_R_B 200
#define PADDLE_INITIAL_Y 120

POLY_F4 poly_paddles[PADDLE_COUNT];
paddle_positions[PADDLE_COUNT];
object_inf paddle_infos[2];


POLY_F4 poly_ball[PADDLE_COUNT];
object_inf ball;

// ----- gamepad INFO  --------------------
u_long padButtons;


void drawBar(object_inf *p) {
    setXY4(p->poly,
        p->x - (PADDLE_WIDTH / 2), p->y + (PADDLE_HEIGHT / 2),
        p->x + (PADDLE_WIDTH / 2), p->y + (PADDLE_HEIGHT / 2),
        p->x + (PADDLE_WIDTH / 2), p->y - (PADDLE_HEIGHT / 2),
        p->x - (PADDLE_WIDTH / 2), p->y - (PADDLE_HEIGHT / 2)
    );
    DrawPrim(p->poly);
}

void setupPaddle(object_inf *p, int r, int g, int b) {
    setPolyF4(p->poly);
    setSemiTrans(p->poly, 1);
    setRGB0(p->poly, r, g, b);
}

void checkPads() {
	padButtons = PadRead(1);

	if(padButtons & PADLup) paddle_infos[0].y --;
	if(padButtons & PADLdown) paddle_infos[0].y ++;
}

void initGame() {

	// Setup paddle structs
	paddle_infos[0].poly = &poly_paddles[0];
	paddle_infos[0].x = 20;
	paddle_infos[0].y = PADDLE_INITIAL_Y;
	setupPaddle(&paddle_infos[0], PADDLE_L_R, PADDLE_L_G, PADDLE_L_B);
	paddle_infos[1].poly = &poly_paddles[1];
	paddle_infos[1].x = 240;
	paddle_infos[1].y = PADDLE_INITIAL_Y;
	setupPaddle(&paddle_infos[1], PADDLE_R_R, PADDLE_R_G, PADDLE_R_B);

	// Initialise ball
	ball.poly = poly_ball;
	ball.x =

	// Initialise controllers
	PadInit(0);

}



void initialize() {
	initializeScreen();
	initializeDebugFont();
}

void display() {
	currentBuffer = GsGetActiveBuff();
	FntFlush(-1);
	GsClearOt(0, 0, &orderingTable[currentBuffer]);
	DrawSync(0);
	VSync(0);
	GsSwapDispBuff();
	GsSortClear(255, 255, 255, &orderingTable[currentBuffer]);
	GsDrawOt(&orderingTable[currentBuffer]);
}


int main() {

	initialize();
	initGame();
	do {
		loopCounter ++;
		checkPads();
		sprintf(fullText, "%d", loopCounter);
		FntPrint(fullText);
		drawBar(&paddle_infos[0]);
		drawBar(&paddle_infos[1]);
		display();
	} while(1);
	return 0;
}

