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

#define __ramsize   0x00200000
#define __stacksize 0x00004000

#define DEBUG 1

int SCREEN_WIDTH, SCREEN_HEIGHT;


GsIMAGE numbers_font;



// -- SCORE -----------------------------
int scores[2];
POLY_FT4 poly_score_numbers[2];
#define SCORE_NUMBER_HEIGHT 20
#define SCORE_NUMBER_WIDTH 20
#define SCORE_NUMBER_MARGIN_LEFT 50
#define SCORE_NUMBER_MARGIN_TOP 30


TIM_IMAGE NumberTim;


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


    setPolyFT4(p);
    setXY4(p,
    	SCORE_NUMBER_MARGIN_LEFT, SCORE_NUMBER_MARGIN_TOP + SCORE_NUMBER_HEIGHT,
    	SCORE_NUMBER_MARGIN_LEFT + SCORE_NUMBER_WIDTH, SCORE_NUMBER_MARGIN_TOP + SCORE_NUMBER_HEIGHT,
    	SCORE_NUMBER_MARGIN_LEFT, SCORE_NUMBER_MARGIN_TOP,
        SCORE_NUMBER_MARGIN_LEFT + SCORE_NUMBER_WIDTH, SCORE_NUMBER_MARGIN_TOP
    );
    basepx = 0;
    basepy = 0;

    //basepx = 320;
    //basepx = 0;
    //basepy = 0;
    setUV4(p,
    		basepx, basepy,
    		basepx + SCORE_NUMBER_HEIGHT , basepy,
    		basepx	, basepy + SCORE_NUMBER_HEIGHT ,
    		basepx + SCORE_NUMBER_HEIGHT , basepy + SCORE_NUMBER_HEIGHT );

    // Set texture page and color depth attribute
    //p->tpage       = GetTPage((numbers_font.pmode & 3), 0, numbers_font.px, numbers_font.py);
	p->tpage       = GetTPage(0, 0, numbers_font.px, numbers_font.py);

    //p->attribute   = (numbers_font.pmode & 3) << 24;

    // CLUT coords
    p->clut          = GetClut(320, 64);
    setRGB0(p, 64, 64, 64);
    //p->cx          = numbers_font.cx;
    //p->cy          = numbers_font.cy;
}

void LoadTexture(GsIMAGE *image, u_long *addr) {
    RECT rect;

    // Get TIM information
    GsGetTimInfo((addr+1), image);

    // Load the texture image
    rect.x = image->px; rect.y = image->py;
    rect.w = image->pw; rect.h = image->ph;
	LoadImage(&rect, image->pixel);
    DrawSync(0);

    // Load the CLUT (if there is one)
    if ((image->pmode>>3) & 0x01) {
        rect.x = image->cx; rect.y = image->cy;
        rect.w = image->cw; rect.h = image->ch;
        LoadImage(&rect, image->clut);
        DrawSync(0);
    }
//		long tim;
//		tim = OpenTIM( addr);
//		ReadTIM( &NumberTim );
//		LoadImage( NumberTim.prect, NumberTim.paddr );
//		printf( "NumberTim = %d, { %d, %d, %d, %d }\n", tim, NumberTim.prect->x, NumberTim.prect->y, NumberTim.prect->w, NumberTim.prect->h );

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


	FntLoad(960, 256);
	SetDumpFnt(FntOpen(0, 230, 300, 10, 1, 512));

	db[0].draw.isbg = 1;
	setRGB0(&db[0].draw, 255, 255, 255);
	db[1].draw.isbg = 1;
	setRGB0(&db[1].draw, 255, 255, 255);

	LoadTexture(&numbers_font, (u_long*)numbers_font_tim);
	//db[0].draw.tpage = LoadTPage((u_long*)numbers_font_tim, 0, 0, 640, 0, 16, 16);
	//db[1].draw.tpage = LoadTPage((u_long*)numbers_font_tim, 0, 0, 640, 0, 16, 16);
	db[0].draw.tpage = GetTPage(0, 0, numbers_font.px, numbers_font.py);
	db[1].draw.tpage = GetTPage(0, 0, numbers_font.px, numbers_font.py);

}

void display() {
	/* current OT */
	u_long	*ot;

	SPRT_16	*sp;		/* work */

//	if (DEBUG)
//		FntPrint(debugText);

	cdb  = (cdb==db)? db+1: db;

	//DumpDrawEnv(&cdb->draw);
	//DumpDispEnv(&cdb->disp);
	//DumpTPage(cdb->draw.tpage);

	ClearOTag(cdb->ot, OTSIZE);

	ot = cdb->ot;
	sp = cdb->sprt;
	AddPrim(cdb->ot, &poly_score_numbers[0]);
	DrawSync(0);

	/* get CPU consuming time */
	/* cnt = VSync(1); */

	/* for 30fps operation */
	/* cnt = VSync(2); */

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
    setupScoreNumber(&poly_score_numbers[0]);

	do {
		display();
	} while(1);
	return 0;
}

