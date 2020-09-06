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
	SetDumpFnt(FntOpen(20, 230, 200, 10, 1, 512)); //Sets the dumped font for use with FntPrint();
}

void initializeOrderingTable(GsOT* orderingTable){
	GsClearOt(0,0,&orderingTable[GsGetActiveBuff()]);
}



GsOT orderingTable[2];
short currentBuffer;
char scoreText[100] = "Begin Game";
char debugText[100] = "Begin Game";


long global_timer = 0;
short seed_set = 0;


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
#define BOUNDARY_X1 280
#define BOUNDARY_Y1 200

POLY_F4 poly_boundary_lines[4];
object_inf boundary_lines[4];

// ----- PADDLE INFO  --------------------
#define PADDLE_SPEED 5
#define PADDLE_COUNT 2
#define PADDLE_HEIGHT 25
#define PADDLE_WIDTH 4
#define PADDLE_L_R 20
#define PADDLE_L_G 120
#define PADDLE_L_B 67
#define PADDLE_R_R 80
#define PADDLE_R_G 200
#define PADDLE_R_B 200
#define PADDLE_INITIAL_Y 120
#define PADDLE_BOUNDARY_POS_MARGIN 20

POLY_F4 poly_paddles[PADDLE_COUNT];
object_inf paddle_infos[PADDLE_COUNT];

// ----- ball INFO  --------------------
#define FPS 60
POLY_F4 poly_ball;
object_inf ball;
int ballV_x;
int ballV_y;

#define BALLV_Y_MAX 4
#define BALL_REFLECTION_FACTOR 3

int ballFrameCount; // ball movement across multiple frames

// --------- Opponent info -------------------
enum OpponentState{
	NOTHING,
	WAITING,
	MOVING_TO_BALL,
	REACHED_POS
};
enum OpponentState currentOpponentState;
short opponentTimeWaited;
short opponentTimeToWait;
short opponentTargetPos;
#define OPPONENT_MIN_WAIT 30
#define OPPONENT_MAX_WAIT 80
#define OPPONENT_MISS_CHANCE 25


// ----- gamepad INFO  --------------------
u_long padButtons;

// -----Net line -------------------------
POLY_F4 poly_net_line;
object_inf net_line;

int score;


int polyIntersects(object_inf* a, object_inf *b) {
	return (
			((a->x + (a->width / 2)) >= (b->x - (b->width / 2))) &&
			((a->x - (a->width / 2)) <= (b->x + (b->width / 2))) &&
			((a->y + (a->height / 2)) >= (b->y - (b->height / 2))) &&
			((a->y - (a->height / 2)) <= (b->y + (b->height / 2)))
		);
}

void ballPaddleCollision(object_inf *paddle) {
	// Get difference in y between ball and paddle
	int Vy_factor = (ball.y - paddle->y) / BALL_REFLECTION_FACTOR;

	if (ballV_y == 0 && Vy_factor != 0)
		ballV_y = Vy_factor;
    ballV_y = ballV_y * Vy_factor;

	// Ensure ball in travelling in direction of the paddle side (e.g. top vs bottom)
    if ((Vy_factor > 0 && ballV_y < 0) || (Vy_factor < 0 && ballV_y > 0))
    	ballV_y = 0 - ballV_y;


	// If vertical velocity exceeds max,
	// set to max
	if (ballV_y > BALLV_Y_MAX)
		ballV_y = BALLV_Y_MAX;
	else if (ballV_y < -BALLV_Y_MAX)
		ballV_y = -BALLV_Y_MAX;

	// Point ball in opposite direction
	ballV_x = 0 - ballV_x;

	//sprintf(debugText, "V: %d  F: %d", ballV_y, Vy_factor);
}

void endRound(int playerLose) {
	// Temp check left/right boundaries
	ballV_x = 0 - ballV_x;
	if (playerLose)
		score --;
	else
		score ++;
}

