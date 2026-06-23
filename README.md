# Modificación del Planificador xv6 — MLFQ

Proyecto de Sistemas Operativos: implementación de un planificador **MLFQ (Multilevel Feedback Queue)** de 4 colas sobre el sistema operativo xv6, en sustitución del planificador Round Robin original del MIT.

---

##  Estructura del repositorio

| Rama | Contenido |
|------|-----------|
| `main` | Este README y estructura general del proyecto |
| `Documentacion-XV6-Original` | Análisis del planificador original: código fuente comentado, estados de procesos, estructuras de datos y funciones del scheduler de xv6 |
| `Implementacion-MLFQ` | Código fuente modificado con el nuevo planificador MLFQ (4 colas), programa de prueba `testproc` y resultados de las métricas comparativas |

> La documentación completa del proyecto (análisis teórico, diseño, tablas comparativas y conclusiones) está en el archivo `DOCUMENTACION_SO_MEJORADA.docx`, disponible en la rama `Documentacion-XV6-Original`.

---

##  Cómo correr el proyecto

### Requisitos previos

- Ubuntu 22.04 LTS (o equivalente)
- QEMU versión 6.2.0 o superior
- Toolchain de compilación cruzada para x86: `gcc`, `make`, `binutils`

Instalar dependencias en Ubuntu:

```bash
sudo apt update
sudo apt install qemu-system-x86 gcc make git
```

### Clonar el repositorio

```bash
git clone https://github.com/andres-rengifo-code/Modificacion_Sistema_Operativo_XV.git
cd Modificacion_Sistema_Operativo_XV
```

### Correr el xv6 original (Round Robin)

```bash
git checkout Documentacion-XV6-Original
make qemu-nox
```

### Correr el xv6 modificado (MLFQ)

```bash
git checkout Implementacion-MLFQ
make qemu-nox
```

Para salir de QEMU: `Ctrl + A`, luego `X`.

---

##  Cambios realizados (rama `Implementacion-MLFQ`)

Los siguientes archivos fueron modificados respecto al xv6 original del MIT:

| Archivo | Cambio |
|---------|--------|
| `proc.h` | Se agregó el campo `queue` a `struct proc` para rastrear en qué cola MLFQ se encuentra cada proceso, y el campo `ticks_used` para contabilizar los ticks consumidos en el quantum actual |
| `proc.c` | Se reemplazó la función `scheduler()` con la implementación MLFQ: selección por prioridad estricta entre 4 colas, degradación automática al agotar el quantum, y FCFS en la cola 3. También se modificó `allocproc()` para inicializar `queue = 0` en cada proceso nuevo |
| `trap.c` | Se modificó el manejador de interrupción de reloj para incrementar `ticks_used` y disparar la degradación de cola cuando corresponde, en lugar de simplemente llamar a `yield()` |
| `testproc.c` | Programa de prueba nuevo (no existía en xv6 original): lanza 4 procesos interactivos y 2 procesos de cómputo pesado, mide y reporta response time, waiting time, turnaround time y número de cambios de contexto para cada proceso |

---

##  Resultados resumidos

El algoritmo MLFQ redujo el turnaround time de los procesos de cómputo pesado de ~90 ms a 30-50 ms, y los cambios de contexto de 5-6 a 1-2, sin afectar el comportamiento de los procesos interactivos. Ver análisis completo en la documentación.

---

##  Documentación

La documentación detallada del proyecto está en `DOCUMENTACION_SO_MEJORADA.docx` (rama `Documentacion-XV6-Original`) e incluye:

- Entorno de trabajo y pasos de configuración (con capturas)
- Análisis teórico del planificador Round Robin original
- Descripción de estructuras de datos y funciones del scheduler
- Diseño del algoritmo MLFQ: fundamento teórico, objetivos y reglas
- Tablas comparativas de métricas y análisis de resultados
