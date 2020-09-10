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
#include "title_tim.h"

#define OTSIZE	1		/* size of OT */
#define MAXOBJ	4000		/* max ball number */

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

#define __ramsize   0x00200000
#define __stacksize 0x00004000

#define DEBUG 1

int SCREEN_WIDTH, SCREEN_HEIGHT;


char scoreText[100] = "Begin Game";
char debugText[100] = "Debug Text";


long global_timer = 0;
short seed_set = 0;

u_short numbers_clut;
u_short numbers_tpage;
u_short title_clut;
u_short title_tpage;

GsIMAGE numbers_font_image;
GsIMAGE title_image;


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
#define BOUNDARY_YC ((BOUNDARY_Y1 - BOUNDARY_Y0) / 2) + BOUNDARY_Y0
#define BOUNDARY_XC ((BOUNDARY_X1 - BOUNDARY_X0) / 2) + BOUNDARY_X0

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
POLY_FT4 poly_score_numbers[3];
#define SCORE_NUMBER_HEIGHT 20
#define SCORE_NUMBER_WIDTH 20
#define SCORE_NUMBER_MARGIN_LEFT 50
#define SCORE_NUMBER_MARGIN_TOP 30

#define TEX_SCORE_BASEX 0
#define TEX_SCORE_BASEY 0
#define TEX_SCORE_NUM_HEIGHT 20
#define TEX_SCORE_NUM_WIDTH 20


POLY_FT4 title_poly;
#define TITLE_Y 60
#define TEX_TITLE_BASEX 0
#define TEX_TITLE_BASEY 41
#define TEX_TITLE_WIDTH 70
#define TEX_TITLE_HEIGHT 24

enum GameState{
	GS_TITLE_SCREEN,
	GS_COUNTDOWN,
	GS_PLAYING,
	GS_GAMEOVER
};
enum GameState currentGameState;

#define COUNTDOWN_TIMER_START 3
int countDownTimer;

#define MAX_SCORE 9


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

void resetBall() {
	// Start buffer
	int BALL_START_MARGIN_Y = 20;
	int BALL_START_VEL_X_LOWER = 1;
	int BALL_START_VEL_X_UPPER = 3;
	ball.x = (BOUNDARY_X1 - BOUNDARY_X0) / 2 + BOUNDARY_X0;
	ball.y = (rand() % ((BOUNDARY_Y1 - BOUNDARY_Y0) - (BALL_START_MARGIN_Y * 2))) + BOUNDARY_Y0 + BALL_START_MARGIN_Y;
	ballV_x = rand() % 3;
	if (ballV_x <= 1)
		ballV_x = -2;
	else
		ballV_x = 2;
	ballV_y = (rand() % (BALL_START_VEL_X_UPPER - BALL_START_VEL_X_LOWER)) + BALL_START_VEL_X_LOWER;

	currentOpponentState = MOVING_TO_BALL;
	opponentTargetPos = calculateTargetMovement();

	currentGameState = GS_COUNTDOWN;
	countDownTimer = COUNTDOWN_TIMER_START * FPS;
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
	if (padButtons)
		setRandomSeed();

	if (currentGameState == GS_TITLE_SCREEN) {
		if (padButtons & PADstart)
			currentGameState = GS_COUNTDOWN;

	} else if (currentGameState == GS_PLAYING) {
		if(padButtons & PADLup) paddle_infos[0].y -= PADDLE_SPEED;
		if(padButtons & PADLdown) paddle_infos[0].y += PADDLE_SPEED;
	}
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
//	sprintf(debugText, "%c:%d dx: %d, dy: %d, Pred: %d",
//			typeT,
//			wasNegative,
//			(BOUNDARY_X1 - PADDLE_BOUNDARY_POS_MARGIN) - ball.x,
//			(delta_x / ballV_x) * ballV_y,
//			delta_y);
	return BOUNDARY_Y0 + delta_y;
}

