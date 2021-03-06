/*
 * matrix.c
 * 
 * Copyright 2014 Diego Torres
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 */

#include <stdlib.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <string.h>
#include "vroot.h"
#include <time.h>

#define MAXSPEED 100

// Acquired 19 using the two following lines:
// int test = XTextWidth(fs, symbols, 63);
// fprintf(stderr, "lineheight: %d", fs->ascent + fs->descent);
#define LINEHEIGHT 19

// Acquired by printing width of each character.
#define LINEWIDTH 10

// falling->movTime has to reach MOVTHRESH so that code can move down.
#define MOVTHRESH 100

// GENPROB/1000 is the probability that a frame creates a new falling code.
#define GENPROB 1000

// FALLDEATH/1000 is the probability that a falling code dies.
#define FALLDEATH 10

// The maximum amount of frames the tail of a falling code will exist.
#define MAXTAILLIFE 250

char symbols[65] = "0987654321abcdefghijklmnopqrstuvwxyz~!#$%^&*()+=-[]';/.,<>?:\"{}";
//char symbols[65] = "0987654321abcdefghijklmnopqrstuvwxyz~!@#$%^&*()+=-[]';/.,<>?:\"{}";
int sizeFall = 0;
int sizeStill = 0;
int lsyms;

typedef struct Falling{
	int sym;
	//int coprime;
	int x;
	int y;
	int speed;
	int movTime;
	int tailLife;
	struct Falling *next;
} Falling;

typedef struct Still{
	int sym;
	int x;
	int y;
	int time;
	int lifespan;
	struct Still *next;
} Still;

void addFalling(Falling **head, int nCols, int nRows){
	Falling *old = *head;
	
	(*head) = malloc(sizeof(Falling));
	(*head)->sym = rand()%lsyms;
	//(*head)->coprime = 2*(rand()%31)+1;
	(*head)->x = (rand()%nCols) * LINEWIDTH;
	(*head)->y = (rand()%nRows) * LINEHEIGHT;
	// Low speeds should be rare.
	(*head)->speed = (rand()%MAXSPEED + rand()%MAXSPEED) / 2;
	(*head)->movTime = 0;
	(*head)->tailLife = rand()%MAXTAILLIFE+1;
	
	(*head)->next = old;
	sizeFall++;
}

void deleteFalling(Falling **pb){
	Falling *b = *pb;
	*pb = (*pb)->next;
	free(b);
	sizeFall--;
	return;
}

void addStill(Still **head, int x, int y, int lifespan){
	Still *old = *head;
	
	(*head) = malloc(sizeof(Still));
	(*head)->sym = rand()%lsyms;
	(*head)->x = x;
	(*head)->y = y;
	(*head)->time = 0;
	(*head)->lifespan = lifespan;
	
	(*head)->next = old;
	sizeStill++;
}

void deleteStill(Still **pb){
	Still *b = *pb;
	*pb = (*pb)->next;
	free(b);
	sizeStill--;
	return;
}

Still *headStill;
Falling *headFall;
	
void printList(Falling *headFall){
	Falling *try = headFall;
	fprintf(stderr, "##################\n");
	while(try != NULL){
		fprintf(stderr, "%c, ", try->sym);
		try = try->next;
	}
	if (try == NULL)
		fprintf(stderr, "NULL.");
	fprintf(stderr, "\n");
}

int updateStill(Display *dpy, Window buff, GC g, XFontStruct *fs, XColor blacks, XColor greens, Still **tryStill){
                     
	// Clean last movement
	int cleanWidth = XTextWidth(fs, symbols + (*tryStill)->sym, 1);
	int cleanHeight = fs->ascent + fs->descent;
	XSetForeground(dpy, g, blacks.pixel);
	XFillRectangle(dpy, buff, g, (*tryStill)->x, 
					(*tryStill)->y - fs->ascent, LINEWIDTH,
					fs->ascent + fs->descent);
					
	(*tryStill)->time++;
	
	// Change symbol displayed
	(*tryStill)->sym = rand()%lsyms;
	/*if ((*tryStill)->sym >= lsyms-1)
		(*tryStill)->sym= 0;
	else
		(*tryStill)->sym++; */
	
	if ((*tryStill)->time == (*tryStill)->lifespan){
		/*int cleanWidth = XTextWidth(fs, symbols + (*tryStill)->sym, 1);
		int cleanHeight = fs->ascent + fs->descent;
							XSetForeground(dpy, g, blacks.pixel);
		XFillRectangle(dpy, root, g, (*tryStill)->x, 
						(*tryStill)->y - fs->ascent, LINEWIDTH,
						fs->ascent + fs->descent);*/
		deleteStill(tryStill);
		return 1;
	}
	
	// Final draw
	//if((*tryStill)->time == 1){
	XSetForeground(dpy, g, greens.pixel);
	XDrawString(dpy, buff, g, (*tryStill)->x, (*tryStill)->y, 
				symbols + (*tryStill)->sym, 1);
	//}
		
	tryStill = &(*tryStill)->next;
	
	return 0;
}

