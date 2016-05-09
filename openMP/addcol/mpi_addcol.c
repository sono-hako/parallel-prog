#include <mpi.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
/*
	int x 	 : value to send
	int root : rank of process to recieve
	
	example: 
	add_all(rank, [rank of process to recieve the sum])
			root gets result (sum of the ranks)
			everything else gets 0

	works for any process amount (n)

	Used binomal tree to represent this
		For any process on each iteration, the  process i'll be sending to is always
				(rank - 2^n)
			The process i'll recieve from
				(rank + 2^n)

		But since this is a binomial tree, once the lowest order process sends its data
			it is no longer needed to do anything
			hence the two cases
				1) rank % (2^(n+1)) == 2^n, then i'll be sending on this iteration
				2) rank % (2^(n+1)) == 0, i will be recieveing data on this iteration
*/
int add_all(int x, int root){
	int rank, procs;
	int i;
	int recvd;
	int sendto, recvfrom;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &procs);

	if(procs == 1)
		return x;
	for(i = 0; i <= (int)floor(log2(procs)); i++){
		
		//Sending Case
		if( rank % (1<<(i+1)) == 1<<i ){
			sendto = rank - (1 << i);//create rank of process i'll send to
			MPI_Send(&x, 1, MPI_INT, sendto, 0, MPI_COMM_WORLD);
		}

		//Receiving Case
		if( (rank % (1<<(i+1)) == 0) && (rank+(1<<i) < procs) ){
			recvfrom = rank + (1 << i);//create rank of process i'll receive from
			MPI_Recv(&recvd, 1, MPI_INT, recvfrom, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			x += recvd;//add to partial sum
		}
	}
	/*collect to 0, 0 then sends to specifed root*/
	if (rank == 0){
		if(rank != root){
			MPI_Send(&x, 1, MPI_INT, root, 0, MPI_COMM_WORLD);
			return 0;
		}
		return x;
	}
	if (rank == root){
		if(rank != 0){
			MPI_Recv(&x, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			return x;
		}
	}
	return 0;
}

/*
	char* sendbuf  : buffer where items are stored
	int sendcnt    : amount of items sent
	char** recvbuf : array of pointers to recieved array
	int* recv_cnt   : array holding the size of each recieved array

	recv_cnt [1] = amount of data recieved from process 1
	recvbuf [1][] =  the actual data recieved from p1, where length == recv_cnt[1]

	ONLY WORKS WITH PROCESS AMOUNTS OF 2^N

	Basic idea of what i'm doing here, represent each process in their binary form
		0->0
		5->101
		..
		Where each bit represents a new dimension to trade values with
		to find out who is the next neighbor to trade with XOR with the current iteration number

		Ideally i'd want to do the equivilant of set union for the data trade between pairs, and get O(log(n)) runtime
*/
void collect_all(char* sendbuf, int sendcnt, char*** recvbuf, int** recv_cnt){
	int rank, procs;
	int i;
	MPI_Comm_size(MPI_COMM_WORLD, &procs);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	*recvbuf = malloc(procs * sizeof(char*));
	(*recvbuf)[rank] = malloc(sendcnt * sizeof(char));
	(*recvbuf)[rank] = sendbuf;//put original send data into this process's recv buf

	*recv_cnt = malloc(procs * sizeof(int));
	
	for(i = 0; i < (int)floor(log2(procs)); i++){
		int pair = rank ^ (1 << i);//find trade partner on this turn (rank XOR 2^n)
		//get the amount of data it will be sending me
		MPI_Sendrecv(&sendcnt, 1, MPI_INT, pair, 1,
			&(*recv_cnt)[pair], 1, MPI_INT, pair, 1,
			MPI_COMM_WORLD, MPI_STATUS_IGNORE);

		(*recvbuf)[pair] = malloc((*recv_cnt)[pair] * sizeof(char)); //allocate space for that amount

		//trade rows
		MPI_Sendrecv(sendbuf, sendcnt, MPI_CHAR, pair, 2,
					(*recvbuf)[pair], (*recv_cnt)[pair], MPI_CHAR, pair, 2,
					MPI_COMM_WORLD, MPI_STATUS_IGNORE);

		int j = 1;
		while(j <= (1<<i)){
			//allocate memory for the incoming row
			MPI_Sendrecv(&(*recv_cnt)[rank ^ j], 1, MPI_INT, pair, 3,
						&(*recv_cnt)[pair ^ j], 1, MPI_INT, pair, 3,
						MPI_COMM_WORLD, MPI_STATUS_IGNORE);

			(*recvbuf)[pair ^ j] = malloc((*recv_cnt)[pair ^ j] * sizeof(char));//allocate space for row

			//trade rows
			MPI_Sendrecv((*recvbuf)[rank ^ j], (*recv_cnt)[rank ^ j], MPI_CHAR, pair, 4,
						(*recvbuf)[pair ^ j], (*recv_cnt)[pair ^ j], MPI_CHAR, pair, 4,
						MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			j++;
		}	
	}
	return;
}

int main (int argc, char* argv[]){

	int procs, rank;
	int i;
	int sum = 0;
	int recvd;
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &procs);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	// Getting the sum of all the processes ranks 
	sum = add_all(rank, 1);
	printf("[%d]: add_all() sum is %d\n", rank, sum);

	/*collect_all ONLY works for processor amounts of 2^n*/
	if(!(procs & (procs - 1))){//only running this with powers of 2^n
		char** recvbuf;
		int* recv_cnt;
		char str[100];
		int size = sprintf(str, "[%d]'s data!", rank);

		collect_all(str, strlen(str) + 1, &recvbuf, &recv_cnt);
		if(rank == 0)
			for(i = 0; i < procs; i++)
				printf("[%d] has [%s] (sz=%d) \n", rank, recvbuf[i], recv_cnt[i]);
	}
	MPI_Finalize();


	return 0;
}