int calculateTargetMovement() {
	int ballEndPos = calculateBallHitPos();
	int decision = rand() % 100;
	if (decision < OPPONENT_MISS_CHANCE) {
		//sprintf(debugText, "Goofin it up");
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

void setupScoreNumber(POLY_FT4 *p, u_short *tpage, u_short *clut, int x, int y) {
	int twidth, theight, basepx, basepy;
	setPolyFT4(p);

	p->clut = *clut;
	p->tpage = *tpage;

	setXY4(p,
		x - (SCORE_NUMBER_WIDTH / 2), y,
		x + (SCORE_NUMBER_WIDTH / 2), y,
		x - (SCORE_NUMBER_WIDTH / 2), y + SCORE_NUMBER_HEIGHT,
		x + (SCORE_NUMBER_WIDTH / 2), y + SCORE_NUMBER_HEIGHT
	);

	//sprintf(debugText, "%d", numbers_font.cy);
}

typedef struct XYCord {
	int x;
	int y;
} XYCord;

XYCord convertScoreToTextureXY(int score) {
	XYCord cords;
	if (score == 0) {
		cords.x = 4;
		cords.y = 1;
	} else {
		cords.y = (score - 1) / 5;
		cords.x = (score - 1) % 5;
	}

	cords.x *= TEX_SCORE_NUM_WIDTH;
	cords.y *= TEX_SCORE_NUM_HEIGHT;

	cords.x += TEX_SCORE_BASEX;
	cords.y += TEX_SCORE_BASEY;

	return cords;
}

void updateScorePoly(POLY_FT4 *p, int score)
{
	XYCord cords = convertScoreToTextureXY(score);
	//sprintf(debugText, "%d", texture_image.ph);
	setUV4(p,
		cords.x					     , cords.y,
		cords.x + TEX_SCORE_NUM_WIDTH, cords.y,
		cords.x					     , cords.y + TEX_SCORE_NUM_HEIGHT,
		cords.x + TEX_SCORE_NUM_WIDTH, cords.y + TEX_SCORE_NUM_HEIGHT
	);
}

void endGame() {
	currentGameState = GS_GAMEOVER;
}

void updateScoreNumbers() {
	updateScorePoly(&poly_score_numbers[0], scores[0]);
	updateScorePoly(&poly_score_numbers[1], scores[1]);
}

void endRound(int playerLose) {
	// Temp check left/right boundaries
	ballV_x = 0 - ballV_x;
	if (playerLose)
		scores[1] ++;
	else
		scores[0] ++;

	updateScoreNumbers();

	// Check if player won
	if (scores[0] >= MAX_SCORE || scores[1] >= MAX_SCORE) {
		endGame();
		return;
	}

	resetBall();
}

void setupLogo() {
	setPolyFT4(&title_poly);

	title_poly.clut = title_clut;
	title_poly.tpage = title_tpage;

	setXY4(&title_poly,
		BOUNDARY_XC - (TEX_TITLE_WIDTH / 2), TITLE_Y,
		BOUNDARY_XC + (TEX_TITLE_WIDTH / 2), TITLE_Y,
		BOUNDARY_XC - (TEX_TITLE_WIDTH / 2), TITLE_Y + TEX_TITLE_HEIGHT,
		BOUNDARY_XC + (TEX_TITLE_WIDTH / 2), TITLE_Y + TEX_TITLE_HEIGHT
	);
	sprintf(debugText, "%d", title_image.px);
	setUV4(&title_poly,
		TEX_TITLE_BASEX					 , TEX_TITLE_BASEY,
		TEX_TITLE_BASEX + TEX_TITLE_WIDTH, TEX_TITLE_BASEY,
		TEX_TITLE_BASEX					 , TEX_TITLE_BASEY + TEX_TITLE_HEIGHT,
		TEX_TITLE_BASEX + TEX_TITLE_WIDTH, TEX_TITLE_BASEY + TEX_TITLE_HEIGHT
	);
}

void importTextures() {
	// Import textures
	GsGetTimInfo((u_long *)(numbers_font_tim + 4), &numbers_font_image);
	numbers_tpage = LoadTPage(numbers_font_image.pixel, numbers_font_image.pmode & 3, 0, numbers_font_image.px, numbers_font_image.py, numbers_font_image.pw * 2, numbers_font_image.ph);
	numbers_clut = LoadClut(numbers_font_image.clut, numbers_font_image.cx, numbers_font_image.cy);

	GsGetTimInfo((u_long *)(title_tim + 4), &title_image);
	title_tpage = LoadTPage(title_image.pixel, title_image.pmode & 3, 0, title_image.px, title_image.py, title_image.pw * 2, title_image.ph);
	title_clut = LoadClut(title_image.clut, title_image.cx, title_image.cy);
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
	ball.width = 2;
	ball.height = 2;
	setupObject(&ball, 0, 0, 0);
	ballFrameCount = 0;
	resetBall();

	// Initialise controllers
	PadInit(0);

	scores[0] = 0;
	scores[1] = 0;


	importTextures();

	setupScoreNumber(&poly_score_numbers[0], &numbers_tpage, &numbers_clut, BOUNDARY_X0 + SCORE_NUMBER_MARGIN_LEFT, SCORE_NUMBER_MARGIN_TOP);
	setupScoreNumber(&poly_score_numbers[1], &numbers_tpage, &numbers_clut, BOUNDARY_X1 - SCORE_NUMBER_MARGIN_LEFT, SCORE_NUMBER_MARGIN_TOP);
	updateScoreNumbers();

	// Countdown number
	setupScoreNumber(&poly_score_numbers[2], &numbers_tpage, &numbers_clut, ((BOUNDARY_X1 - BOUNDARY_X0) / 2) + BOUNDARY_X0, ((BOUNDARY_Y1 - BOUNDARY_Y0) / 2) + BOUNDARY_Y0);

	// Title
	setupLogo();

	currentGameState = GS_TITLE_SCREEN;
}

void drawScore() {
	DrawPrim(&poly_score_numbers[0]);
	DrawPrim(&poly_score_numbers[1]);
	sprintf(scoreText, "Score: %f", scores[0]);
	if (currentGameState == GS_COUNTDOWN)
		DrawPrim(&poly_score_numbers[2]);
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

	SetDefDrawEnv(&db[0].draw, 0,   0, 320, 240);
	SetDefDrawEnv(&db[1].draw, 0, 240, 320, 240);
	SetDefDispEnv(&db[0].disp, 0, 240, 320, 240);
	SetDefDispEnv(&db[1].disp, 0,   0, 320, 240);

	FntLoad(960, 256);
	SetDumpFnt(FntOpen(0, 230, 300, 10, 1, 512));

	db[0].draw.isbg = 1;
	setRGB0(&db[0].draw, 255, 255, 255);
	db[1].draw.isbg = 1;
	setRGB0(&db[1].draw, 255, 255, 255);
}

void display() {
	u_long	*ot;

	if (DEBUG)
		FntPrint(debugText);

	cdb  = (cdb==db)? db+1: db;

	ClearOTag(cdb->ot, OTSIZE);

	ot = cdb->ot;
	DrawSync(0);

	VSync(0);		/* wait for V-BLNK (1/60) */

	PutDispEnv(&cdb->disp);
	PutDrawEnv(&cdb->draw);

	DrawOTag(cdb->ot);
	FntFlush(-1);
}

void updateGameState() {
	checkPads();

	if (currentGameState == GS_TITLE_SCREEN) {
		checkPads();
	} else if (currentGameState == GS_PLAYING) {
		moveComputer();

		moveBall();

		checkCollisions();
	} else if (currentGameState == GS_COUNTDOWN) {
		countDownTimer --;
		updateScorePoly(&poly_score_numbers[2], (countDownTimer / FPS) + 1);
		if (countDownTimer == 0)
			currentGameState = GS_PLAYING;
	} else {
		sprintf(debugText, "Game Over");
	}
}

void drawObjects() {
	if (currentGameState == GS_TITLE_SCREEN) {
		DrawPrim(&title_poly);
	} else if (
			(currentGameState == GS_COUNTDOWN) ||
			(currentGameState == GS_PLAYING) ||
			(currentGameState == GS_GAMEOVER)
	) {
		drawObject(&boundary_lines[0]);
		drawObject(&boundary_lines[1]);
		drawObject(&boundary_lines[2]);
		drawObject(&boundary_lines[3]);
		drawObject(&net_line);

		drawScore();

		drawObject(&paddle_infos[0]);
		drawObject(&paddle_infos[1]);

		drawObject(&ball);
	}
}

int main() {

	initialize();
	initGame();
	do {
		global_timer ++;

		updateGameState();
		drawObjects();

		display();
	} while(1);
	return 0;
}