void checkCollisions() {
	if (polyIntersects(&ball, &paddle_infos[0]))
		ballPaddleCollision(&paddle_infos[0]);
	if (polyIntersects(&ball, &paddle_infos[1]))
		ballPaddleCollision(&paddle_infos[1]);

	// Check collision with upper/lower boundaries
	if (ball.y <= BOUNDARY_Y0 || ball.y >= BOUNDARY_Y1)
		ballV_y = 0 - ballV_y;

	if (ball.x <= BOUNDARY_X0)
		endRound(1);
	else if (ball.x >= BOUNDARY_X1)
		endRound(0);
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
        p->x - (p->width / 2), p->y - (p->height / 2),
        p->x + (p->width / 2), p->y - (p->height / 2)
    );
    DrawPrim(p->poly);
}

void setRandomSeed() {
	if (seed_set == 0) {
		seed_set = 1;
        srand((unsigned int)global_timer ^ GetRCnt(2));
	}
}

void checkPads() {
	padButtons = PadRead(1);
	if (padButtons) {
		setRandomSeed();
	}
	if(padButtons & PADLup) paddle_infos[0].y -= PADDLE_SPEED;
	if(padButtons & PADLdown) paddle_infos[0].y += PADDLE_SPEED;
}

int calculateBallHitPos() {
	int court_height = BOUNDARY_Y1 - BOUNDARY_Y0;
	short delta_x = (BOUNDARY_X1 - PADDLE_BOUNDARY_POS_MARGIN) - ball.x;
	short delta_y = (delta_x / ballV_x) * ballV_y;
	// Convert delta_y to distance from top of boundary Y
	delta_y += (ball.y - BOUNDARY_Y0);

	delta_y += court_height;


	delta_y = delta_y % (court_height * 2);

	if (delta_y < court_height) {
		sprintf(debugText, "BBBB: %d", (BOUNDARY_Y0 + court_height - delta_y));
		return ((BOUNDARY_Y0 + court_height) - delta_y);
	} else {
		sprintf(debugText, "BBBB: %d", (BOUNDARY_Y0 + (delta_y - court_height)));
		return (BOUNDARY_Y0 + (delta_y - court_height));
	}
}

int calculateTargetMovement() {
	int ballEndPos = calculateBallHitPos();
	int decision = rand() % 100;
	if (decision < OPPONENT_MISS_CHANCE) {
		sprintf(debugText, "Goofin it up");
		if (ballEndPos <= (BOUNDARY_Y1 / 2 + BOUNDARY_Y0))
			return ballEndPos + PADDLE_HEIGHT;
		else
			return ballEndPos - PADDLE_HEIGHT;
	}
	return ballEndPos;
}

void moveComputer() {
	if (ballV_x > 0) {
		//sprintf(debugText, "TO WAIT: %d  WAITED: %d", opponentTimeToWait, opponentTimeWaited);
		switch (currentOpponentState) {
			case NOTHING:
				opponentTimeToWait = rand() % (OPPONENT_MAX_WAIT - OPPONENT_MIN_WAIT) + OPPONENT_MIN_WAIT;
				opponentTimeWaited = 0;
				currentOpponentState = WAITING;
				break;
			case WAITING:
				opponentTimeWaited ++;
				if (opponentTimeWaited >= opponentTimeToWait) {
					currentOpponentState = MOVING_TO_BALL;
					opponentTargetPos = calculateTargetMovement();
				}
				break;
			case MOVING_TO_BALL:
                if ((paddle_infos[1].y + (PADDLE_SPEED - 1)) < opponentTargetPos)
			        paddle_infos[1].y += PADDLE_SPEED;
		        else if ((paddle_infos[1].y - (PADDLE_SPEED - 1)) > opponentTargetPos)
			        paddle_infos[1].y -= PADDLE_SPEED;
                break;
			case REACHED_POS:
				break;
		}
	} else {
		currentOpponentState = NOTHING;
	}
}

