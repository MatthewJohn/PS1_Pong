/*
 * main.c
 *
 *  Created on: 3 Sep 2020
 *	  Author: matt
 */

#include <stdlib.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include <libetc.h>
#include <libpad.h>

#include "numbers_font_tim.h"

#define OTSIZE	10		/* size of OT */
#define MAXOBJ	4000		/* max ball number */
#define OT_LENGTH 10
GsOT		OT[2], *System_CurOT;
GsOT_TAG	OT_zSort[2][1 << OT_LENGTH];

typedef struct {
	DRAWENV	draw;		/* drawing environment */
	DISPENV	disp;		/* display environment */
	u_long	ot[OTSIZE];	/* OT entry */
	SPRT_16	sprt[MAXOBJ];	/* 16x16 sprite */
} DB;


/* double buffer */
DB	db[2];
DB	*cdb;
					   // OT handler

int System_Prio;
System_Prio = (1 << OT_LENGTH) - 1;

#define __ramsize   0x00200000
#define __stacksize 0x00004000

#define DEBUG 1

int SCREEN_WIDTH, SCREEN_HEIGHT;


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
//POLY_FT4 poly_score_numbers[PADDLE_COUNT];
GsSPRITE poly_score_numbers[PADDLE_COUNT];
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

//void setupScoreNumber(POLY_FT4 *p) {
void setupScoreNumber(GsSPRITE *p) {
		//setSprt(p);
	   //p->attribute = 0 << 24; // Set bits 24-25 to 1 for 8-bit CLUT texture
	   p->x	= SCORE_NUMBER_MARGIN_LEFT;
	   p->y	= SCORE_NUMBER_MARGIN_TOP;
	   p->w	= SCORE_NUMBER_WIDTH;
	   p->h	= SCORE_NUMBER_HEIGHT;
	   p->tpage = GetTPage((numbers_font.pmode & 3), 0, numbers_font.px, numbers_font.py);
	   p->u = 320; //numbers_font.px;
	   p->v = 0; //numbers_font.py;
	   //p->tpage	= GetTPage(0, 0, numbers_font.px, numbers_font.py);
	   //p->clut	= GetClut(320, 64);
	   sprintf(debugText, "%d", GetClut(320, 64));
	   p->cx	= numbers_font.cx;
	   p->cy   = numbers_font.cy;

	   //p->r0	= 128;
	   //p->g0   = 128;
	   //p->b0   = 128;
	   p->scalex   = 4096; // Scale factor * 4096
	   p->scaley   = 4096;



//	int twidth, theight, basepx, basepy;
//
//	// Set texture page and color depth attribute
//	p->tpage	   = GetTPage((numbers_font.pmode & 3), 0, numbers_font.px, numbers_font.py);
//	//p->tpage	   = GetTPage(1, 0, 320, 0);
//	//p->attribute   = (numbers_font.pmode & 3) << 24;
//
//	// CLUT coords
//	p->clut		  = GetClut(numbers_font.cx, numbers_font.cy);
//	setPolyFT4(p);
//	setXY4(p,
//		SCORE_NUMBER_MARGIN_LEFT, SCORE_NUMBER_MARGIN_TOP + SCORE_NUMBER_HEIGHT,
//		SCORE_NUMBER_MARGIN_LEFT + SCORE_NUMBER_WIDTH, SCORE_NUMBER_MARGIN_TOP + SCORE_NUMBER_HEIGHT,
//		SCORE_NUMBER_MARGIN_LEFT, SCORE_NUMBER_MARGIN_TOP,
//		SCORE_NUMBER_MARGIN_LEFT + SCORE_NUMBER_WIDTH, SCORE_NUMBER_MARGIN_TOP
//	);
//	//basepx = numbers_font.px;
//	//basepy = numbers_font.py;
//	twidth = 20;
//	theight = 20;
//	//sprintf(debugText, "%d", theight);
//	//basepx = 320;
//	basepx = 320;
//	basepy = 0;
//
//	setUV4(p,
//			basepx		 , basepy + theight,
//			basepx + twidth, basepy + theight,
//			basepx		 , basepy,
//			basepx + twidth, basepy
//
//			);
//	setRGB0(p, 64, 64, 64);
//	//p->cx		  = numbers_font.cx;
//	//p->cy		  = numbers_font.cy;
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
	opponentTargetPos = calculateTargetMovement();

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

	RECT rect;

	ResetGraph(0);		/* initialize Renderer		*/
	InitGeom();		/* initialize Geometry		*/

	//SetGraphDebug(0);	/* set graphics debug mode */

	/* clear image to reduce display distortion */
	setRECT(&rect, 0, 0, 320, 256);
	ClearImage(&rect, 0, 0, 0);
	SetDispMask(1);

	/* set debug mode (0:off,1:monitor,2:dump) */
	//SetGraphDebug(0);

	/* set up V-sync callback function */
	//VSyncCallback(cbvsync);

	/* set up rendering/display environment for double buffering
		when rendering (0, 0) - (320,240), display (0,240) - (320,480) (db[0])
		when rendering (0,240) - (320,480), display (0,  0) - (320,240) (db[1])  */
	SetDefDrawEnv(&db[0].draw, 0,   0, 320, 240);
	SetDefDrawEnv(&db[1].draw, 0, 240, 320, 240);
	SetDefDispEnv(&db[0].disp, 0, 240, 320, 240);
	SetDefDispEnv(&db[1].disp, 0,   0, 320, 240);

	OT[0].length = OT_LENGTH;
	OT[0].org = OT_zSort[0];
	OT[1].length = OT_LENGTH;
	OT[1].org = OT_zSort[1];



	FntLoad(960, 256);
	SetDumpFnt(FntOpen(0, 230, 300, 10, 1, 512));

	db[0].draw.isbg = 1;
	setRGB0(&db[0].draw, 255, 255, 255);
	db[1].draw.isbg = 1;
	setRGB0(&db[1].draw, 255, 255, 255);

	//LoadTexture(&numbers_font, (u_long*)numbers_font_tim);
	db[0].draw.tpage = LoadTPage((u_long*)numbers_font_tim, 1, 0, 320, 0, 50, 40);
	db[1].draw.tpage = db[0].draw.tpage ; //LoadTPage((u_long*)numbers_font_tim, 1, 0, 320, 0, 16, 16);
	//db[0].draw.tpage = GetTPage((numbers_font.pmode & 3), 0, numbers_font.px, numbers_font.py);
	//db[1].draw.tpage = GetTPage((numbers_font.pmode & 3), 0, numbers_font.px, numbers_font.py);

}


void display() {
	u_long	*ot;

	if (DEBUG)
		FntPrint(debugText);

	cdb  = (cdb==db)? db+1: db;

	//DumpDrawEnv(&cdb->draw);
	//DumpDispEnv(&cdb->disp);
	//DumpTPage(cdb->draw.tpage);

	ClearOTag(cdb->ot, OTSIZE);

	ot = cdb->ot;
	//AddPrim(cdb->ot, &poly_score_numbers[0]);
	GsSortSprite(&poly_score_numbers[0], &cdb->ot, System_Prio--);
	DrawSync(0);

	/* wait for Vsync */
	VSync(0);		/* wait for V-BLNK (1/60) */

	/* swap double buffer */
	/* display environment */
	PutDispEnv(&cdb->disp);

	/* drawing environment */
	PutDrawEnv(&cdb->draw);

	/* draw OT */
	DrawOTag(cdb->ot);
	FntFlush(-1);
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

