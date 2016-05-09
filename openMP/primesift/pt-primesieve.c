#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <math.h>


typedef struct _node{
	int chunk_low;//low index of chunk
	int chunk_high;//top index of chunk
	int k;//num to sift multiples of
	int new;//flag that worker watches for new work.
	int* marked;//array to store 'marked' values (was a multiple of a given k)
	pthread_cond_t consu_wait;
	struct _node* next;
}node_t;

typedef struct queue_t{
	pthread_mutex_t mutex_queue;
	pthread_cond_t produ_wait;
	int n;
	int chunk_size;
	int threads;
	int* elements;
	node_t* front;
	node_t* rear;
}queue_t;

void* consumeSift(void*);
void* produceSift(void*);
pthread_t* spawnWorkers(queue_t*, int);
void joinWorkers(pthread_t*, int);
queue_t* queueInit(int, int*);
void enqueue(queue_t*, node_t*);
node_t* dequeue(queue_t*);

/* Spawn all the worker threads needed */
pthread_t* spawnWorkers(queue_t* queue, int threads){
		int i = 0;
		pthread_t* arr_tid = malloc(sizeof(pthread_t) * threads);
		for(i = 0; i < threads; i++){
			pthread_create(&arr_tid[i], NULL, consumeSift, (void*) queue);
		}
		return arr_tid;
}

/* Not particularly needed, but joins all the worker threads, give array of threadIDs */
void joinWorkers(pthread_t* tid, int threads){
	int i = 0;
	for(i = 0; i < threads; i++){
		pthread_join(tid[i], NULL);
	}
}


/* Init. the queue struct. */
queue_t* queueInit(int c, int* elements){
	queue_t* queue;
	
	queue = malloc(sizeof(queue_t));
	if(!queue){
		fprintf(stderr, "queue allocation failed\n");
		return 0;
	}
	

	queue->front = queue->rear = NULL;
	queue->chunk_size = c;
	queue->elements = elements;
	
	pthread_mutex_init(&queue->mutex_queue, NULL);
	pthread_cond_init(&queue->produ_wait, NULL);
	
	return queue;
}

/* Add a new item to the queue. */
void enqueue(queue_t* queue, node_t* newNodeChunk){
	
	newNodeChunk->next = NULL;
	if(queue->rear == NULL && queue->front == NULL){//empty queue
		queue->front = queue->rear = newNodeChunk;
		return;
	}

	queue->rear->next = newNodeChunk;
	queue->rear = newNodeChunk;
	return;
}

/* Remove a single item from the queue */
node_t* dequeue(queue_t* queue){
	 node_t* chunk = queue->front;
	 
	 if(queue->front == queue->rear){
		queue->front = queue->rear = NULL; //queue is empty now
	 } else {
		queue->front = queue->front->next;
	 }
	 
	 return chunk;
}
/* 
Manages the producer thread for prime sifting.
Distributes chunks to worker threads, and a multiple to mark by.
Takes the workers results and compiles them into primes[] for printing.

Waits for the front of the queue to not be NULL before distributing work.
 */
