#include <iostream> 
#include <mpi.h>
#include <ctime>
#include <cstdio>
#include <algorithm>
#include <set>
#include <assert.h>
#include <vector>
#include "Judge.h"
//#define DEBUG
#define STAT
#include <chrono>
#define TIMERSTART(tag)  auto tag##_start = std::chrono::steady_clock::now(),tag##_end = tag##_start
#define TIMEREND(tag)  tag##_end =  std::chrono::steady_clock::now()
#define DURATION_s(tag) printf("%s costs %d s\n",#tag,std::chrono::duration_cast<std::chrono::seconds>(tag##_end - tag##_start).count())
#define DURATION_ms(tag) printf("%s costs %d ms\n",#tag,std::chrono::duration_cast<std::chrono::milliseconds>(tag##_end - tag##_start).count());
#define DURATION_us(tag) printf("%s costs %d us\n",#tag,std::chrono::duration_cast<std::chrono::microseconds>(tag##_end - tag##_start).count());
#define DURATION_ns(tag) if(#tag,std::chrono::duration_cast<std::chrono::nanoseconds>(tag##_end - tag##_start).count())printf("rank:%d, %s costs %d ns\n",rank,#tag,std::chrono::duration_cast<std::chrono::nanoseconds>(tag##_end - tag##_start).count());


using namespace std;
const int MAX_ITERATIONS = 8000000;
const int MAX_NODE=4000000;
const int BUF_SIZE=1000000;
double c = 0.707, err = 0.000001; //err 用于平移，以免出现0/0的情况
int** board1;
int _noX, _noY, boardRow, boardCol; //main parameters of the chess board
int top1[12],top[12];//todo alloc
struct NODE{
	vector<int> child;
	int father;
	int locX,locY;
	int reward,totalCount,result;
	bool user;
	void init(){
		reward=totalCount=0;
		child.clear();
	}
}nodes[MAX_NODE];
int pool[MAX_NODE],pcnt;
int layer; 
inline int newnode(){
	int no;
	if(pcnt)
		no= pool[--pcnt];
	else
		no=layer++;
	nodes[no].init();
	return no;
}
inline void deletenode(int no){
	pool[pcnt++]=no;
	for(auto item:nodes[no].child){
		deletenode(item);
	}
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
	

	MPI_Recv(&c, 1, MPI_DOUBLE, 0 ,rank, parent,MPI_STATUSES_IGNORE);
	int M,N;
	MPI_Bcast(&M,1,MPI_INT,0,everyone);
	MPI_Bcast(&N,1,MPI_INT,0,everyone);
	boardRow = M;
	boardCol = N;
	MPI_Bcast(&_noX,1,MPI_INT,0,everyone);
	MPI_Bcast(&_noY,1,MPI_INT,0,everyone);

	int esize;
	MPI_Comm_size(everyone, &esize); 
	#ifdef DEBUG
	if(rank==0){//for debug
		printf("recv from parent:%d %d, id=%d, size=%d, everyonesize=%d\n",M,N,rank,size,esize);
		fflush(stdout);
	}
	#endif

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

	srand(time(NULL)*(rank+1)); //分配不同的随机数种子
	
	while(true){
		layer = 0;
		int root=newnode();
		nodes[root].father=-1;
		nodes[root].result=-2;
		nodes[root].user=0;
		if(rank==0){
			printBoard(M,N,board);
		}
		clock_t start = std::clock();
		int its=0;
		while ((double)(std::clock() - start) / CLOCKS_PER_SEC < 1.0 && its < MAX_ITERATIONS){
			int iterations=0;
			while (++iterations < 10)
			{
				for(int i = 0; i < boardRow; i++)	 //copy board: TODO OPT
					for( int j = 0; j<boardCol; j++) 
						board1[i][j] = board[i][j];
				for(int i = 0; i < boardCol; i++)
					top1[i] = top[i];

				int t = root;							//back to root
				//if(iterations%1000==0){
				//	cout<<rank<<": "<<"time stamp "<<timestamp<<" iterations "<<iterations<<endl;
				//}
				while(nodes[t].child.size() >0)	
				{
					t = BestChildNode(t,c);
					putLoc(nodes[t].locX,nodes[t].locY,nodes[t].user);
				//	cout<<rank<<": "<< t <<endl;
				}
				if(nodes[t].totalCount==0&&t!=root)				//first encountered node, check result
				{
					nodes[t].result = checkResult(nodes[t].locX,nodes[t].locY,nodes[t].user);
				}
				if(nodes[t].result != -2)				// the game is end, either win, lose or tie, do backup
				{
					backUp(std::make_pair(nodes[t].result , t));
					continue;
				}
				backUp(DefaultPolicy(t)); // ==-2
			}
			its+=iterations;
			#ifdef DEBUG
				if(its%1000==0){
					cout<<rank<<": "<<" iterations "<<its<<" "<<(double)(std::clock() - start) / CLOCKS_PER_SEC<<endl;
				}
			#endif
			fflush(stdout);
		}
		#ifdef STAT
		cout<<rank<<": "<<its<<" times "<<layer<<" layers "<<endl;
		#endif

		int num1=nodes[root].child.size();
		int treward[20],ttotal[20];
		for(int i=0;i<num1;i++){
			treward[i]=nodes[nodes[root].child[i]].reward;
			ttotal[i]=nodes[nodes[root].child[i]].totalCount;
		}
		MPI_Send(treward, num1, MPI_INT, 0 ,rank, parent);
		MPI_Send(ttotal, num1, MPI_INT, 0 ,rank, parent);
		int lastX,lastY,myX,myY;
		MPI_Bcast(&myX,1,MPI_INT,0,everyone);
		MPI_Bcast(&myY,1,MPI_INT,0,everyone);
		board[myX][myY] = 2;
 		top[myY]--;
 		if( myX == _noX + 1 && myY == _noY )
 			top[myY]--;
		MPI_Bcast(&lastX,1,MPI_INT,0,everyone);
		if(lastX==-1){//退出
			break;
		}
		MPI_Bcast(&lastY,1,MPI_INT,0,everyone);
 		board[lastX][lastY] = 1;
 		top[lastY]--;
 		if( lastX == _noX + 1 && lastY == _noY )
 			top[lastY]--;
	}

	delete boardbuf;
	clearArray(M,N,board1);	
	clearArray(M,N,board);
	MPI_Finalize();
	return 0; 
}

