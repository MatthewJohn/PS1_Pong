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
	int width;
	int height;
} object_inf;


// Boundaries
#define BOUNDARY_X0 10
#define BOUNDARY_Y0 10
#define BOUNDARY_X1 240
#define BOUNDARY_Y1 200

POLY_F4 boundary_lines[4];

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
object_inf paddle_infos[2];

// ----- ball INFO  --------------------
#define FPS 60
POLY_F4 poly_ball;
object_inf ball;
float ballV_scale = 5; // Pixels per second
short ballV_x;
short ballV_y;
short ballFrameCount; // ball movement across multiple frames

// ----- gamepad INFO  --------------------
u_long padButtons;


int polyIntersects(object_inf* a, object_inf *b) {
	if (
			((a->x + (a->width / 2)) >= (b->x - (b->width / 2))) &&
			((a->x - (a->width / 2)) <= (b->x + (b->width / 2))) &&
			((a->y + (a->height / 2)) >= (b->y - (b->height / 2))) &&
			((a->y - (a->height / 2)) <= (b->y + (b->height / 2)))
		)
		return 1;
	else
		return 0;
}

void ballPaddleCollision(object_inf *paddle) {
	// Get difference in y between ball and paddle
	int diffY = ball.y - paddle->y;
	if (diffY != 0)
        ballV_y = 1 + (diffY / 2);

	// Point ball in opposite direction
	ballV_x = 0 - ballV_x;
}

void checkCollisions() {
	if (polyIntersects(&ball, &paddle_infos[0]))
		ballPaddleCollision(&paddle_infos[0]);
	if (polyIntersects(&ball, &paddle_infos[1]))
		ballPaddleCollision(&paddle_infos[1]);

	// Check collision with upper/lower boundaries
	if (ball.y <= BOUNDARY_Y0 || ball.y >= BOUNDARY_Y1)
		ballV_y = 0 - ballV_y;

	// Temp check left/right boundaries
	if (ball.x <= BOUNDARY_X0 || ball.x >= BOUNDARY_X1)
		ballV_x = 0 - ballV_x;

}

void moveBall() {
	ball.y += ballV_y;
	ball.x += ballV_x;

}

void setupObject(object_inf *p, int r, int g, int b) {
    setPolyF4(p->poly);
    setSemiTrans(p->poly, 1);
    setRGB0(p->poly, r, g, b);
}

void drawObject(object_inf *p) {
    setXY4(p->poly,
        p->x - (p->width / 2), p->y + (p->height / 2),
        p->x + (p->width / 2), p->y + (p->height / 2),
        p->x + (p->width / 2), p->y - (p->height / 2),
        p->x - (p->width / 2), p->y - (p->height / 2)
    );
    DrawPrim(p->poly);
}

void checkPads() {
	padButtons = PadRead(1);

	if(padButtons & PADLup) paddle_infos[0].y -= 2;
	if(padButtons & PADLdown) paddle_infos[0].y += 2;
}

void initGame() {

	// Setup paddle structs
	paddle_infos[0].poly = &poly_paddles[0];
	paddle_infos[0].x = 20;
	paddle_infos[0].y = PADDLE_INITIAL_Y;
	paddle_infos[0].height = PADDLE_HEIGHT;
	paddle_infos[0].width = PADDLE_WIDTH;
	setupObject(&paddle_infos[0], PADDLE_L_R, PADDLE_L_G, PADDLE_L_B);

	paddle_infos[1].poly = &poly_paddles[1];
	paddle_infos[1].x = 240;
	paddle_infos[1].y = PADDLE_INITIAL_Y;
	paddle_infos[1].height = PADDLE_HEIGHT;
	paddle_infos[1].width = PADDLE_WIDTH;
	setupObject(&paddle_infos[1], PADDLE_R_R, PADDLE_R_G, PADDLE_R_B);

	// Initialise ball
	ball.poly = &poly_ball;
	ball.x = (BOUNDARY_X1 - BOUNDARY_X0) / 2;
	ball.y = (BOUNDARY_Y1 - BOUNDARY_Y0) / 2;
	ball.width = 2;
	ball.height = 2;
	setupObject(&ball, 0, 0, 0);
	ballFrameCount = 0;
	ballV_x = 2;
	ballV_y = 1;

	// Initialise controllers
	PadInit(0);
}

void initialize() {
	initializeScreen();
	initializeDebugFont();
}

void display() {
	currentBuffer = GsGetActiveBuff();
	//FntFlush(-1);
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

		sprintf(fullText, "%d", loopCounter);
		FntPrint(fullText);


		checkPads();

		moveBall();

		checkCollisions();

		drawObject(&paddle_infos[0]);
		drawObject(&paddle_infos[1]);
		drawObject(&ball);

		display();
	} while(1);
	return 0;
}

