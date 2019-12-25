#include <iostream>
#include "Point.h"
#include "Strategy.h"
#include "Judge.h"
#include <math.h>
#include <stdio.h>
#include <ctime>
#include <cstdio>
#include <stdlib.h>
#include <string>
#include <cassert>
#include <windows.h>
using namespace std;

#include <mpi.h>

//#define DEBUG
#define MESAURE
#define STATS

#include <chrono>
#define TIMERSTART(tag)  auto tag##_start = std::chrono::steady_clock::now(),tag##_end = tag##_start
#define TIMEREND(tag)  tag##_end =  std::chrono::steady_clock::now()
#define DURATION_s(tag) printf("%s costs %d s\n",#tag,std::chrono::duration_cast<std::chrono::seconds>(tag##_end - tag##_start).count())
#define DURATION_ms(tag) printf("%s costs %d ms\n",#tag,std::chrono::duration_cast<std::chrono::milliseconds>(tag##_end - tag##_start).count());
#define DURATION_us(tag) printf("%s costs %d us\n",#tag,std::chrono::duration_cast<std::chrono::microseconds>(tag##_end - tag##_start).count());
#define DURATION_ns(tag) if(#tag,std::chrono::duration_cast<std::chrono::nanoseconds>(tag##_end - tag##_start).count())printf("%s costs %d ns\n",#tag,std::chrono::duration_cast<std::chrono::nanoseconds>(tag##_end - tag##_start).count());

const double c = 0.6625, err = 0.000001; //err 用于平移，以免出现0/0的情况
const int MAX_NODE = 10000000;      //set the maximum number of nodes available
const int BUF_SIZE= 2000000;
const int MAX_ITERATIONS = 8000000;  //set a max iterations so that it won't go out of the maximum arrays allowable
int _noX, _noY, boardRow, boardCol; //main parameters of the chess board
int** board1; //copy board
int top1[12]; //top
int canloc[12];

/*
	策略函数接口,该函数被对抗平台调用,每次传入当前状态,要求输出你的落子点,该落子点必须是一个符合游戏规则的落子点,不然对抗平台会直接认为你的程序有误
	
	input:
		为了防止对对抗平台维护的数据造成更改，所有传入的参数均为const属性
		M, N : 棋盘大小 M - 行数 N - 列数 均从0开始计， 左上角为坐标原点，行用x标记，列用y标记
		top : 当前棋盘每一列列顶的实际位置. e.g. 第i列为空,则_top[i] == M, 第i列已满,则_top[i] == 0
		_board : 棋盘的一维数组表示, 为了方便使用，在该函数刚开始处，我们已经将其转化为了二维数组board
				你只需直接使用board即可，左上角为坐标原点，数组从[0][0]开始计(不是[1][1])
				board[x][y]表示第x行、第y列的点(从0开始计)
				board[x][y] == 0/1/2 分别对应(x,y)处 无落子/有用户的子/有程序的子,不可落子点处的值也为0
		lastX, lastY : 对方上一次落子的位置, 你可能不需要该参数，也可能需要的不仅仅是对方一步的
				落子位置，这时你可以在自己的程序中记录对方连续多步的落子位置，这完全取决于你自己的策略
		noX, noY : 棋盘上的不可落子点(注:其实这里给出的top已经替你处理了不可落子点，也就是说如果某一步
				所落的子的上面恰是不可落子点，那么UI工程中的代码就已经将该列的top值又进行了一次减一操作，
				所以在你的代码中也可以根本不使用noX和noY这两个参数，完全认为top数组就是当前每列的顶部即可,
				当然如果你想使用lastX,lastY参数，有可能就要同时考虑noX和noY了)
		以上参数实际上包含了当前状态(M N _top _board)以及历史信息(lastX lastY),你要做的就是在这些信息下给出尽可能明智的落子点
	output:
		你的落子点Point
*/

extern "C" __declspec(dllexport) Point* getPoint(const int M, const int N, const int* top, const int* _board, 
	const int lastX, const int lastY, const int noX, const int noY){

	char child[] = "C:\\Users\\lanvent\\Desktop\\mpi\\AI_Project\\win\\Sourcecode\\UCT_Connect4_parallel_progamming\\Debug\\subprocess.exe";
	MPI_Comm children,everyone;
	int threadsnum=0;
	for(int i=0;i<M;i++){
		if(top[i]>0){
			canloc[threadsnum++]=i;
		}
	}

	MPI_Comm_spawn(child, MPI_ARGV_NULL, threadsnum,MPI_INFO_NULL, 0, MPI_COMM_SELF, &children,MPI_ERRCODES_IGNORE);
	MPI_Intercomm_merge(children,0,&everyone);
	
	for(int i=0;i<threadsnum;i++){//发送给子进程，它需要下子的列
		MPI_Send(canloc+i,1,MPI_INT,i,i,children);
	}

	MPI_Bcast((void *)&M,1,MPI_INT,0,everyone);
	MPI_Bcast((void *)&N,1,MPI_INT,0,everyone);
	MPI_Bcast((void *)&noX,1,MPI_INT,0,everyone);
	MPI_Bcast((void *)&noY,1,MPI_INT,0,everyone);
	MPI_Datatype linetype,boardtype;
	MPI_Type_contiguous(N,MPI_INT,&linetype);// one line (1xN) 
	MPI_Type_contiguous(M,linetype,&boardtype);// M line (MxN)
	MPI_Type_commit(&linetype);
	MPI_Type_commit(&boardtype);
	MPI_Bcast((void*)top,1,linetype,0,everyone);
	MPI_Bcast((void*)_board,1,boardtype,0,everyone);

	std::clock_t start = std::clock()

	int x = -1, y = -1;//最终将你的落子点存到x,y中

	int** board = new int*[M];
	board1 = new int*[M];
	for(int i = 0; i < M; i++){
		board[i] = new int[N];
		board1[i] = new int[N];
		for(int j = 0; j < N; j++){
			board[i][j] = _board[i * N + j];
		}
	}
	
	boardInitialization(M,N,noX,noY);
	//====================================================================================UCT SEARCH starts
	Sleep(2300);
	double maxUCB =-100000;
	double UCB;
	for(int i=0;i<threadsnum;i++){//接受子进程的UCB
		MPI_Recv(&UCB,1,MPI_DOUBLE,i,i,children,MPI_STATUSES_IGNORE);
		if(UCB>maxUCB){
			int loc=canloc[i];
			maxUCB=UCB;
			x=top[loc]-1;
			y=loc;
		}
		cout<<canloc[i]<<":"<<UCB<<" ";
	}
	cout<<endl;

//	int bestNode = BestChildNode(0,0);

	//====================================================================================UCT SEARCH ends

	MPI_Comm_disconnect(&children);//需要disconnect 不然子进程不会关闭
	MPI_Comm_disconnect(&everyone);

	clearArray(M,N,board1);
	clearArray(M,N,board);
	cout<<x<<" "<<y<<endl;
	return new Point(x, y);
}


/*
	getPoint函数返回的Point指针是在本dll模块中声明的，为避免产生堆错误，应在外部调用本dll中的
	函数来释放空间，而不应该在外部直接delete
*/
extern "C" __declspec(dllexport) void clearPoint(Point* p){
	delete p;
	return;
}

/*
	清除board数组
*/
void clearArray(int M, int N, int** board){
	for(int i = 0; i < M; i++){
		delete[] board[i];
	}
	delete[] board;
}

void boardInitialization(int M, int N, int noX, int noY)
{
	boardRow = M;
	boardCol = N;
	_noX = noX;
	_noY = noY;
}

