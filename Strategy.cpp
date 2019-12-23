#include <iostream>
#include "Point.h"
#include "Strategy.h"
#include "Judge.h"
#include <math.h>
#include <stdio.h>
#include <ctime>
#include <cstdio>
#include <stdlib.h>
#include <cassert>
#include <unordered_map>

#include <mpi.h>

//#define DEBUG
using namespace std;

const double c = 0.6625, err = 0.000001; //err 用于平移，以免出现0/0的情况
const int MAX_NODE = 10000000;      //set the maximum number of nodes available
const int BUF_SIZE= 2000000;
const int MAX_ITERATIONS = 8000000;  //set a max iterations so that it won't go out of the maximum arrays allowable
int _noX, _noY, boardRow, boardCol; //main parameters of the chess board
int layer,firstChild[MAX_NODE],lastChild[MAX_NODE],father[MAX_NODE]; //to indicate the relation of the mentioned node
int reward[MAX_NODE],totalCount[MAX_NODE],locX[MAX_NODE],locY[MAX_NODE],result[MAX_NODE]; //all the variables related to the node
bool user[MAX_NODE]; //player
int** board1; //copy board
int top1[12]; //top
struct AddNode{
	int father,locX,locY,result,reward,totalCount;
}addbuf[2][BUF_SIZE],taddbuf[BUF_SIZE];
struct UpdateNode{
	int state,reward,totalCount;
}updbuf[BUF_SIZE];
int rawreward[MAX_NODE],rawtotalCount[MAX_NODE];//保存更新前的reward和totalcount，用于计算增量

int updated[MAX_NODE];//用时间戳判断是否更新
int lastlayer;

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
bool cmp(AddNode&a, AddNode& b){ //比较函数
	if(a.father!=b.father)
		return a.father<b.father;
	if(a.locX!=b.locX)
		return a.locX<b.locX;
	if(a.locY!=b.locY)
		return a.locY<b.locY;
	return 0;
}

