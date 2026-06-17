#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

// Esta es una estructura de datos la cual crea una tabla de 64 filas
// ademas de generar un candado con el cual solo una funcion a la ves puede 
// modificar la tabla(blindando una capa de seguridad)

//TABLA DE PROCESOS 
struct {
  struct spinlock lock; // candado el cual da seguridad al modificar la tabla
  struct proc proc[NPROC]; // <- define la cantidad de procesos que puede manejar la cpu
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.

//FUNCION
//Encargada de buscar un slot vacio (UNUSED) en la tabla de procesos (ptable)
//y lo prepara para la creacion de un nuevo proceso 
static struct proc* allocproc(void)
{
  struct proc *p;
  char *sp;

  //Toma el candado para revisar la tabla 
  acquire(&ptable.lock);

  //Recorre la tabla para buscar slot vacios 
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found; // Si encuentra un slot vacio

  // Si no encuentra ningun slot vacio -> error(Todos los slot estan ocupados)
  release(&ptable.lock);
  return 0;

//Marca el estado del slot vacio como EMBRYO (naciendo o creando proceso)
found:
  p->state = EMBRYO;
  p->pid = nextpid++; // le asigna un numero unico al proceso

  release(&ptable.lock); // suelta el candado para que otra funcion la pueda utilizar

  // Allocate kernel stack.
  // Crea su pila en el kernel 
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }

  //Prepara el contexto inicial para que depues la cpu sepa donde iniciar
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p; // retorna el proceso listo
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.

//FUNCION
//Funcion encargada de crear procesos 
//Crea los procesos de una manera particular :
//Crendo una copia del proceso el cual llamo a fork() 
//Esta copia seria el nuevo proceso donde este nuevo proceso 
//Puede ser igual al que lo llamo o convertirse en otro proceso 
//Completamente diferente

int fork(void)
{
  int i, pid;
  struct proc *np; //np = nuevo proceso (hijo)
  struct proc *curproc = myproc(); //curproc = proceso actual (padre) 

  // Allocate process.

  // Busca un slot vacio en ptable
  // si no ay espacio da error
  if((np = allocproc()) == 0){ 
    return -1;
  }

  // Copy process state from proc.

  //Fotocopia la memoria de del proceso padre al proceso hijo
  //Si falla limpia y retorna error
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }

  //Copia la informacion del proceso padre al proceso hijo
  np->sz = curproc->sz; //mismo tamanio de memoria
  np->parent = curproc; // le dice al proceso hijo quien es el padre
  *np->tf = *curproc->tf; // mismos registros de la CPU

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0; // manera con la cual el nuevo proceso hijo sabe que es el hijo y no el padre

  //Copia los archivos del proceso padre al proceso hijo
  //Maximo 16 archivos (NOFILE = 16)
  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  //Copia la carpeta actual del proceso padre al proceso hijo
  np->cwd = idup(curproc->cwd);

  //Copia del nombre del proceso padre al proceso hijo
  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  //Toma el candado de la ptable para asi depues poder modificar el estado del proceso
  acquire(&ptable.lock);
  
  //EL proceso hijo pasa a ser un proceso ejecutable (RUNNABLE)
  //Es decir que el proceso ya puede correr
  np->state = RUNNABLE;

  //Suelta el candado
  release(&ptable.lock);

