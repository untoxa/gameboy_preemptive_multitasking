#include <gb/gb.h>
#include <gb/font.h>
#include <stdio.h>
#include <string.h>

#define CONTEXT_STACK_SIZE 256
typedef void (* task_t)();
typedef struct context_t {
   unsigned char * task_sp;
   struct context_t * next;   // next context
   unsigned char stack[CONTEXT_STACK_SIZE];  // context stack size
} context_t;

context_t *current_context = 0, *first_context = 0;

void supervisor() __naked {
__asm        
        di
        pop     HL     
        pop     HL                  ; we never return to timer handler
        
        lda     HL, 0(SP)
        ld      B, H
        ld      C, L                ; BC = SP
        
        ld      HL, #_current_context
        ld      A, (HL+)
        ld      H, (HL)
        ld      L, A
        
        ld      (HL), C
        inc     HL
        ld      (HL), B
        inc     HL                  ; _current_context->task_sp = SP of the task
        
        ld      A, (HL+)
        ld      H, (HL)
        ld      L, A                ; HL = context_t(_current_context)->next

        or      H
        jr      NZ, 1$
        ld      HL, #_first_context
        ld      A, (HL+)
        ld      H, (HL)
        ld      L, A
1$:     ld      A, L
        ld      (#_current_context), A
        ld      A, H
        ld      (#_current_context + 1), A

        ld      A, (HL+)    
        ld      H, (HL)
        ld      L, A

        ld      SP, HL

        pop     DE                  ; pop all regs
        pop     BC
        pop     AF
        pop     HL                  

        ei
        ret
__endasm;    
}

UWORD __sp_save;
unsigned char * __context_stack_end;
task_t __task;
void add_task(context_t * context, task_t task) {
    if (!context) return;
    context->next = first_context;
    first_context = context;
    __context_stack_end = &context->stack[CONTEXT_STACK_SIZE - 1];
    __task = task;    
    __asm
        ld      (#___sp_save), SP
        ld      HL, #___context_stack_end
        ld      A, (HL+)
        ld      H, (HL)
        ld      L, A
        ld      SP, HL
        
        ld      HL, #___task
        ld      A, (HL+)
        ld      H, (HL)
        ld      L, A
        push    HL                  ; address of a task is pushed to task context
        
        push    HL                  ; push all regs
        push    AF
        push    BC
        push    DE     
        
        ld      HL, #___sp_save
        ld      A, (HL+)
        ld      H, (HL)
        ld      L, A
        ld      SP, HL
    __endasm;    
    context->task_sp = __context_stack_end - 10;
}

// task1

int task1_counter = 0;
void task1() {
    while (1) {
        task1_counter++;
        delay(100);
    }
}
context_t task1_context;

// task2

int task2_counter = 0;
void task2() {
    while (1) {
        task2_counter++;
        delay(33);
    }
}
context_t task2_context;

// task3

int task3_counter = 0;
void task3() {
    while (1) {
        task3_counter++;
        delay(2);
    }
}
context_t task3_context;


// main
context_t main_context;

void main() {
	font_init();                           // Initialize font
	font_set(font_load(font_spect));       // Set and load the font

    __critical {
        add_task(&task1_context, &task1);
        add_task(&task2_context, &task2);
        add_task(&task3_context, &task3);   
        add_task(&main_context, 0);       // extra context is needed for main()  
        current_context = first_context;   
    }
        
    add_TIM(&supervisor);
    TMA_REG = 0xC0U;
    TAC_REG = 0x04U;
    set_interrupts(VBL_IFLAG | TIM_IFLAG);
    
    
    while (1) {
        printf("t1:%x t2:%x t3:%x\n", task1_counter, task2_counter, task3_counter);
        delay(1000);
    }
}