extern "C" __declspec(dllexport) Point* getPoint(const int M, const int N, const int* top, const int* _board, 
	const int lastX, const int lastY, const int noX, const int noY){
	/*
		不要更改这段代码
	*/ 

	char child[] = "C:\\Users\\lanvent\\Desktop\\mpi\\AI_Project\\win\\Sourcecode\\UCT_Connect4_parallel_progamming\\Debug\\subprocess.exe";
	MPI_Comm children,everyone;
	int THREADSNUM=4;
	MPI_Comm_spawn(child, MPI_ARGV_NULL, THREADSNUM,MPI_INFO_NULL, 0, MPI_COMM_SELF, &children,MPI_ERRCODES_IGNORE);
	MPI_Intercomm_merge(children,0,&everyone);
	
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


	//for(int i=0;i<THREADSNUM;i++){
	//	MPI_Send(&i,1,MPI_INT,i,i,everyone);
	//}

	std::clock_t start = std::clock();
	//srand(time(NULL));

	int x = -1, y = -1;//最终将你的落子点存到x,y中
	
	board1 = new int*[M];
	int** board = new int*[M];
	for(int i = 0; i < M; i++){
		board[i] = new int[N];
		board1[i] = new int[N];
		for(int j = 0; j < N; j++){
			board[i][j] = _board[i * N + j];
		}
	}
	
	boardInitialization(M,N,noX,noY);
	int iterations = -1;
	int timestamp=0;
	//====================================================================================UCT SEARCH starts
	AddNode* curabuf=addbuf[0];
	AddNode* nextabuf=addbuf[1];
	while ((double)(std::clock() - start) / CLOCKS_PER_SEC < 2.3 && iterations < MAX_ITERATIONS)
	{
		++iterations;
		++timestamp;
		lastlayer=layer;

		MPI_Bcast(&timestamp,1,MPI_INT,0,everyone);
		#ifdef DEBUG
		cout<<"main: timestamp "<<timestamp<<endl;
		#endif
		//MPI_Barrier(everyone);
		int addNum=0;
		for(int i=0;i<THREADSNUM;i++){
			int taddNum;
			MPI_Recv(&taddNum,1,MPI_INT,i,i,children,MPI_STATUS_IGNORE);
			#ifdef DEBUG
			if(i==0)
				cout<<"main: Recv "<<taddNum<<" addNodes from " <<i<<endl;
			#endif
			if(taddNum<=0){
				continue;
			}
			MPI_Recv(taddbuf,sizeof(AddNode)*taddNum,MPI_BYTE,i,i,children,MPI_STATUS_IGNORE);
			//将 addbuf[cura] 和 taddbuf 归并到 addbuf[nexta]
			int naddNum=0;
			int curhead=0,thead=0;
			while(curhead<addNum&&thead<taddNum){
				if(!cmp(curabuf[curhead],taddbuf[thead])){//cur <= tcur
					nextabuf[naddNum++]=curabuf[curhead++];
					if(!cmp(taddbuf[thead],curabuf[curhead])){//tcur >= cur   --> tcur==cur
						nextabuf[naddNum].reward+=taddbuf[thead].reward;
						nextabuf[naddNum].totalCount+=taddbuf[thead].totalCount;
						thead++;
					}
				}else{
					nextabuf[naddNum++]=taddbuf[thead++];
				}
			}
			while(curhead<addNum){
				nextabuf[naddNum++]=curabuf[curhead++];
			}
			while(thead<taddNum){
				nextabuf[naddNum++]=taddbuf[thead++];
			}
			addNum=naddNum;
			swap(curabuf,nextabuf);
		}
		#ifdef DEBUG
		cout<<"main: Total "<<addNum<<" addNodes"<<endl;
		#endif
		for(int i=0;i<addNum;i++){
			//cout<<"main : "<<layer<<" father "<<curabuf[i].father<<endl;
			//cout<<"main : "<<layer<<" locX "<<curabuf[i].locX<<endl;
			//cout<<"main : "<<layer<<" locY "<<curabuf[i].locY<<endl;
			//cout<<"main : "<<layer<<" result "<<curabuf[i].result<<endl;
			//cout<<"main : "<<layer<<" reward "<<curabuf[i].reward<<endl;
			//cout<<"main : "<<layer<<" totalCount "<<curabuf[i].totalCount<<endl;
			++layer;

			int fi=curabuf[i].father;
			father[layer]=fi;
			if(firstChild[fi]==-1){
				firstChild[fi]=layer;
			}else{
				firstChild[fi]=min(layer,firstChild[fi]);
			}
			lastChild[fi]=max(layer,firstChild[fi]);
			user[layer]=!user[fi];
			
			firstChild[layer]=-1;

			locX[layer]=curabuf[i].locX;
			locY[layer]=curabuf[i].locY;
			result[layer]=curabuf[i].result;
			reward[layer]=curabuf[i].reward;
			totalCount[layer]=curabuf[i].totalCount;
		}
		MPI_Bcast(&layer,1,MPI_INT,0,everyone);
		MPI_Bcast(father+lastlayer+1,layer-lastlayer,MPI_INT,0,everyone);
		MPI_Bcast(locX+lastlayer+1,layer-lastlayer,MPI_INT,0,everyone);
		MPI_Bcast(locY+lastlayer+1,layer-lastlayer,MPI_INT,0,everyone);
		MPI_Bcast(result+lastlayer+1,layer-lastlayer,MPI_INT,0,everyone);
		MPI_Bcast(reward+lastlayer+1,layer-lastlayer,MPI_INT,0,everyone);
		MPI_Bcast(totalCount+lastlayer+1,layer-lastlayer,MPI_INT,0,everyone);
		for(int i=0;i<THREADSNUM;i++){
			int tupdNum;
			MPI_Recv(&tupdNum,1,MPI_INT,i,i,children,MPI_STATUS_IGNORE);
			#ifdef DEBUG
			if(i==0)
				cout<<"main: Recv "<<tupdNum<<" updNodes from " <<i<<endl;
			#endif
			if(tupdNum<=0){
				continue;
			}
			MPI_Recv(updbuf,sizeof(UpdateNode)*tupdNum,MPI_BYTE,i,i,children,MPI_STATUS_IGNORE);
			for(int j=0;j<tupdNum;j++){
				int st=updbuf[j].state;
				if(updated[st]!=timestamp){
					updated[st]=timestamp;
					rawreward[st]=reward[st];
					rawtotalCount[st]=totalCount[st];
				}
				reward[st]+=updbuf[j].reward-rawreward[st];
				totalCount[st]+=updbuf[j].totalCount-rawtotalCount[st];
			}
		}
		int updNum=0;
		for(int i=0;i<=lastlayer;i++){
			if(updated[i]==timestamp){
				updbuf[updNum].state=i;
				updbuf[updNum].reward=reward[i];
				updbuf[updNum].totalCount=totalCount[i];
				++updNum;
			}
		}
		#ifdef DEBUG
		cout<<"main: Total "<<updNum<<" updNodes"<<endl;
		#endif
		MPI_Bcast(&updNum,1,MPI_INT,0,everyone);
		MPI_Bcast(updbuf,sizeof(UpdateNode)*updNum,MPI_BYTE,0,everyone);
		
		// for(int i = 0; i < boardRow; i++)	 //copy board
		// 	for( int j = 0; j<boardCol; j++)
		// 		board1[i][j] = board[i][j];
		// for(int i = 0; i < boardCol; i++)
		// 	top1[i] = top[i];

		// int t = 0;							//back to root

		// //tree policy starts
		// while(firstChild[t] !=-1)						
		// {
		// 	t = BestChildNode(t,c);
		// 	putLoc(locX[t],locY[t],user[t]);
		// }

		// if(totalCount[t] ==0)				//first encountered node, check result
		// {
		// 	result[t] = checkResult(locX[t],locY[t],user[t]);
		// }

		// if(result[t] != -2)				// the game is end, either win, lose or tie, do backup
		// {
		// 	int temp = result[t];
		// 	backUp(std::make_pair(temp , t));
		// 	continue;
		// }
//		if(t>lastlayer) //等待更新
		// backUp(DefaultPolicy(t)); // ==-2
	}
	timestamp=-1;
	MPI_Bcast(&timestamp,1,MPI_INT,0,everyone);
	cout<<"main: "<<iterations<<" times "<<layer<<" layers"<<endl;

	int bestNode = BestChildNode(0,0);

	//====================================================================================UCT SEARCH ends
	x = locX[bestNode];
	y = locY[bestNode];

	MPI_Comm_disconnect(&children);//需要disconnect 不然子进程不会关闭
	MPI_Comm_disconnect(&everyone);

	clearArray(M,N,board1);
	clearArray(M, N, board);
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
	清除top和board数组
*/
void clearArray(int M, int N, int** board){
	for(int i = 0; i < M; i++){
		delete[] board[i];
	}
	delete[] board;
}

int BestChildNode(int t, double inputC)
{
	double compare =-100000;
	double UCB;
	int ans = 1;
	//cout<<layer<<endl;
	for( int i = firstChild[t]; i <= lastChild[t]; i++)
	{
		if( user[t] == 0 )
			UCB = (double)reward[i] / ((double)totalCount[i] + err) + inputC * sqrt(2 * log((double)totalCount[father[i]] + 1) / ((double)totalCount[i] + err));
		else
			UCB = -(double)reward[i] / ((double)totalCount[i] + err) + inputC * sqrt(2 * log((double)totalCount[father[i]] + 1) / ((double)totalCount[i] + err));
		//cout<<"main: "<<i<<" "<<UCB<<endl;
		if( UCB > compare )
		{
			compare = UCB;
			ans = i;
		}
	}
	return ans;
}

// void putLoc(int x, int y, bool u)
// {
// 	board1[x][y] = u + 1;
// 	top1[y]--;
// 	if( x == _noX + 1 && y == _noY )
// 		top1[y]--;
// }
// int checkResult(int x, int y, bool u)
// {
// 	if ( u == 0 && userWin(x,y,boardRow,boardCol,board1) )
// 		return -1;
// 	if ( u ==1 && machineWin(x,y,boardRow,boardCol,board1) )
// 		return 1;
// 	if ( isTie(boardCol,top1) )
// 		return 0;
// 	return -2; // hasn't ended
// }
void boardInitialization(int M, int N, int noX, int noY)
{
	boardRow = M;
	boardCol = N;
	_noX = noX;
	_noY = noY;
	layer = 0;
	firstChild[0] = -1;
	user[0] =0;
	result[0] = -2;
}

// std::pair<int, int> DefaultPolicy(int state)
// {

// 	//open all available nodes
// 	firstChild[state] = layer + 1;
// 	for(int i = 0; i < boardCol; i++)// TODO SEND 
// 	{
// 		if(top1[i] > 0)
// 		{
// 			firstChild[++layer] = -1;
// 			reward[layer] = totalCount[layer] = 0;
// 			father[layer] = state;
// 			user[layer] = !user[state];              //different from the last step
// 			locX[layer] = top1[i] - 1;
// 			locY[layer] = i;
// 			result[layer] = -2;						//mark that its not done
// 		}
// 	}
// 	lastChild[state] = layer;
// 	int k = rand() % (lastChild[state] - firstChild[state] + 1) + firstChild[state]; //randomly select a point to put
// 	putLoc(locX[k],locY[k],user[k]);
// 	int delta = checkResult(locX[k],locY[k],user[k]);
// 	result[k]= delta;
// 	bool u = !user[k];

// 	while(delta ==-2)                            //play until the end
// 	{
// 		int y1 = rand() % boardCol, x1;
// 		while(top1[y1] ==0)
// 			y1 = rand() % boardCol;
// 		x1 = top1[y1] - 1;
// 		putLoc(x1,y1,u);

// 		delta = checkResult(x1,y1,u);
// 		u = !u;
// 	}
	
// 	state = k;  // k becomes the current node

// 	return std::make_pair(delta , state ); 
// }

// int backUp(pair<int, int> input) // TODO SEND 
// {
// 	int rewardInput = input.first;
// 	int t = input.second;
// 	while(t != 0)                             //do backup
// 	{
// 		totalCount[t]++;
// 		reward[t]+= rewardInput;
// 		t = father[t];
// 	}
// 	totalCount[0]++;
// 	return t;
// }