void* produceSift(void* arg){
	int i = 0;
	int j = 0;
	int k = 0;
	int newlow = 0;
	int newhigh = 0;
	queue_t* queue = (queue_t*)arg;
	int* primes = queue->elements;

	
	for(k = 2; k < sqrt(queue->n); k++){
		for(i = 2; i < queue->n; i += queue->chunk_size){
		
			/* determine new low/high index of the next chunk to hand out*/
			newlow = i;
			newhigh = i + queue->chunk_size;
			if(newhigh > queue->n){
				newhigh = queue->n;
			}
			
			//check if there are threads waiting in the queue
			pthread_mutex_lock(&queue->mutex_queue);
			while(queue->front == NULL){//wait for a thread to start waiting
				pthread_cond_wait(&queue->produ_wait, &queue->mutex_queue);
			}
			node_t* chunk = dequeue(queue);//remove it from the queue, grab it's chunk for reassignment
			for(j = 0; j < queue->chunk_size; j++){ //read it's marked array, and adjust primes[] accordingly
				if(chunk->marked[j] != 0)
					primes[chunk->marked[j]] = 0;
			}
			/* update it's chunk info */
			chunk->k = k;
			chunk->new = 1;
			chunk->chunk_low = newlow;
			chunk->chunk_high = newhigh;
			
			pthread_cond_signal(&chunk->consu_wait);//signal and unlock the mutex
			pthread_mutex_unlock(&queue->mutex_queue);

		}

		
	}
	//sifting done, go through queue and send 'kill' signal: new = -1
	while(queue->front != NULL){
		node_t* chunk = dequeue(queue);
		chunk->new = -1;
		pthread_cond_signal(&chunk->consu_wait);
		
	}
	
	/* Print results. */
	printf("Primes: ");
	for(i = 0; i < queue->n; i++){
		if(primes[i] != 0)
			printf("%d ", primes[i]);
		
	}
	if(primes[ (queue->n) - 2])
		printf("\n[%d] is a prime.\n", primes[ (queue->n) - 2]);
	else
		printf("\n[%d] is not a prime.\n", queue->n);

	exit(0);
	
}
/* 
Manages worker threads.

Recieves a given K and range of indeces [low, high]
Then determines if they are multiples of said K.
If so, save the index of that now 'marked value' in the marked array for later processing by master.
 */
void* consumeSift(void* arg){
	int i, j;
	queue_t* queue = (queue_t*)arg;
	node_t* chunk = malloc(sizeof(node_t));
	int* primes = queue->elements;
	int* marked = chunk->marked = malloc(sizeof(int) * queue->chunk_size);
	chunk->new = 0;
	chunk->next = NULL;
	pthread_cond_init(&chunk->consu_wait, NULL);


		
	while(queue->n){
		//insert self into queue - ready for more work!
		pthread_mutex_lock(&queue->mutex_queue);
			enqueue(queue, chunk);
			pthread_cond_signal(&queue->produ_wait);//poke producer for work if it's waiting
			while(!(chunk->new) && chunk->new != -1){ //while there is no new work in the chunk (-1 is kill)
				pthread_cond_wait(&chunk->consu_wait, &queue->mutex_queue);
			}
			
		pthread_mutex_unlock(&queue->mutex_queue);
		
		
		//i have work to do now!
		if(chunk->new != -1){
			chunk->new = 0;//mark chunk as read
			memset(marked, 0, sizeof(marked));
			for(i = chunk->chunk_low, j = 0; i < chunk->chunk_high; i++){
				if(primes[i] == chunk->k)
					continue;
				if(primes[i] == 0)
					continue;
				if((primes[i] % chunk->k) == 0){
					marked[j++] = i;
					continue;
				}
			}
		}
	}
}

int main(int argc, char* argv[]){
	int i = 0, j = 0;
	int n, p, c;
	int opt;
	int* elements;
	pthread_t master;
	queue_t* queue;

	while ((opt = getopt(argc, argv, "n:p:c:v")) != -1){
		switch (opt){
			case 'n'://num elements
				n = atoi(optarg);
				break;
			case 'p'://num threads
				p = atoi(optarg);
				break; 
			case 'c'://chunk size
				c = atoi(optarg);
				break; 
			case '?':
				fprintf(stderr, "-%c requires an argument.\n", optopt);
				break;
			default:
				fprintf(stderr, "requires arguments -n -p -c\n");
				return -1;
		}
	}
	
	elements = malloc(sizeof(int) * n-2);
	
	
	for(i = 0, j = 2; j <= n; i++, j++){
		elements[i] = j;
	}
	queue = queueInit(c, elements);
	queue->n = n;

	pthread_create(&master, NULL, produceSift, (void*)queue);
	pthread_t* tids = spawnWorkers(queue, p);

	
	pthread_join(master, NULL);
	joinWorkers(tids, p);
	
	return 0;
}