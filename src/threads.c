#include <gbdk/version.h>
#include <stdint.h>

#define THREAD_SMART_SWITCHING

#include "threads.h"

#if defined(__TARGET_gb) || defined(__TARGET_ap) || defined(__TARGET_megaduck)
    #define REGISTER_BLOB_SIZE 4
#elif defined(__TARGET_sms) || defined(__TARGET_gg) || defined(__TARGET_msxdos)
    #define REGISTER_BLOB_SIZE 6
#endif

uint8_t switch_mutex = 0xff;
main_context_t main_context = {0, 0};                           // this is a main task context  
context_t * first_context = (context_t *)&main_context;         // start of a context chain 
context_t * current_context = (context_t *)&main_context;       // current context pointer 

static void __dummy() __nonbanked __naked {
__asm
#if defined(__TARGET_gb) || defined(__TARGET_ap) || defined(__TARGET_megaduck)
_supervisor_ISR::        
#if (__GBDK_VERSION < 312)
        push    HL                      ; push all in crt order
        push    AF
#else
        push    AF                      ; push all in GBDK 3.1.2+ crt order
        push    HL
#endif            
        push    BC
        push    DE

        add     SP, #-4
        jr      _supervisor

_switch_to_thread::
        di
#if (__GBDK_VERSION < 312)
        push    HL                      ; push all in crt order
        push    AF
#else
        push    AF                      ; push all in GBDK 3.1.2+ crt order
        push    HL
#endif            
        push    BC
        push    DE

#ifdef THREAD_SMART_SWITCHING
        ld      HL, #_switch_mutex
        ld      (HL), #0b11111101       ; release of the slice disables next scheduled switching of context
#endif
        add     SP, #-4                 ; two words on the top of a stack are dropped by supervisor
                                        ; to make it compatible with the beginning of an interriupt routine in crt
_supervisor::
#ifdef THREAD_SMART_SWITCHING
        ld      HL, #_switch_mutex      ; check whether we need to switch context this time or not
        sra     (HL)
        jr      nc, 3$                  ; carry is normally set here
#endif
                                        ; context switch is required
        ldhl    SP, #4
        ld      B, H
        ld      C, L                    ; BC = SP + 4
        
        ld      HL, #_current_context
        ld      A, (HL+)
        ld      H, (HL)
        ld      L, A
        
        ld      (HL), C
        inc     HL
        ld      (HL), B
        inc     HL                      ; _current_context->task_sp = SP of the task
        
        ld      A, (HL+)
        ld      H, (HL)
        ld      L, A                    ; HL = context_t(_current_context)->next

        or      H
        jr      NZ, 1$
        
        ld      HL, #_first_context 
        ld      A, (HL+)
        ld      H, (HL)
        ld      L, A                    ; if (!next) HL = _first_context        
1$:
        ld      A, L
        ld      (#_current_context), A
        ld      A, H
        ld      (#_current_context + 1), A

        ld      A, (HL+)    
        ld      H, (HL)
        ld      L, A

        ld      SP, HL                  ; switch stack and restore context
2$:
#if (__GBDK_VERSION < 312)
        pop     DE                      ; this order is in crt
        pop     BC
        pop     AF
        pop     HL                  
#else
        pop     DE                      ; this order is in GBDK 3.1.2+ crt
        pop     BC
        pop     HL                  
#endif            
#if defined(ENABLE_WAIT_STAT) && (__GBDK_VERSION < 312)
        push    AF
#endif
#ifdef ENABLE_WAIT_STAT
4$:
        ldh     A, (#_STAT_REG)
        and     #0x02
        jr      NZ, 4$
#endif        
#if defined(ENABLE_WAIT_STAT) || (__GBDK_VERSION >= 312)
        pop     AF
#endif

        reti
#ifdef THREAD_SMART_SWITCHING
3$:                                     ; switching is not required, context was switched by user during the previous slice
        add     SP, #4
        jr      2$
#endif
#elif defined(__TARGET_sms) || defined(__TARGET_gg) || defined(__TARGET_msxdos)
_supervisor_ISR::        
        push    af
        push    bc
        push    de
        push    hl
        push    iy
        push    ix

        push    HL
        jp      _supervisor

_switch_to_thread::
        di
        push    af
        push    bc
        push    de
        push    hl
        push    iy
        push    ix

#ifdef THREAD_SMART_SWITCHING
        ld      hl, #_switch_mutex
        ld      (hl), #0b11111101       ; release of the slice disables next scheduled switching of context
#endif
        push    HL                      ; one word on the top of a stack is dropped by supervisor
                                        ; to make it compatible with the beginning of an interriupt routine in crt
_supervisor::
#ifdef THREAD_SMART_SWITCHING
        ld      HL, #_switch_mutex      ; check whether we need to switch context this time or not
        sra     (HL)
        jp      nc, 3$                  ; carry is normally set here
#endif
                                        ; context switch is required
        ld      HL, #2
        add     HL, SP
        ex      DE, HL
        
        ld      HL, (_current_context)
        
        ld      (HL), E
        inc     HL
        ld      (HL), D
        inc     HL                      ; _current_context->task_sp = SP of the task
        
        ld      A, (HL)
        inc     HL
        ld      H, (HL)
        ld      L, A                    ; HL = context_t(_current_context)->next

        or      H
        jp      NZ, 1$
        
        ld      HL, (_first_context)    ; if (!next) HL = _first_context
1$:
        ld      (_current_context), HL

        ld      A, (HL)
        inc     HL    
        ld      H, (HL)
        ld      L, A

        ld      SP, HL                  ; switch stack and restore context
2$:
        pop     ix
        pop     iy
        pop     hl
        pop     de
        pop     bc
        pop     af
        ei
        reti
#ifdef THREAD_SMART_SWITCHING
3$:                                     ; switching is not required, context was switched by user during the previous slice
        pop     HL
        jp      2$
#endif
#endif
__endasm;    
}

_Noreturn void __trap_function(context_t * context) __sdcccall(0) {
    context->finished = 1;
    while(1) switch_to_thread();        // it is safe to dispose context when the thread execution is here
}

context_t * get_thread_by_id(uint8_t id) {
    context_t * ctx = first_context->next;
    while (ctx) {
        if (ctx->thread_id == id) return ctx;
        ctx = ctx->next;
    }
    return 0;
}

uint8_t generate_thread_id() {
    uint8_t id = 1;
    while (get_thread_by_id(id)) id++;
    return id;
}

void create_thread(context_t * context, int stack_size, threadproc_t threadproc, void * arg) {   
    if ((context) && (threadproc)) {
        if (!stack_size) stack_size = CONTEXT_STACK_SIZE_IN_WORDS; else stack_size = stack_size >> 1;
        context_t * last_context;
        
        // initialize the new context
        context->next = context->finished = context->terminated = 0;
        context->thread_id = generate_thread_id();
        // memset is not actually necessary
        for (int i = 0; i < stack_size; i++) context->stack[i] = 0;
        // set stack for a new thread
        context->stack[stack_size - 1] = (uint16_t)context;           // thread context 
        context->stack[stack_size - 2] = (uint16_t)arg;               // threadproc argument
        context->stack[stack_size - 3] = (uint16_t)__trap_function;   // fall thare when threadproc exits
        context->stack[stack_size - 4] = (uint16_t)threadproc;        // threadproc entry point   
        context->task_sp = &context->stack[stack_size - 4 - REGISTER_BLOB_SIZE];    // space for registers (all registers are 0 on threadproc entry)

        // get last context in the chain
        for (last_context = first_context; (last_context->next); last_context = last_context->next) ;
        // append new context
        last_context->next = context;
    }
}

void destroy_thread(context_t * context) {
    if (context) {
        context_t * prev_context = first_context;
        while ((prev_context) && (prev_context->next != context)) prev_context = prev_context->next;
        if (prev_context) prev_context->next = context->next;
    }
}

void terminate_thread(context_t * context) {
    if (context) context->terminated = 1;
}

void join_thread(context_t * context) {
    if (context) while (!context->finished) switch_to_thread();
}

#if defined(__TARGET_gb) || defined(__TARGET_ap) || defined(__TARGET_megaduck)
uint8_t mutex_trylock(mutex_t * mutex) __preserves_regs(b, c, d) __naked __sdcccall(0) {
    mutex;
__asm
        ldhl    sp, #2
        ld      a, (hl+)
        ld      h, (hl)
        ld      l, a

        xor     a
        sra     (hl)
        rla

        ld      e, a
        ret        
__endasm;
}
void mutex_lock(mutex_t * mutex) __preserves_regs(b, c, d, e) __naked __sdcccall(0) {
    mutex;
__asm
        ldhl    sp, #2
        ld      a, (hl+)
        ld      h, (hl)
        ld      l, a
2$:
        sra     (hl)
        jr      nc, 1$
        call    _switch_to_thread    ; preserves everything
        jr      2$
1$:
        ret
__endasm;
}
void mutex_unlock(mutex_t * mutex) __preserves_regs(b, c, d, e) __naked __sdcccall(0) {
    mutex;
__asm
        ldhl    sp, #2
        ld      a, (hl+)
        ld      h, (hl)
        ld      l, a
        res     0, (hl)
        ret
__endasm;
}
#elif defined(__TARGET_sms) || defined(__TARGET_gg) || defined(__TARGET_msxdos)
uint8_t mutex_try_lock(mutex_t * mutex) __z88dk_fastcall __preserves_regs(b, c, d, e, iyh, iyl) __naked {
    mutex;
__asm
        xor     a
        sra     (hl)
        rla

        ld      l, a
        ret        
__endasm;
}
void mutex_lock(mutex_t * mutex) __z88dk_fastcall __preserves_regs(b, c, d, e, iyh, iyl) __naked {
    mutex;
__asm
2$:
        sra     (hl)
        jr      nc, 1$
        call    _switch_to_thread    ; preserves everything
        jr      2$
1$:
        ret
__endasm;
}
void mutex_unlock(mutex_t * mutex) __z88dk_fastcall __preserves_regs(b, c, d, e, iyh, iyl) __naked {
    mutex;
__asm
        res     0, (hl)
        ret
__endasm;
}
#endif