int updateFalling(Display *dpy, Window buff, GC g, XFontStruct *fs, XColor blacks, XColor whites, XColor greens, int height, Falling **tryFall){
	int cleanWidth = XTextWidth(fs, symbols + (*tryFall)->sym, 1);
	int cleanHeight = fs->ascent + fs->descent;
	
	// Used to acquire LINEWIDTH.
	// fprintf(stderr, "cleanWidth: %d\n", cleanWidth);
	
	// Die randomly or due to screen exit
	if(rand()%10000 < 80 || (*tryFall)->y > height){
		// Remove corpse
		XSetForeground(dpy, g, blacks.pixel);
		XFillRectangle(dpy, buff, g, (*tryFall)->x, (*tryFall)->y - fs->ascent, LINEWIDTH,
					fs->ascent + fs->descent);
					
		deleteFalling(tryFall);
		return 1;
	}
	
	// Movement
	(*tryFall)->movTime += (*tryFall)->speed;
	if ((*tryFall)->movTime >= MOVTHRESH){
		(*tryFall)->movTime = 0;
		
		// Create tail
		addStill(&headStill, (*tryFall)->x, (*tryFall)->y, 
					(*tryFall)->tailLife);
		updateStill(dpy, buff, g, fs, blacks, greens, &headStill);
		
		(*tryFall)->y = (*tryFall)->y + LINEHEIGHT;
		
	}
	
	// Clean last movement
	XSetForeground(dpy, g, blacks.pixel);
	XFillRectangle(dpy, buff, g, (*tryFall)->x, (*tryFall)->y - fs->ascent, LINEWIDTH,
					fs->ascent + fs->descent);
					
	
	// Change symbol displayed
	(*tryFall)->sym = rand()%lsyms;
	/*if ((*tryFall)->sym >= lsyms-1)
		(*tryFall)->sym = 0;
	else
		(*tryFall)->sym++; */
	
	// Final draw
	XSetForeground(dpy, g, whites.pixel);
	XDrawString(dpy, buff, g, (*tryFall)->x, (*tryFall)->y, 
				symbols + (*tryFall)->sym, 1);
	
	tryFall = &(*tryFall)->next;
	
	return 0;
}

main (){
	Display *dpy;
	Window root;
	XWindowAttributes wa;
	GC g;

	Font f;
	XFontStruct *fs;
	XGCValues v;

	XColor whitex, whites, greenx, greens, blackx, blacks;
	
	Pixmap double_buffer;
	
	int nCols, nRows;

	srand(time(NULL));
	
	lsyms = strlen(symbols);
	
	/* open the display (connect to the X server) */
	dpy = XOpenDisplay (getenv ("DISPLAY"));

	/* get the root window */
	root = DefaultRootWindow (dpy);

	/* get attributes of the root window */
	XGetWindowAttributes(dpy, root, &wa);

	/* create a GC for drawing in the window */
	g = XCreateGC (dpy, root, 0, NULL);

	/* load a font */
	f=XLoadFont(dpy, "-*-matrix code nfi-*-*-*-*-*-120-*-*-*-*-*-*");
	XSetFont(dpy, g, f);

	/* get font metrics */
	XGetGCValues (dpy, g, GCFont, &v);
	fs = XQueryFont (dpy, v.font);
	
	/* create the double buffer */
	double_buffer = XCreatePixmap(dpy, root,
                  wa.width, wa.height, wa.depth);
	XSetForeground(dpy, g, BlackPixelOfScreen(DefaultScreenOfDisplay(dpy)));
	XFillRectangle(dpy, double_buffer, g, 0, 0, wa.width, wa.height);


	// Allocate white
	XAllocNamedColor(dpy,
                     DefaultColormapOfScreen(DefaultScreenOfDisplay(dpy)),
                     "white",
                     &whites, &whitex);
    
    // Allocate black
	XAllocNamedColor(dpy,
                     DefaultColormapOfScreen(DefaultScreenOfDisplay(dpy)),
                     "black",
                     &blacks, &blackx);
                     
	// Allocate green
	XAllocNamedColor(dpy,
                     DefaultColormapOfScreen(DefaultScreenOfDisplay(dpy)),
                     "green",
                     &greens, &greenx);

	/* set foreground color */
	XSetForeground(dpy, g, whites.pixel);
	
	// Create lists of falling and stills.
	headFall = NULL;
	headStill = NULL;
	
	nCols = wa.width / LINEWIDTH;
	nRows = wa.height / LINEHEIGHT;
	
	//XFlush(dpy);		
	while (1) {
		// Generate new falling code
		if (rand()%FALLDEATH < GENPROB)
			addFalling(&headFall, nCols, nRows);
		
		// Update stills
		Still **tryStill = &headStill;
		while((*tryStill) != NULL){
			if (!updateStill(dpy, double_buffer, g, fs, blacks, greens, tryStill))
				tryStill = &(*tryStill)->next;
		}
		
		// Update falling
		Falling **tryFall = &headFall;
		while((*tryFall) != NULL){
			if (!updateFalling(dpy, double_buffer, g, fs, blacks, whites, greens, wa.height, tryFall))
				tryFall = &(*tryFall)->next;
		}
		
		/* XSetForeground(dpy, g, blacks.pixel);
		XFillRectangle(dpy, double_buffer, g, 20, 20-LINEHEIGHT, LINEWIDTH*64, LINEHEIGHT);
		XSetForeground(dpy, g, greens.pixel);
		XDrawString(dpy, double_buffer, g, 20, 20, symbols, 64); */
				
		/* copy the buffer to the window */
		XCopyArea(dpy, double_buffer, root, g, 0, 0, wa.width, wa.height, 0, 0);
		
		//fprintf(stderr, "Length of symbols: %d\n", strlen(symbols));
		//fprintf(stderr, "%c\n", symbols[38]);
		
		/* flush changes and sleep */
		XFlush(dpy);
		usleep (55000);
		

	}

	XCloseDisplay (dpy);
}
