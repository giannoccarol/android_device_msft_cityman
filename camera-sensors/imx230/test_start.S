/* SPDX-License-Identifier: GPL-2.0-only */
.syntax unified
.text
.global _start
_start:
    bl run_tests
    mov r7, #1
    svc #0
.section .note.GNU-stack,"",%progbits
