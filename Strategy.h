/********************************************************
*	Strategy.h : 策略接口文件                           *
*	张永锋                                              *
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
	添加你自己的辅助函数
*/

int TreePolicy(int t);
void putLoc(int x, int y, bool u);
int checkResult(int x, int y, bool u);
int BestChildNode(int t, double inputC);
void boardInitialization(int M, int N, int noX, int noY);
std::pair<int, int> DefaultPolicy(int state);
int backUp(std::pair<int, int> input);

#endif
