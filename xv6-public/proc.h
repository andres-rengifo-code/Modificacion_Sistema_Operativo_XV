// Per-CPU state
struct cpu {
  uchar apicid;                // Local APIC ID
  struct context *scheduler;   // swtch() here to enter scheduler
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  volatile uint started;       // Has the CPU started?
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // Were interrupts enabled before pushcli?
  struct proc *proc;           // The process running on this cpu or null
};

extern struct cpu cpus[NCPU];
extern int ncpu;

//PAGEBREAK: 17
// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.

// ESTRUCTURA QUE GUARDA LOS REGISTROS DEL PROCESO CUANDO SALE DE LA CPU 
// (CONTEXT SWITCH), PARA QUE AL VOLVER A EJECUTARSE LA CPU LOS RESTAURE 
// Y EL PROCESO CONTINÚE DONDE SE QUEDÓ. swtch() ES QUIEN GUARDA Y 
// RESTAURA ESTOS VALORES.
struct context {
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
};

//ESTRUCTURA LA CUAL DEFINE LOS ESTADOS DE UN PROCESO
enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };


// Per-process state

// DEFINE LA ESTRUCTURA DE UN PROCESO
// CONTIENE TODOS LOS DATOS QUE EL SISTEMA NECESITA PARA ADMINISTRAR UN PROCESO
struct proc {
  uint sz;                     // Size of process memory (bytes)
  pde_t* pgdir;                // Page table
  char *kstack;                // Bottom of kernel stack for this process
  enum procstate state;        // DEFINE EN QUÉ ESTADO ESTÁ EL PROCESO (FUNDAMENTAL PARA QUE EL SCHEDULER DECIDA SI PUEDE CORRER)
  int pid;                     // Process ID
  struct proc *parent;         // Parent process
  struct trapframe *tf;        // Trap frame for current syscall
  struct context *context;     // GUARDA EL CONTEXTO DEL PROCESO (REGISTROS) PARA RETOMAR SU EJECUCIÓN DONDE QUEDÓ
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)

    // --- Campos para medicion del planificador (MEDICION) ---
  int t_created;          // Guarda el momento en el que un proceso fue creado sirve para depues calccular en que momento el proceso pudo entrar a la CPU (RESPONSE TIME RT)
  int t_first_run;        // Guarda el momento en el que el proceso corrio por primera ves en la CPU (RESPONSE TIME) (-1 = aun no corrio)
  int t_ready_start;      // Guarda desde cuando el proceso esta en RUNNABLE, para calcular cada periodo de espera puntual
  int wait_time;          // Guarda todos los periodos de espera es decir suma todos los "t_ready_start" (WAITING TIME)
  int n_context_switches; // Veces que el scheduler le dio la CPU
  int t_completion;        // Momento en que el proceso termino (COMPLETION TIME)

  // --- Campos nuevos para MLFQ ---
  int queue;        // Cola actual del proceso
  int ticks_used;    // Ticks consumidos en el quantum actual de su cola
};



// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