  //Retorna el Pid del hijo al padre 
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.

//(Planificador de procesos)
// ALGORITMO:
// Round Robin puro:
// Recorre la tabla de izquierda a derecha
// Le da la CPU al primero que esté RUNNABLE
// Todos reciben el mismo tiempo (10ms)
// Sin prioridades, sin favoritismos
void scheduler(void)
{
  //CPU actual ningun proceso corre al inicio 
  struct proc *p;
  struct cpu *c = mycpu(); 
  c->proc = 0;
  
  //Se genera un bucle infinito mientras el sistema operativo este encendido
  for(;;){
    // Enable interrupts on this processor.
    sti(); // Habilita la interrupcion de la CPU cada 10 ms 

    // Loop over process table looking for process to run.
    acquire(&ptable.lock); // Candado para poder modificar la tabla de procesos
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) //Bucle para recorrer la tabla de procesos
    {
      //Si el proceso no esta listo para correr (RUNNABLE) continua
      //recorriendo la tabla  
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      //cuando encuentre un proceso que este listo para corer (RUNNABLE) 
      //se le asigna este procecso a la CPU
      c->proc = p;
      switchuvm(p); // Se carga la memoria del proceso 
      p->state = RUNNING; // cambia el estadoa corriendo (RUNNING)

      //Me falta complender estas dos lineas 
      swtch(&(c->scheduler), p->context); //Guarda el contexto de el proceso que sale de la cpu y carga el contexto de el proceso que va a entrar a la cpu
      switchkvm(); // Se limpia la memoria que estaba ocupando el proceso

      // Process is done running for now.
      // It should have changed its p->state before coming back.

      //Ningun proceso se encunetra corriendo en la CPU
      c->proc = 0;
    }
    //Suelta el candado para que otras funciones puedan modificar la tabla ded procesos
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.

//FUNCION
//Esta funcion se encarda primero que todo quye se cumplan una reglas para depues guardar el contexto de la funcion actual para depues ser utilizada
//y brindrale el contexto a la CPU del proceso que va a correr 
void sched(void)
{
  int intena;
  struct proc *p = myproc();
  //Verifica que el candado haya sido tomado antes de llamar a sched()
  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  //verifica que no haya candados extras
  if(mycpu()->ncli != 1)
    panic("sched locks");
  //verifica que el proceso NO esta corriendo (RUNNING)
  if(p->state == RUNNING)
    panic("sched running");
  //verifica que las interrupciones esten Desactivadas
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  
  intena = mycpu()->intena; //guarda el estado de las interrupciones

  //Guarda el contexto o estado del proceso actual osea lo que ya ah procesado y devuelve los registros o el
  //de el proceso que esta por correr la cpu
  swtch(&p->context, mycpu()->scheduler);

  mycpu()->intena = intena;//Restaura el estado de las interrupciones
}

// Give up the CPU for one scheduling round.

//FUNCION
//Esta funcion se encarga de que el proceso pase de corriendo a listo para correr
//cuando su turno termina y le cede el turno a otro proceso
void yield(void)
{
  //Toma el candado para depues poder modificar su estado
  acquire(&ptable.lock);  //DOC: yieldlock
  //Cabia el estado del proceso a listo para ejecutar (RUNNABLE)
  myproc()->state = RUNNABLE;
  //llama al scheduler (Gestor de procesos)
  sched();
  //Devuelve el candado 
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.

//FUNCION
//se encarga de poner los estados que esperan un dato externo en estado bloqueadoo (SlEEPING)
void
sleep(void *chan, struct spinlock *lk)
//Dos parametros
//1.chan = ondoca lo que esta esperando
//2.lk = el candado que el proceso tenia antes de SLEEP()
{
  struct proc *p = myproc(); //Obtiene el proceso actual
  
  //Verifica que este un proceso corriendo
  if(p == 0) 
    panic("sleep");
  
  //Verirfica que SLEEO() tenga un candado
  //ya que va a modificar su estado
  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.

  //VErifica que el candado tomado sea el correcto si este no es el correcto entonces lo libera 
  //y toma otro candado
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan; //Guarda lo que esta esperando el proceso usa wakeup para encontrar lo que esta esperando
  p->state = SLEEPING; //cambia el estado a SLEEPING

  //llama al gestor de procesos para que eliga otro proceso
  sched();

  // Tidy up.

  //cuando el proceso tenga lo que esta esperendo este permite que 
  //limpie lo que espera y que este ya no espere nada
  p->chan = 0;

  // Reacquire original lock.

  //retaura o devuelve el candado original
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.

//Funcion
//Encargada de liberar un proceso que este bloqueado cuando 
//encuentre lo que faltaba para ser desbloqueada
static void
wakeup1(void *chan)
{
  struct proc *p; //Puntero para recorrer la tabla

  //Recorre toda la tabla
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)

    //Se verifica que el proceso este en estado SLEEPING y que debe de estar esperando un evento especifico
    if(p->state == SLEEPING && p->chan == chan)
      //si eso es asi entonces se cambia el estado del proceso a RUNNABLE 
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
//FUNCION
//encargada de desbloquear el proceso con el candado para la modificacion de los procesos
void
wakeup(void *chan)
{
  //Toma el candado para asi poder modificar la tabla
  acquire(&ptable.lock);
  //Se realiza la funcion wakeup1 
  wakeup1(chan);
  //Suelta el candado para que otras funciones puedan modificar la tabla
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
