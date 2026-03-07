.section .text
.global _start
_start:
    /* write(1, msg, 13) */
    mov x0, #1              /* fd = stdout */
    adr x1, msg             /* buf = &msg */
    mov x2, #13             /* len = 13 */
    mov x8, #64             /* syscall: write */
    svc #0                  /* syscall */

    /* exit(0) */
    mov x0, #0              /* return code */
    mov x8, #93             /* syscall: exit */
    svc #0                  /* syscall */

.section .data
msg:
    .ascii "Hello, World!\n"
