#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}


//PAGEBREAK: 41
//---------------------------------------------------------------------------------------
//FUNCION
// Manejador central de interrupciones, excepciones y llamadas al sistema.
// Se ejecuta automaticamente cuando ocurre alguno de estos eventos y segun
// el tipo de interrupcion (tf->trapno) redirige la ejecucion a la rutina
// correspondiente. En el caso del timer, es quien se encarga de aplicar
// la logica de quantum y degradacion de colas del planificador MLFQ.
//----------------------------------------------------------------------------------------
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)

  // Si el proceso fue marcado para terminar y esta en espacio de usuario, lo termina
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // --- MLFQ: logica de quantum y degradacion de cola ---
  //sumale 1 tick consumido al proceso que está corriendo ahora mismo, y si ya gastó todo el quantum de su cola actual, bajalo de cola y hacelo ceder la CPU


  // para entrar al bloque se deben de cumplir tres condiciones:
  // 1. que haya un proceso corriendo
  // 2. El porceso debe de estar en etado RUNNING
  // 3. Que la interrupcion por la cual se usa trap() sea por el tiempo timer
  if(myproc() && myproc()->state == RUNNING && tf->trapno == T_IRQ0 + IRQ_TIMER){
    
    //Si cumplio las condiciones, significa que ya transcurrio un tick mientras
    // este proceso corria: se le suma uno a su contador personal de ticks usados
    myproc()->ticks_used++;

    //Revisamos si nos entontramos en la cola 3 
    if(myproc()->queue == NUM_QUEUE - 1){
      // Cola FCFS (la ultima): nunca cede por tiempo,
      // solo termina o se bloquea por su cuenta
    }
    // SI no esta el la ultima cola 
    // verificamos si el contador de ticks usados ya llegó (o superó) el límite permitido para esta cola
    else if(myproc()->ticks_used >= quantum_queue[myproc()->queue]){
      myproc()->queue++;        //degrada al proceso, sumándole 1 a su número de cola (de la 0 pasa a la 1, de la 1 a la 2, etc.)
      myproc()->ticks_used = 0; // resetea el contador
      yield();                  //le quita la CPU
    }
  }

  // Check if the process has been killed since we yielded

  // Vuelve a verificar si el proceso fue marcado para terminar luego del yield()
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
