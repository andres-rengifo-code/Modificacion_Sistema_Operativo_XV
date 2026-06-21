#define NPROC        64  // maximum number of processes 
#define KSTACKSIZE 4096  // size of per-process kernel stack
#define NCPU          8  // maximum number of CPUs
#define NOFILE       16  // open files per process
#define NFILE       100  // open files per system
#define NINODE       50  // maximum number of active i-nodes
#define NDEV         10  // maximum major device number
#define ROOTDEV       1  // device number of file system root disk
#define MAXARG       32  // max exec arguments
#define MAXOPBLOCKS  10  // max # of blocks any FS op writes
#define LOGSIZE      (MAXOPBLOCKS*3)  // max data blocks in on-disk log
#define NBUF         (MAXOPBLOCKS*3)  // size of disk block cache
#define FSSIZE       1000  // size of file system in blocks
#define NUM_QUEUE      4   // numero de colas de MLFQ

//Tiempo de cada cola (QUANTUM) un tick corresponde a 10 ms 
// la cola 0 tiene un (QUANTUM) de 20 ms
// la cola 1 tiene un (QUANTUM) de 40 ms
// la cola 2 tiene un (QUANTUM) de 60 ms
// la cola 3 no tiene (QUANTUM) por que se maneja con FCFS
static int quantum_queue[NUM_QUEUE] = {2,4,6,0};
