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

#include "numbers_font_tim.h"

#define OT_LENGTH 10
#define PACKETMAX 18
#define OT_ENTRIES  1<<OT_LENGTH

#define PACKETMAX   2048


GsOT        myOT[2];                        // OT handlers


#define __ramsize   0x00200000
#define __stacksize 0x00004000

#define DEBUG 1

int SCREEN_WIDTH, SCREEN_HEIGHT;

GsOT orderingTable[2];
GsOT_TAG    orderingTableTag[2][OT_ENTRIES];        // OT tables
short currentBuffer;
PACKET packetArea[2][PACKETMAX*24];

char scoreText[100] = "Begin Game";
char debugText[100] = "Debug Text";


long global_timer = 0;
short seed_set = 0;


GsIMAGE numbers_font;


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
#define PADDLE_SPEED 3
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
#define BALL_REFLECTION_FACTOR 4

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
#define OPPONENT_MISS_CHANCE 10


// ----- gamepad INFO  --------------------
u_long padButtons;

// -----Net line -------------------------
POLY_F4 poly_net_line;
object_inf net_line;

// -- SCORE -----------------------------
int scores[PADDLE_COUNT];
POLY_FT4 poly_score_numbers[PADDLE_COUNT];
#define SCORE_NUMBER_HEIGHT 20
#define SCORE_NUMBER_WIDTH 20
#define SCORE_NUMBER_MARGIN_LEFT 50
#define SCORE_NUMBER_MARGIN_TOP 30





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
		scores[1] ++;
	else
		scores[0] ++;
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
    int wasNegative = 0;
	char typeT;

	// Convert delta_y to distance from top of boundary Y
	delta_y += (ball.y - BOUNDARY_Y0);

    if (delta_y < 0) {
    	wasNegative = 1;
    	delta_y = 0 - delta_y;

    }

	delta_y += court_height;

	delta_y = delta_y % (court_height * 2);

	if (delta_y > court_height) {
		delta_y -= court_height;
		typeT = 'a';
	} else {
		delta_y = court_height - delta_y;
		typeT = 'b';
	}
	sprintf(debugText, "%c:%d dx: %d, dy: %d, Pred: %d",
			typeT,
			wasNegative,
			(BOUNDARY_X1 - PADDLE_BOUNDARY_POS_MARGIN) - ball.x,
			(delta_x / ballV_x) * ballV_y,
			delta_y);
	return BOUNDARY_Y0 + delta_y;
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

void setupScoreNumber(POLY_FT4 *p) {
	int x, y;
	int basepx, basepy;

    // Set texture size and coordinates
//    switch (numbers_font.pmode & 3) {
//        case 0: // 4-bit
//            sprite->w = numbers_font.pw << 2;
//            sprite->u = (numbers_font.px & 0x3f) * 4;
//            break;
//        case 1: // 8-bit
//            sprite->w = numbers_font.pw << 1;
//            sprite->u = (numbers_font.px & 0x3f) * 2;
//            break;
//        default: // 16-bit
//            sprite->w = numbers_font.pw;
//            sprite->u = numbers_font.px & 0x3f;
//    };

    //p->h = numbers_font.ph;
    //p->v = numbers_font.py & 0xff;

    // Set texture page and color depth attribute
    //p->tpage       = GetTPage((numbers_font.pmode & 3), 0, numbers_font.px, numbers_font.py);
	p->tpage       = GetTPage(0, 0, numbers_font.px, numbers_font.py);
	sprintf(debugText, GetClut(numbers_font.cx, numbers_font.cy));
    //p->attribute   = (numbers_font.pmode & 3) << 24;

    // CLUT coords
    p->clut          = GetClut(numbers_font.cx, numbers_font.cy);
    setPolyFT4(p);
    setXY4(p,
    	SCORE_NUMBER_MARGIN_LEFT, SCORE_NUMBER_MARGIN_TOP + SCORE_NUMBER_HEIGHT,
    	SCORE_NUMBER_MARGIN_LEFT + SCORE_NUMBER_WIDTH, SCORE_NUMBER_MARGIN_TOP + SCORE_NUMBER_HEIGHT,
    	SCORE_NUMBER_MARGIN_LEFT, SCORE_NUMBER_MARGIN_TOP,
        SCORE_NUMBER_MARGIN_LEFT + SCORE_NUMBER_WIDTH, SCORE_NUMBER_MARGIN_TOP
    );
    basepx = numbers_font.px;
    basepy = numbers_font.py;
    //basepx = 320;
    //basepx = 0;
    //basepy = 0;
    setUV4(p,
    		basepx, basepy,
    		basepx + 10, basepy,
    		basepx, basepy + 10,
    		basepx + 10, basepy + 10);
    //setRGB0(p, 64, 64, 64);
    //p->cx          = numbers_font.cx;
    //p->cy          = numbers_font.cy;
}

