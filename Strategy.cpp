#include <iostream>
#include "Point.h"
#include "Strategy.h"
#include "Judge.h"
#include <math.h>
#include <stdio.h>
#include <ctime>
#include <cstdio>
#include <stdlib.h>

using namespace std;

const double c = 0.6625, err = 0.000001; //err ����ƽ�ƣ��������0/0�����
const int MAX_NODE = 10000000;
const int MAX_ITERATIONS = 800000;
int _noX, _noY, boardRow, boardCol, p;
int firstChild[MAX_NODE],lastChild[MAX_NODE],father[MAX_NODE]; //to indicate the relation of the mentioned node
int reward[MAX_NODE],totalCount[MAX_NODE],locX[MAX_NODE],locY[MAX_NODE],result[MAX_NODE]; //all the variables related to the node
bool user[MAX_NODE];   //player
int** board1; //copy board
int top1[12]; //top

/*
	���Ժ����ӿ�,�ú������Կ�ƽ̨����,ÿ�δ��뵱ǰ״̬,Ҫ�����������ӵ�,�����ӵ������һ��������Ϸ��������ӵ�,��Ȼ�Կ�ƽ̨��ֱ����Ϊ��ĳ�������
	
	input:
		Ϊ�˷�ֹ�ԶԿ�ƽ̨ά����������ɸ��ģ����д���Ĳ�����Ϊconst����
		M, N : ���̴�С M - ���� N - ���� ����0��ʼ�ƣ� ���Ͻ�Ϊ����ԭ�㣬����x��ǣ�����y���
		top : ��ǰ����ÿһ���ж���ʵ��λ��. e.g. ��i��Ϊ��,��_top[i] == M, ��i������,��_top[i] == 0
		_board : ���̵�һά�����ʾ, Ϊ�˷���ʹ�ã��ڸú����տ�ʼ���������Ѿ�����ת��Ϊ�˶�ά����board
				��ֻ��ֱ��ʹ��board���ɣ����Ͻ�Ϊ����ԭ�㣬�����[0][0]��ʼ��(����[1][1])
				board[x][y]��ʾ��x�С���y�еĵ�(��0��ʼ��)
				board[x][y] == 0/1/2 �ֱ��Ӧ(x,y)�� ������/���û�����/�г������,�������ӵ㴦��ֵҲΪ0
		lastX, lastY : �Է���һ�����ӵ�λ��, ����ܲ���Ҫ�ò�����Ҳ������Ҫ�Ĳ������ǶԷ�һ����
				����λ�ã���ʱ��������Լ��ĳ����м�¼�Է������ಽ������λ�ã�����ȫȡ�������Լ��Ĳ���
		noX, noY : �����ϵĲ������ӵ�(ע:��ʵ���������top�Ѿ����㴦���˲������ӵ㣬Ҳ����˵���ĳһ��
				������ӵ�����ǡ�ǲ������ӵ㣬��ôUI�����еĴ�����Ѿ������е�topֵ�ֽ�����һ�μ�һ������
				��������Ĵ�����Ҳ���Ը�����ʹ��noX��noY��������������ȫ��Ϊtop������ǵ�ǰÿ�еĶ�������,
				��Ȼ�������ʹ��lastX,lastY�������п��ܾ�Ҫͬʱ����noX��noY��)
		���ϲ���ʵ���ϰ����˵�ǰ״̬(M N _top _board)�Լ���ʷ��Ϣ(lastX lastY),��Ҫ���ľ�������Щ��Ϣ�¸������������ǵ����ӵ�
	output:
		������ӵ�Point
*/
extern "C" __declspec(dllexport) Point* getPoint(const int M, const int N, const int* top, const int* _board, 
	const int lastX, const int lastY, const int noX, const int noY){
	/*
		��Ҫ������δ���
	*/
	std::clock_t start = std::clock();
	srand(time(NULL));
	int x = -1, y = -1;//���ս�������ӵ�浽x,y��
	
	board1 = new int*[M];
	int** board = new int*[M];
	for(int i = 0; i < M; i++){
		board[i] = new int[N];
		board1[i] = new int[N];
		for(int j = 0; j < N; j++){
			board[i][j] = _board[i * N + j];
		}
	}
	
	/*
		�������Լ��Ĳ������������ӵ�,Ҳ���Ǹ�����Ĳ�����ɶ�x,y�ĸ�ֵ
		�ò��ֶԲ���ʹ��û�����ƣ�Ϊ�˷���ʵ�֣�����Զ����Լ��µ��ࡢ.h�ļ���.cpp�ļ�
	*/
	//Add your own code below
	/*
     //a naive example
	for (int i = N-1; i >= 0; i--) {
		if (top[i] > 0) {
			x = top[i] - 1;
			y = i;
			break;
		}
	}
    */
	//for(int i = 0; i < M; i++)
		//board1[i] = new int[N];
	
	boardInitialization(M,N,noX,noY);
	int iterations = -1;

	//====================================================================================UCT SEARCH starts

	while ((double)(std::clock() - start) / CLOCKS_PER_SEC < 2.3 && iterations < MAX_ITERATIONS)
	{
		iterations++;

		for(int i = 0; i < boardRow; i++)				//copy board
			for( int j = 0; j<boardCol; j++)
				board1[i][j] = board[i][j];
		for(int i = 0; i < boardCol; i++)
			top1[i] = top[i];

		int t = 0; //back to rootroot


		while(firstChild[t] !=-1)						//tree policy
		{
			t = BestChildNode(t,c);
			putLoc(locX[t],locY[t],user[t]);
		}

		if(totalCount[t] ==0)							//first encountered node, check result
		{
			result[t] = checkResult(locX[t],locY[t],user[t]);
		}

		if(result[t] != -2)   // the game is end, either win, lose or tie, do backup
		{
			int temp = result[t];
			t = backUp(std::make_pair(temp , t));
			continue;
		}

		t = backUp(DefaultPolicy(t));


	}

	int bestNode = BestChildNode(0,0);

	//====================================================================================UCT SEARCH ends



	x = locX[bestNode];
	y = locY[bestNode];
	clearArray(M,N,board1);


	/*
	��Ҫ������δ���
	*/
	clearArray(M, N, board);
	return new Point(x, y);
}


