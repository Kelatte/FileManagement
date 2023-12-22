/*----------------------------------------------
File: printfc.hh -- printf with color
Date: 2014/2/24 16:52:35
Author: js2854@gmail.com
----------------------------------------------*/
#pragma once
#ifndef __PRINTFC_H__
#define __PRINTFC_H__

#include <stdio.h>
#include <string>

#define MAX_STR_LEN		2048

#ifndef WIN32
#include <stdarg.h> //vsnprintf,va_start,va_end
#include <string.h> //strok
const typedef enum _ForeColor
{
	FG_BLACK = 30, //��ɫ
	FG_RED = 31, //��ɫ
	FG_GREEN = 32, //��ɫ
	FG_YELLOW = 33, //��ɫ
	FG_BLUE = 34, //��ɫ
	FG_PURPLE = 35, //��ɫ
	FG_DARKGREEN = 36, //����ɫ
	FG_WHITE = 37, //��ɫ
}ForeColor;

const typedef enum _BackColor
{
	BG_BLACK = 40, //��ɫ
	BG_DARKRED = 41, //���ɫ
	BG_GREEN = 42, //��ɫ
	BG_YELLOW = 43, //��ɫ
	BG_BLUE = 44, //��ɫ
	BG_PURPLE = 45, //��ɫ
	BG_DARKGREEN = 46, //����ɫ
	BG_WHITE = 47, //��ɫ
}BackColor;

#else
#include <windows.h>
const typedef enum _ForeColor
{
	FG_BLACK = 0, //��ɫ
	FG_RED = FOREGROUND_INTENSITY | FOREGROUND_RED, //��ɫ
	FG_GREEN = FOREGROUND_INTENSITY | FOREGROUND_GREEN, //��ɫ
	FG_YELLOW = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN, //��ɫ
	FG_BLUE = FOREGROUND_INTENSITY | FOREGROUND_BLUE, //��ɫ
	FG_PURPLE = FOREGROUND_RED | FOREGROUND_BLUE, //��ɫ
	FG_DARKGREEN = FOREGROUND_GREEN, //����ɫ
	FG_WHITE = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE, //��ɫ
}ForeColor;

const typedef enum _BackColor
{
	BG_BLACK = 0, //��ɫ
	BG_DARKRED = BACKGROUND_RED, //���ɫ
	BG_GREEN = BACKGROUND_INTENSITY | BACKGROUND_GREEN, //��ɫ
	BG_YELLOW = BACKGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN, //��ɫ
	BG_BLUE = BACKGROUND_INTENSITY | BACKGROUND_BLUE, //��ɫ
	BG_PURPLE = BACKGROUND_RED | BACKGROUND_BLUE, //��ɫ
	BG_DARKGREEN = BACKGROUND_GREEN, //����ɫ
	BG_WHITE = BACKGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE, //��ɫ
}BackColor;

#endif

int printfc(ForeColor fc, std::string str, ...);

int printfc(ForeColor fc, BackColor bc, std::string str,  ...);

int perrorc(std::string str);
int pfilec(std::string str);
int pdirc(std::string str);
int ppathc(std::string str);
int pinfoc(std::string str);
int pstatc(std::string str);
int psucc(std::string str);

#endif //__PRINTFC_H__
