#FUNCION
#encargada de guaradr los datos del proceso que sale dee la cpu y 
#insartar los datos ya calculados del procesoq  ue va a a entrar a la CPU



# Context switch
#
#   void swtch(struct context **old, struct context *new);
# 
# Save the current registers on the stack, creating
# a struct context, and save its address in *old.
# Switch stacks to new and pop previously-saved registers.

.globl swtch
swtch:
  # Recibe dos parámetros:
  # 1. &old → dónde guardar el contexto actual
  # 2. new  → qué contexto cargar
  movl 4(%esp), %eax
  movl 8(%esp), %edx

  # Save old callee-saved registers
  # GUARDAR DATOS  DEL PROCESO ACTUAL
  pushl %ebp 
  pushl %ebx
  pushl %esi
  pushl %edi

  # Switch stacks
  movl %esp, (%eax)
  movl %edx, %esp

  # Load new callee-saved registers
  # CARGAR DATOS DEL SIGUIENTE PROCESO
  popl %edi
  popl %esi
  popl %ebx
  popl %ebp
  ret
