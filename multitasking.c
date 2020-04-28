#include <gb/gb.h>
#include <gb/font.h>
#include <stdio.h>
#include <string.h>
#include <rand.h>

#define CONTEXT_STACK_SIZE 256
#define CONTEXT_STACK_SIZE_IN_WORDS CONTEXT_STACK_SIZE/2
typedef void (* task_t)();
typedef struct context_t {                     // context of a task
   unsigned char * task_sp;
   struct context_t * next;                    // next context
   UINT16 stack[CONTEXT_STACK_SIZE_IN_WORDS];  // context stack size
} context_t;
typedef struct {                               // context of main() -- stack is a native machine stack
   unsigned char * task_sp;
   struct context_t * next;   // next context
} main_context_t;


context_t *current_context = 0, *first_context = 0;

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
        push    HL
        push    AF
        push    BC
        push    DE
        
        push    AF
        push    AF
            
        jp      _supervisor
__endasm;    
}

void add_task(context_t * context, task_t task) {   
    if (context) {
        context->next = first_context;
        first_context = context;
        
        // if !task then it is a main thread
        if (task) { 
            // memset is not actually necessary
            for (int i = 0; i < CONTEXT_STACK_SIZE_IN_WORDS; i++) context->stack[i] = 0;
            // set stack for a new task
            context->stack[CONTEXT_STACK_SIZE_IN_WORDS - 1] = (UINT16)task;
            context->task_sp = &context->stack[CONTEXT_STACK_SIZE_IN_WORDS - 5];
        }
        current_context = first_context;
    }
}


// task1
const unsigned char const sprite[] = {0x00,0x00,0x00,0x3C,0x00,0x66,0x00,0x5E,0x00,0x5E,0x00,0x7E,0x00,0x3C,0x00,0x00};
UBYTE x = 8, dx = 1, y = 16, dy = 1;
void task1() {
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
int task2_value = 0;
void task2() {
    while (1) { task2_value++; }            // increment as fast as possible
}
context_t task2_context;

// task3
UBYTE task3_value = 0;
void task3() {
    while (1) {
        task3_value = rand() % 10;
        switch_to_thread();                 // leave the rest of a quant to other tasks
    }
}
context_t task3_context;


// main
main_context_t main_context;
void main() {
    font_init();                           // Initialize font
    font_set(font_load(font_spect));       // Set and load the font

    __critical {
        add_task(&task1_context, &task1);
        add_task(&task2_context, &task2);
        add_task(&task3_context, &task3);   
        add_task((context_t*)&main_context, 0);       // extra context is needed for main()  
    }
        
    add_TIM(&supervisor);
    TMA_REG = 0xC0U;                      // nearly 50 hz? not sure
    TAC_REG = 0x04U;
    set_interrupts(VBL_IFLAG | TIM_IFLAG);
    
    SHOW_SPRITES;
    
    while (1) {
        __critical{ 
            printf("t2:0x%x t3:%d\n", task2_value, (int)task3_value); 
        }
        delay(100);
    }
}
