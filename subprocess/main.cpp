#include <iostream> 
#include <mpi.h>
#include <cstdio>

using namespace std;
int** board1;
int top1[12],top[12];//todo alloc
void clearArray(int M, int N, int** board);
void printBoard(int M, int N, int** board);
int main(int argc, char* argv[]) 
{ 
	MPI_Init(&argc, &argv); 
	int rank,size; 
	MPI_Comm parent,everyone;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank); 
	MPI_Comm_size(MPI_COMM_WORLD, &size); 
	MPI_Comm_get_parent(&parent);
	MPI_Intercomm_merge(parent,1,&everyone);

	//if (0 == rank) for test
	//{ 
	//	int SendNum = 16; 
	//	MPI_Send(&SendNum, 1, MPI_INT, 1, 0, MPI_COMM_WORLD); 
	//} 
	//else if (1 == rank) 
	//{ 
	//	int RecvNum = 0; 
	//	MPI_Recv(&RecvNum, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE); 
	//	std::cout << "Receive: " << RecvNum << std::endl;
	//}
	int M,N;
	MPI_Bcast(&M,1,MPI_INT,0,everyone);
	MPI_Bcast(&N,1,MPI_INT,0,everyone);
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



	//iteration x times


	//	MPI_Recv(&recv, 1, MPI_INT, 0 , size, parent, MPI_STATUS_IGNORE); 


	delete boardbuf;
	clearArray(M,N,board1);	
	clearArray(M,N,board);
	fflush(stdout);
	MPI_Finalize();
	return 0; 
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
