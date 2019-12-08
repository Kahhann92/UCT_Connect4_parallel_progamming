/********************************************************
*	Strategy.h : ���Խӿ��ļ�                           *
*	������                                              *
*	zhangyf07@gmail.com                                 *
*	2010.8                                              *
*********************************************************/

#ifndef STRATEGY_H_
#define	STRATEGY_H_

#include "Point.h"

extern "C" __declspec(dllexport) Point* getPoint(const int M, const int N, const int* top, const int* _board, 
	const int lastX, const int lastY, const int noX, const int noY);

extern "C" __declspec(dllexport) void clearPoint(Point* p);

void clearArray(int M, int N, int** board);

/*
	������Լ��ĸ�������
*/

int TreePolicy(int t);
void putLoc(int x, int y, bool u);
int checkResult(int x, int y, bool u);
int BestChildNode(int t, double inputC);
void boardInitialization(int M, int N, int noX, int noY);
void boardRenew(int** inBoard, const int* inTop);
std::pair<int, int> DefaultPolicy(int state);
int backUp(std::pair<int, int> input);

#endif