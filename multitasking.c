#include <gb/gb.h>
#include <gb/font.h>
#include <stdio.h>
#include <string.h>
#include <rand.h>

#define CONTEXT_STACK_SIZE 256                 // stack size in bytes, must be sufficent for your tasks, set it with care
#define CONTEXT_STACK_SIZE_IN_WORDS (CONTEXT_STACK_SIZE/2)
typedef void (* task_t)(void * arg);           // prototype of a threadfunc()
typedef struct context_t {                     // context of a task
   unsigned char * task_sp;
   struct context_t * next;                    // next context
   UINT16 stack[CONTEXT_STACK_SIZE_IN_WORDS];  // context stack size
} context_t;
typedef struct {                               // context of main(); stack is a native stack that is set by crt
   unsigned char * task_sp;
   struct context_t * next;                    // next context
} main_context_t;

main_context_t main_context = {0, 0};          // this is a main task context  
context_t * first_context = (context_t *)&main_context;    // start of a context chain 
context_t * current_context = (context_t *)&main_context;  // current context pointer 

void supervisor() __naked {
__asm        
        di

        lda     HL, 4(SP)           ; we have two technical values pushed by a crt handler
        ld      B, H
        ld      C, L                ; BC = SP + 4
        
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
        ld      L, A                ; if (!next) HL = _first_context
        
1$:     ld      A, L
        ld      (#_current_context), A
        ld      A, H
        ld      (#_current_context + 1), A

        ld      A, (HL+)    
        ld      H, (HL)
        ld      L, A

        ld      SP, HL              ; switch stack and restore context

        pop     DE                  ; this order is in crt
        pop     BC
        pop     AF
        pop     HL                  

        ei
        ret
__endasm;    
}

void switch_to_thread() __naked {
__asm        
        di
        push    HL                  ; push all in crt order
        push    AF
        push    BC
        push    DE
        
        push    AF                  ; no matter what to push here: two words on the top of a stack are dropped by supervisor
                                    ; to make it compatible with the beginning of an interriupt routine in crt
        call    _supervisor         ; we never return, call is just for the second push
__endasm;    
}

void add_task(context_t * context, task_t task, void * arg) {   
    if ((context) && (task)) {
        context_t * last_context;
        // get last context in the chain
        for (last_context = first_context; (last_context->next); last_context = last_context->next) ;
        
        // append new context
        last_context->next = context;
        
        // initialize the new context
        context->next = 0;
        // memset is not actually necessary
        for (int i = 0; i < CONTEXT_STACK_SIZE_IN_WORDS; i++) context->stack[i] = 0;
        // set stack for a new task
        context->stack[CONTEXT_STACK_SIZE_IN_WORDS - 1] = (UINT16)arg;       // threadfunc argument 
        context->stack[CONTEXT_STACK_SIZE_IN_WORDS - 3] = (UINT16)task;      // threadfunc entry point   
        context->task_sp = &context->stack[CONTEXT_STACK_SIZE_IN_WORDS - 7]; // space for registers (all become null on thread entry)
    }
}


// --- "tasks" (or "threads") ---------------------

// task1
const unsigned char const sprite[] = {0x00,0x00,0x00,0x3C,0x00,0x66,0x00,0x5E,0x00,0x5E,0x00,0x7E,0x00,0x3C,0x00,0x00};
UBYTE x = 8, dx = 1, y = 16, dy = 1;
void task1(void * arg) {
    arg;                                        // suppress warning
    set_sprite_data(0, 1, sprite);
    set_sprite_tile(0, 0); 
    while (1) {
        if (dx) { 
            x++; if (x > 20 * 8 - 1) dx = 0; 
        } else { 
            x--; if (x < 8 + 1) dx = 1;             
        }
        if (dy) { 
            y++; if (y > 18 * 8 + 8 - 1) dy = 0; 
        } else { 
            y--; if (y < 16 + 1) dy = 1;             
        }
        move_sprite(0, x, y);
        delay(10);
    }
}
context_t task1_context;

// task2
const unsigned char const hello[] = "hello!";
int task2_value = 0;
void task2(void * arg) {
    printf("arg: %s\n", arg);
    while (1) { task2_value++; }                // increment as fast as possible
}
context_t task2_context;

// task3
UBYTE task3_value = 0;
void task3(void * arg) {
    arg;                                        // suppress warning
    while (1) {
        task3_value = rand() % 10;
        switch_to_thread();                     // leave the rest of a quant to other tasks
    }
}
context_t task3_context;


// --- main ---------------------------------------

void main() {
    font_init();                                // Initialize font
    font_set(font_load(font_spect));            // Set and load the font

    __critical {
        add_task(&task1_context, &task1, 0);
        add_task(&task2_context, &task2, &hello);
        add_task(&task3_context, &task3, 0);
    }
        
    add_TIM(&supervisor);                       // may be any interrupt but not standard VBL
    TMA_REG = 0xC0U;                            // nearly 50 hz? not sure
    TAC_REG = 0x04U;
    set_interrupts(VBL_IFLAG | TIM_IFLAG);      // RUN!
    
    SHOW_SPRITES;
    
    while (1) {
        __critical{ 
            printf("t2:0x%x t3:%d\n", task2_value, (int)task3_value); 
        }
        delay(100);                             // note: delay is not 100 anymore
    }
}
