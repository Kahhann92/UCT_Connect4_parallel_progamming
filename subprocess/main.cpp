#include <iostream> 
#include <mpi.h>
#include <ctime>
#include <cstdio>
#include <algorithm>
#include <set>
#include "Judge.h"

using namespace std;
const int MAX_NODE=10000000;
const int BUF_SIZE=1000000;
const double c = 0.6625, err = 0.000001; //err 用于平移，以免出现0/0的情况
int** board1;
int _noX, _noY, boardRow, boardCol; //main parameters of the chess board
int top1[12],top[12];//todo alloc
int layer,firstChild[MAX_NODE],lastChild[MAX_NODE],father[MAX_NODE]; //to indicate the relation of the mentioned node
int reward[MAX_NODE],totalCount[MAX_NODE],locX[MAX_NODE],locY[MAX_NODE],result[MAX_NODE]; //all the variables related to the node
bool user[MAX_NODE];   //player 可通过父节点的state推断，不用发送
int lastlayer;//与主进程最后1次同步时的节点数, 之后均为新增的节点, 需要发送给主进程
//新增的节点
//  发送格式为 FATHER|LOCX|LOCY|RESULT|REWARD|TOTALCOUNT， FATHER要求必须为主进程拥有的state，为了方便更新
//	子进程发送的更新列表需要按照FATHER排序，保证同一个FATHER的子节点连续，方便主进程合并father相同的新增节点
//  主进程合并后广播新增的state数量,之后按照father，locX，locY，result，reward，totalCount的顺序广播6个数组的新增部分
//	各进程根据这些更新firstChild,lastchild,user


//更新的节点
//  发送格式为 STATE|REWARD|TOTALCOUNT，子进程发送到主进程，合并再由主进程广播
//  主进程合并后广播修改的state数量，之后同样按STATE|REWARD|TOTALCOUNT ，广播到各子进程

struct AddNode{
	int father,locX,locY,result,reward,totalCount;
}addbuf[BUF_SIZE];
struct UpdateNode{
	int state,reward,totalCount;
}updbuf[BUF_SIZE];
int timestamp=1;
int updated[MAX_NODE];//用时间戳判断是否更新

bool cmp(AddNode&a, AddNode& b){ //比较函数
	if(a.father!=b.father)
		return a.father<b.father;
	if(a.locX!=b.locX)
		return a.locX<b.locX;
	if(a.locY!=b.locY)
		return a.locY<b.locY;
	return 0;
}

void clearArray(int M, int N, int** board);
void printBoard(int M, int N, int** board);
int checkResult(int x, int y, bool u);
void putLoc(int x, int y, bool u);
int BestChildNode(int t, double inputC);

std::pair<int, int> DefaultPolicy(int state);
int backUp(std::pair<int, int> input);