void initGame() {
	global_timer = 0;

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

	currentOpponentState = MOVING_TO_BALL;
	//opponentTargetPos = calculateTargetMovement();

	// Initialise controllers
	PadInit(0);

    scores[0] = 0;
    scores[1] = 0;

    setupScoreNumber(&poly_score_numbers[0]);
}

void drawScore() {
	sprintf(scoreText, "Score: %f", scores[0]);
	//FntPrint(scoreText);
}

void LoadTexture(GsIMAGE *image, u_long *addr) {
    RECT rect;

    // Get TIM information
    GsGetTimInfo((addr+1), image);

    // Load the texture image
    rect.x = image->px; rect.y = image->py;
    rect.w = image->pw; rect.h = image->ph;
    if (LoadImage2(&rect, image->pixel)) {
    	sprintf(debugText, "failure load1");
    	FntPrint(debugText);
    }
    DrawSync(0);

    // Load the CLUT (if there is one)
    if ((image->pmode>>3) & 0x01) {
        rect.x = image->cx; rect.y = image->cy;
        rect.w = image->cw; rect.h = image->ch;
        if (LoadImage2(&rect, image->clut)) {
           sprintf(debugText, "failure load2");
           FntPrint(debugText);
        }
        DrawSync(0);
    }
}

void initialize() {
	if (*(char *)0xbfc7ff52=='E') { // SCEE string address
    	// PAL
    	SCREEN_WIDTH = 320;
    	SCREEN_HEIGHT = 256;
    	SetVideoMode(1);
   	} else {
     	// NTSC
     	SCREEN_WIDTH = 320;
     	SCREEN_HEIGHT = 240;
     	SetVideoMode(0);
    }
	GsInitGraph(SCREEN_WIDTH, SCREEN_HEIGHT, GsINTER|GsOFSGPU, 1, 0);
	GsDefDispBuff(0, 0, 0, SCREEN_HEIGHT);

	GsInitGraph(SCREEN_WIDTH, SCREEN_HEIGHT, GsINTER|GsOFSGPU, 1, 0);
	GsDefDispBuff(0, 0, 0, SCREEN_HEIGHT);

	orderingTable[0].length = OT_LENGTH;
	orderingTable[1].length = OT_LENGTH;
    orderingTable[0].org    = orderingTableTag[0];
    orderingTable[1].org    = orderingTableTag[1];

	FntLoad(960, 256);
	SetDumpFnt(FntOpen(0, 230, 300, 10, 1, 512));

	LoadTexture(&numbers_font, (u_long*)numbers_font_tim);
}

void display() {

	if (DEBUG)
		FntPrint(debugText);

	currentBuffer = GsGetActiveBuff();

	GsSetWorkBase((PACKET*)packetArea[currentBuffer]);
	FntFlush(-1);
	GsClearOt(0, 0, &orderingTable[currentBuffer]);
	DrawSync(0);
	VSync(0);
	GsSwapDispBuff();
	GsSortClear(255, 255, 255, &orderingTable[currentBuffer]);

	GsSortPoly(&poly_score_numbers[0], &orderingTable[currentBuffer], 0);


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
		//DrawPrim(&poly_score_numbers[0]);

		display();
	} while(1);
	return 0;
}