/*
	getPoint�������ص�Pointָ�����ڱ�dllģ���������ģ�Ϊ��������Ѵ���Ӧ���ⲿ���ñ�dll�е�
	�������ͷſռ䣬����Ӧ�����ⲿֱ��delete
*/
extern "C" __declspec(dllexport) void clearPoint(Point* p){
	delete p;
	return;
}

/*
	���top��board����
*/
void clearArray(int M, int N, int** board){
	for(int i = 0; i < M; i++){
		delete[] board[i];
	}
	delete[] board;
}


/*
	������Լ��ĸ�������������������Լ����ࡢ����������µ�.h .cpp�ļ�������ʵ������뷨
*/
int TreePolicy(int t)
{
	int ans = BestChildNode(t,c);
	putLoc(locX[ans],locY[ans],user[ans]);
	return ans;
}

int BestChildNode(int t, double inputC)
{
	double compare =-100000, uct;
	int ans = 1;
	for( int i = firstChild[t]; i <= lastChild[t]; i++)
	{
		if( user[t] == 0 )
			uct = (double)reward[i] / ((double)totalCount[i] + err) + inputC * sqrt(2 * log((double)totalCount[father[i]] + 1.001) / ((double)totalCount[i] + err));
		else
			uct = -(double)reward[i] / ((double)totalCount[i] + err) + inputC * sqrt(2 * log((double)totalCount[father[i]] + 1.001) / ((double)totalCount[i] + err));

		if( uct > compare )
		{
			compare = uct;
			ans = i;
		}
	}
	return ans;
}

void putLoc(int x, int y, bool u)
{
	board1[x][y] = u + 1;
	top1[y]--;
	if( x == _noX + 1 && y == _noY )
		top1[y]--;
}
int checkResult(int x, int y, bool u)
{
	if ( u == 0 && userWin(x,y,boardRow,boardCol,board1) )
		return -1;
	if ( u ==1 && machineWin(x,y,boardRow,boardCol,board1) )
		return 1;
	if ( isTie(boardCol,top1) )
		return 0;
	return -2;
}
void boardInitialization(int M, int N, int noX, int noY)
{
	boardRow = M;
	boardCol = N;
	_noX = noX;
	_noY = noY;
	p = 0;
	firstChild[0] = -1;
	user[0] =0;
	result[0] = -2;
}

void boardRenew(int** inBoard, const int* inTop)
{

}

std::pair<int, int> DefaultPolicy(int state)
{

	//open all available nodes
	firstChild[state] = p + 1;
	for(int i = 0; i < boardCol; i++)        
	{
		if(top1[i] > 0)
		{
			firstChild[++p] = -1;
			reward[p] = totalCount[p] = 0;
			father[p] = state;
			user[p] = !user[state];              //different from the last step
			locX[p] = top1[i] - 1;
			locY[p] = i;
			result[p] = -2;						//mark that its not done
		}
	}
	lastChild[state] = p;
	int k = rand() % (lastChild[state] - firstChild[state] + 1) + firstChild[state]; //randomly select a point to put
	putLoc(locX[k],locY[k],user[k]);
	int delta = checkResult(locX[k],locY[k],user[k]);
	result[k]= delta;
	bool u = !user[k];

	while(delta ==-2)                            //play until the end
	{
		int y1 = rand() % boardCol, x1;
		while(top1[y1] ==0)
			y1 = rand() % boardCol;
		x1 = top1[y1] - 1;
		putLoc(x1,y1,u);

		delta = checkResult(x1,y1,u);
		u = !u;
	}
	
	state = k;  // k becomes the current node

	return std::make_pair(delta , state ); 
}

int backUp(std::pair<int, int> input)
{
	int rewardInput = input.first;
	int t = input.second;
	while(t != 0)                             //do backup
	{
		totalCount[t]++;
		reward[t]+= rewardInput;
		t = father[t];
	}
	totalCount[0]++;
	return t;
}