int main(int argc, char* argv[]) 
{ 

	MPI_Init(&argc, &argv); 
	int rank,size; 
	MPI_Comm parent,everyone;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank); 
	MPI_Comm_size(MPI_COMM_WORLD, &size); 
	MPI_Comm_get_parent(&parent);
	MPI_Intercomm_merge(parent,1,&everyone);

	int M,N;
	MPI_Bcast(&M,1,MPI_INT,0,everyone);
	MPI_Bcast(&N,1,MPI_INT,0,everyone);
	boardRow = M;
	boardCol = N;
	MPI_Bcast(&_noX,1,MPI_INT,0,everyone);
	MPI_Bcast(&_noY,1,MPI_INT,0,everyone);

	int esize;
	MPI_Comm_size(everyone, &esize); 
	if(rank==0){//for debug
		printf("recv from parent:%d %d, id=%d, size=%d, everyonesize=%d\n",M,N,rank,size,esize);
		fflush(stdout);
	}

	MPI_Datatype linetype,boardtype;
	MPI_Type_contiguous(N,MPI_INT,&linetype);// one line (1xN) 
	MPI_Type_contiguous(M,linetype,&boardtype);// M line (MxN)
	MPI_Type_commit(&linetype);
	MPI_Type_commit(&boardtype);

	int *boardbuf=new int[M*N*2];//使这段内存连续,以用来传输
	board1 = new int*[M];	
	int **board = new int*[M];
	for(int i = 0; i < M; i++){
		board[i] = boardbuf+i*N;
		board1[i] = boardbuf+(M+i)*N;
	}

	MPI_Bcast((void*)top,1,linetype,0,everyone);
	MPI_Bcast((void*)boardbuf,1,boardtype,0,everyone);

	if(rank==0){
		printBoard(M,N,board);
	}

	srand(time(NULL)*(rank+1)); //分配不同的随机数种子
	layer = 0;
	firstChild[0] = -1;
	user[0] =0;
	result[0] = -2;

	while(true){
		MPI_Bcast(&timestamp,1,MPI_INT,0,everyone);
		if(timestamp==-1){// 主进程发出停机信号
			break;
		}
		int iterations=0;
		while (iterations++ < 100)
		{
			for(int i = 0; i < boardRow; i++)	 //copy board: TODO OPT
				for( int j = 0; j<boardCol; j++) 
					board1[i][j] = board[i][j];
			for(int i = 0; i < boardCol; i++)
				top1[i] = top[i];

			int t = 0;							//back to root

			//tree policy starts
			while(firstChild[t] !=-1)						
			{
				t = BestChildNode(t,c);
				putLoc(locX[t],locY[t],user[t]);
			}

			if(totalCount[t] ==0)				//first encountered node, check result
			{
				result[t] = checkResult(locX[t],locY[t],user[t]);
			}

			if(result[t] != -2)				// the game is end, either win, lose or tie, do backup
			{
				int temp = result[t];
				backUp(std::make_pair(temp , t));
				continue;
			}
			if(t>lastlayer){ //要扩展的节点为新增节点
				break;//todo: or continue
			}//等待更新

			backUp(DefaultPolicy(t)); // ==-2
		}
		MPI_Barrier(everyone);
		/*
			added values
		*/
		int addNum=layer-lastlayer;
		MPI_Send(&addNum,1,MPI_INT,0,rank,everyone);
		if(addNum>0){
			for(int i=lastlayer+1;i<=layer;i++){
				addbuf[i-lastlayer-1].father=father[i];
				addbuf[i-lastlayer-1].locX=locX[i];
				addbuf[i-lastlayer-1].locY=locY[i];
				addbuf[i-lastlayer-1].result=result[i];
				addbuf[i-lastlayer-1].reward=reward[i];
				addbuf[i-lastlayer-1].totalCount=totalCount[i];
			}
			sort(addbuf,addbuf+addNum,cmp);
			MPI_Send(addbuf,sizeof(AddNode)*addNum,MPI_BYTE,0,rank,everyone);
			MPI_Bcast(&layer,1,MPI_INT,0,everyone);
			MPI_Bcast(father+lastlayer+1,layer-lastlayer,MPI_INT,0,everyone);
			MPI_Bcast(locX+lastlayer+1,layer-lastlayer,MPI_INT,0,everyone);
			MPI_Bcast(locY+lastlayer+1,layer-lastlayer,MPI_INT,0,everyone);
			MPI_Bcast(result+lastlayer+1,layer-lastlayer,MPI_INT,0,everyone);
			MPI_Bcast(reward+lastlayer+1,layer-lastlayer,MPI_INT,0,everyone);
			MPI_Bcast(totalCount+lastlayer+1,layer-lastlayer,MPI_INT,0,everyone);
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
		MPI_Send(&updNum,1,MPI_INT,0,rank,everyone);
		if(updNum>0){
			MPI_Send(updbuf,sizeof(UpdateNode)*updNum,MPI_BYTE,0,rank,everyone);
		}
		MPI_Bcast(&updNum,1,MPI_INT,0,everyone);
		MPI_Bcast(updbuf,sizeof(UpdateNode)*updNum,MPI_BYTE,0,everyone);
		
		lastlayer=layer;
		timestamp++;

	}
	//iteration x times
	//while(true){
	//	
	//}

	//	MPI_Recv(&recv, 1, MPI_INT, 0 , size, parent, MPI_STATUS_IGNORE); 


	delete boardbuf;
	clearArray(M,N,board1);	
	clearArray(M,N,board);
	MPI_Finalize();
	return 0; 
}

std::pair<int, int> DefaultPolicy(int state)
{

	//open all available nodes
	firstChild[state] = layer + 1;
	for(int i = 0; i < boardCol; i++)
	{
		if(top1[i] > 0)
		{
			firstChild[++layer] = -1;
			reward[layer] = totalCount[layer] = 0;
			father[layer] = state;
			user[layer] = !user[state];              //different from the last step
			locX[layer] = top1[i] - 1;
			locY[layer] = i;
			result[layer] = -2;						//mark not done
		}
	}
	lastChild[state] = layer;
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

int backUp(pair<int, int> input)
{
	int rewardInput = input.first;
	int t = input.second;
	while(t != 0) //do backup
	{
		updated[t]=timestamp;
		totalCount[t]++;
		reward[t]+= rewardInput;
		t = father[t];
	}
	updated[0]=timestamp;
	totalCount[0]++;
	return t;
}

int checkResult(int x, int y, bool u)
{
	if ( u == 0 && userWin(x,y,boardRow,boardCol,board1) )
		return -1;
	if ( u ==1 && machineWin(x,y,boardRow,boardCol,board1) )
		return 1;
	if ( isTie(boardCol,top1) )
		return 0;
	return -2; // hasn't ended
}
/*
	清除board数组
*/
void clearArray(int M, int N, int** board){
	delete[] board;
}

void printBoard(int M,int N,int** board){
	for(int i = 0; i < M; i++){
		for(int j = 0; j < N; j++){
			if(board[i][j] == 0){
					cout << ". ";
			}
			else if(board[i][j] == 2){
				cout << "A ";
			}
			else if(board[i][j] == 1){
				cout << "B ";
			}
		}
		cout << endl;
	}
	return;
}
