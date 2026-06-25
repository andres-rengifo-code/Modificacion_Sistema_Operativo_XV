// testproc.c
//
// Programa de prueba para medir el planificador de xv6 (original y MLFQ).
// Crea una carga mixta de procesos:
//   - 4 procesos "interactivos": hacen poco computo y se bloquean seguido
//     (simulado con sleep()), nunca agotan un quantum largo.
//   - 2 procesos de "computo pesado": un loop largo sin bloquearse nunca,
//     pensado para agotar quantums y, en MLFQ, degradar hasta la cola 3.
//
// Cada proceso, al terminar, imprime su linea [METRICAS] (ver exit()
// modificado en proc.c). Correr el mismo testproc en el sistema original
// y en el modificado con MLFQ permite comparar los resultados.
//
// Uso dentro de xv6:  $ testproc
 
#include "types.h" // Define tipos basicos usados en xv6 (ej. uint)
#include "stat.h"  // Define struct stat; necesario porque user.h lo requiere
#include "user.h"  // Declara las funciones que un programa de usuario puede llamar (fork, exit, sleep, printf, etc.)

// Cantidad de iteraciones del "trabajo corto" de un proceso interactivo
#define INTERACTIVE_WORK   20000
// Cantidad de veces que el proceso interactivo repite trabajo + bloqueo
#define INTERACTIVE_ROUNDS 5
// Ticks que duerme el proceso interactivo entre rondas (simula espera I/O)
#define INTERACTIVE_SLEEP  3
 
// Cantidad de iteraciones del proceso de computo pesado
#define HEAVY_WORK 4000000
 
//--------------------------------------------------------------------
// FUNCION
// Mantiene a la CPU ocupada dando vueltas a un calculo sin
// proposito util. No usa ninguna syscall, asi que el proceso no se
// bloquea mientras esto corre.
//
// volatile evita que el compilador elimine el loop como optimizacion
// (al no usarse "x" para nada, podria borrarlo entero y la funcion
// terminaria sin haber ocupado la CPU).
//---------------------------------------------------------------------
void
burn_cpu(int iterations)
{
  volatile int i; //contador de vueltas
  volatile int x = 0; //Guarda el resulattdo de la operacion para que la CPU  tenga que calcular algo en cada vuelta 
  for(i = 0; i < iterations; i++){
    x = x + (i % 7) - (i % 3); //proceso que va a relizar la CPU (solo se usa para ocupar tiempo en la CPU)
  }
}
 
//-----------------------------------------------------------------------
//PROCESO
// Proceso interactivo: alterna trabajo corto + sleep, varias rondas.
// Nunca deberia agotar un quantum largo, porque se bloquea seguido.
//---------------------------------------------------------------------
void
interactive_process(int id)
{
  int r;
  for( r = 0; r < INTERACTIVE_ROUNDS; r++){
    burn_cpu(INTERACTIVE_WORK); // Ocupa la cpu en un trabajo corto 
    sleep(INTERACTIVE_SLEEP); // Simulacion de bloquear o dormir un proceceso en espera de algo (Que pase un tiempo especifico)
  }
  printf(1, "[TEST] interactivo %d (pid %d) termino\n", id, getpid()); // Imprime que el proceso  termino 
  exit(); // termina el prooceso y imprime las Metricas
}
 
//----------------------------------------------------------------------
// Proceso de computo pesado: un solo bucle largo, sin bloquearse nunca.
// Diseniado para ver como se comporta con un proceso que tome arto tiempo 
//---------------------------------------------------------------------------
void
heavy_process(int id)
{
  burn_cpu(HEAVY_WORK); // ocupa la cpu en un trabajo que le va a costar mas esfuerzo
  printf(1, "[TEST] pesado %d (pid %d) termino\n", id, getpid());// Imprime que el proceso  termino 
  exit(); // termina el prooceso y imprime las Metricas
}
 

//---------------------------------------------------------------------
//INICIALIAZDOR
//encargado de mandar a crear los 6 porcesos esperar que se ejecuten 
//y que terminen
//-----------------------------------------------------------------------
int
main(int argc, char *argv[])
{
  int i;
  int pid;
 
  printf(1, "[TEST] iniciando carga mixta: 4 interactivos + 2 pesados\n");
 
  // Lanza los 2 procesos de computo pesado
  for(i = 0; i < 2; i++){
    pid = fork();
    if(pid < 0){
      printf(1, "[TEST] fork fallo (pesado %d)\n", i);
      exit();
    }
    if(pid == 0){
      heavy_process(i);
      // heavy_process termina con exit(), no vuelve aqui
    }
  }

  // Lanza los 4 procesos interactivos
  for(i = 0; i < 4; i++){
    pid = fork();
    if(pid < 0){
        // si fork() fallo dedvuelve este mensaje
      printf(1, "[TEST] fork fallo (interactivo %d)\n", i);
      exit();
    }
    //si fork() creo un porceso de manera adecuada este devuelve 0 
    if(pid == 0){
      interactive_process(i);
      // interactive_process termina con exit(), no vuelve aqui
    }
  } 
  // El proceso padre espera a que todos los hijos (6 en total) terminen
  for(i = 0; i < 6; i++){
    wait();
  }
 
  printf(1, "[TEST] carga mixta completa: todos los procesos terminaron\n");
  exit();
}
 