std::pair<int, int> DefaultPolicy(int state)
{

	for(int i = 0; i < boardCol; i++)
	{
		if(top1[i] > 0)
		{
			int no=newnode();
			nodes[state].child.push_back(no);
			nodes[no].locX=top1[i]-1;
			nodes[no].locY=i;
			nodes[no].user=!nodes[state].user;
			nodes[no].father=state;
			nodes[no].result=-2;
		}
	}
	int k = nodes[state].child[rand() %nodes[state].child.size()]; //randomly select a point to put
	putLoc(nodes[k].locX,nodes[k].locY,nodes[k].user);
	int delta = checkResult(nodes[k].locX,nodes[k].locY,nodes[k].user);
	nodes[k].result= delta;
	bool u = !nodes[k].user;

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

 void putLoc(int x, int y, bool u)
 {
 	board1[x][y] = u + 1;
 	top1[y]--;
 	if( x == _noX + 1 && y == _noY )
 		top1[y]--;
 }

 int BestChildNode(int t, double inputC)
{
	double compare =-100000;
	double UCB;
	int ans = -1;
	for( auto i :nodes[t].child)
	{
		if(nodes[t].user==0)
			UCB = (double)nodes[i].reward / ((double)nodes[i].totalCount + err) + inputC * sqrt(2 * log((double)nodes[t].totalCount) / ((double)nodes[i].totalCount + err));
		else
			UCB = -(double)nodes[i].reward / ((double)nodes[i].totalCount + err) + inputC * sqrt(2 * log((double)nodes[t].totalCount) / ((double)nodes[i].totalCount + err));
		if( UCB > compare )
		{
			compare = UCB;
			ans = i;
		}
	}
	return ans;
}

int backUp(pair<int, int> input)
{
	int rewardInput = input.first;
	int t = input.second;
	while(t != -1) //do backup
	{
		nodes[t].totalCount++;
		nodes[t].reward+= rewardInput;
		t = nodes[t].father;
	}
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
