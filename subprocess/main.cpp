#include <iostream> 
#include <mpi.h>
#include <ctime>
#include <cstdio>
#include <algorithm>
#include <set>
#include <assert.h>
#include "Judge.h"
#define DEBUG
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
const int MAX_NODE=3000000;
const int BUF_SIZE=1000000;
const double c = 0.6, err = 0.000001; //err 用于平移，以免出现0/0的情况
int** board1;
int _noX, _noY, boardRow, boardCol; //main parameters of the chess board
int top1[12],top[12];//todo alloc
int layer,firstChild[MAX_NODE],lastChild[MAX_NODE],father[MAX_NODE]; //to indicate the relation of the mentioned node
int reward[MAX_NODE],totalCount[MAX_NODE],locX[MAX_NODE],locY[MAX_NODE],result[MAX_NODE]; //all the variables related to the node
bool user[MAX_NODE]; 

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
	
	int loc;
	MPI_Recv(&loc,1,MPI_INT,0,rank,parent,MPI_STATUSES_IGNORE);

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

	if(rank==0){
		printBoard(M,N,board);
	}
	#ifdef DEBUG
		printf("%d loc:%d %d\n",rank,top[loc]-1,loc);
		fflush(stdout);
	#endif

	srand(time(NULL)*(rank+1)); //分配不同的随机数种子
	layer = 0;
	firstChild[0] = -1;
	user[0] =0; 
	result[0] = -2;

	clock_t start = std::clock();
	int its=0;
	while ((double)(std::clock() - start) / CLOCKS_PER_SEC < 2.3 && its < MAX_ITERATIONS){
		int iterations=0;
		while (iterations++ < 10000)
		{
			for(int i = 0; i < boardRow; i++)	 //copy board: TODO OPT
				for( int j = 0; j<boardCol; j++) 
					board1[i][j] = board[i][j];
			for(int i = 0; i < boardCol; i++)
				top1[i] = top[i];

			int t = 0;							//back to root
			//if(iterations%1000==0){
			//	cout<<rank<<": "<<"time stamp "<<timestamp<<" iterations "<<iterations<<endl;
			//}
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
			backUp(DefaultPolicy(t)); // ==-2
		}
		its+=iterations;
		fflush(stdout);
	}
	#ifdef STAT
	cout<<rank<<": "<<its<<" times "<<layer<<" layers"<<endl;
	#endif

	int num1=lastChild[0];
	MPI_Send(reward+1, num1, MPI_INT, 0 ,rank, parent);
	MPI_Send(total+1, num1, MPI_INT, 0 ,rank, parent);


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
			//cout<<"father["<<layer<<"]="<<state<<endl;
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
	int ans = 1;
	for( int i = firstChild[t]; i <= lastChild[t]; i++)
	{
		if( user[t] == 0 )
			UCB = (double)reward[i] / ((double)totalCount[i] + err) + inputC * sqrt(2 * log((double)totalCount[father[i]] + 1) / ((double)totalCount[i] + err));
		else
			UCB = -(double)reward[i] / ((double)totalCount[i] + err) + inputC * sqrt(2 * log((double)totalCount[father[i]] + 1) / ((double)totalCount[i] + err));
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
	while(t != 0) //do backup
	{
		totalCount[t]++;
		reward[t]+= rewardInput;
		t = father[t];
	}
	reward[0]+=rewardInput;
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