void initGame() {
	global_timer = 0;
	currentOpponentState = NOTHING;

	// Setup boundary lines
	boundary_lines[0].poly = &poly_boundary_lines[0];
	boundary_lines[0].y = BOUNDARY_Y0;
	boundary_lines[0].x = ((BOUNDARY_X1 - BOUNDARY_X0) / 2) + BOUNDARY_X0;
	boundary_lines[0].width = BOUNDARY_X1 - BOUNDARY_X0;
	boundary_lines[0].height = 2;
	setupObject(&boundary_lines[0], 0, 0, 0);

	boundary_lines[1].poly = &poly_boundary_lines[1];
	boundary_lines[1].y = ((BOUNDARY_Y1 - BOUNDARY_Y0) / 2) + BOUNDARY_Y0;
	boundary_lines[1].x = BOUNDARY_X1;
	boundary_lines[1].width = 2;
	boundary_lines[1].height = BOUNDARY_Y1 - BOUNDARY_Y0;
	setupObject(&boundary_lines[1], 0, 0, 0);

	boundary_lines[2].poly = &poly_boundary_lines[2];
	boundary_lines[2].y = BOUNDARY_Y1;
	boundary_lines[2].x = ((BOUNDARY_X1 - BOUNDARY_X0) / 2) + BOUNDARY_X0;
	boundary_lines[2].width = BOUNDARY_X1 - BOUNDARY_X0;
	boundary_lines[2].height = 2;
	setupObject(&boundary_lines[2], 0, 0, 0);

	boundary_lines[3].poly = &poly_boundary_lines[3];
	boundary_lines[3].y = ((BOUNDARY_Y1 - BOUNDARY_Y0) / 2) + BOUNDARY_Y0;
	boundary_lines[3].x = BOUNDARY_X0;
	boundary_lines[3].width = 2;
	boundary_lines[3].height = BOUNDARY_Y1 - BOUNDARY_Y0;
	setupObject(&boundary_lines[3], 0, 0, 0);

	//Netline
	net_line.poly = &poly_net_line;
	net_line.y = ((BOUNDARY_Y1 - BOUNDARY_Y0) / 2) + BOUNDARY_Y0;
	net_line.x = ((BOUNDARY_X1 - BOUNDARY_X0) / 2) + BOUNDARY_X0;
	net_line.width = 2;
	net_line.height = BOUNDARY_Y1 - BOUNDARY_Y0;
	setupObject(&net_line, 0, 0, 0);

	// Setup paddle structs
	paddle_infos[0].poly = &poly_paddles[0];
	paddle_infos[0].x = BOUNDARY_X0 + PADDLE_BOUNDARY_POS_MARGIN;
	paddle_infos[0].y = PADDLE_INITIAL_Y;
	paddle_infos[0].height = PADDLE_HEIGHT;
	paddle_infos[0].width = PADDLE_WIDTH;
	setupObject(&paddle_infos[0], PADDLE_L_R, PADDLE_L_G, PADDLE_L_B);

	paddle_infos[1].poly = &poly_paddles[1];
	paddle_infos[1].x = BOUNDARY_X1 - PADDLE_BOUNDARY_POS_MARGIN;
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

    score = 0;
}

void drawScore() {
	sprintf(scoreText, "Score: %f", score);
	FntPrint(scoreText);
}

void initialize() {
	initializeScreen();
	initializeDebugFont();
}

void display() {

	if (DEBUG)
		FntPrint(debugText);

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
		global_timer ++;

		checkPads();
		moveComputer();

		moveBall();

		checkCollisions();

		drawObject(&boundary_lines[0]);
		drawObject(&boundary_lines[1]);
		drawObject(&boundary_lines[2]);
		drawObject(&boundary_lines[3]);
		drawObject(&net_line);

		drawObject(&paddle_infos[0]);
		drawObject(&paddle_infos[1]);
		drawObject(&ball);

		drawScore();

		display();
	} while(1);
	return 0;
}

