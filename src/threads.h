#ifndef __THREADS_H_INCLUDE
#define __THREADS_H_INCLUDE

#define ENABLE_WAIT_STAT
#ifdef DISABLE_WAIT_STAT
    #undef ENABLE_WAIT_STAT
#endif

#include <stdint.h>

#define DEFAULT_STACK_SIZE 0

#define CONTEXT_STACK_SIZE 256                  // stack size in bytes, must be sufficent for your threads, set it with care
#define CONTEXT_STACK_SIZE_IN_WORDS (CONTEXT_STACK_SIZE >> 1)
typedef void (* threadproc_t)(void * arg, void * ctx) __sdcccall(0); // prototype of a threadproc()
typedef struct context_t {                      // context of a thread
    unsigned char * task_sp;                    // current stack pointer of a thread
    struct context_t * next;                    // next context
    void * queue;                               // queue of the context
    void * userdata;                            // user data of the context
    uint8_t thread_id;                            // identifier of a thread
    uint8_t terminated;                           // thread termination signal 
    uint8_t finished;                             // thread finished signal
    uint16_t stack[CONTEXT_STACK_SIZE_IN_WORDS];  // context stack size
} context_t;
typedef struct {                                // context of main(): stack is "crt-native"
    unsigned char * task_sp;                    // current stack pointer of main()
    struct context_t * next;                    // next context
} main_context_t;

extern main_context_t main_context;             // this is a main() context  
extern context_t * first_context;               // start of a context chain 

extern void supervisor();                       // supervisor function
extern void switch_to_thread();                 // release the rest of the slice and switch to the next thread

void supervisor_ISR();                          // bare ISR handler for use with ISR_VECTOR() macro only

extern void create_thread(context_t * context, int stack_size, threadproc_t threadproc, void * arg);
extern void destroy_thread(context_t * context);

extern context_t * get_thread_by_id(uint8_t id);

extern void terminate_thread(context_t * context);
extern void join_thread(context_t * context); 

typedef uint8_t mutex_t;

inline uint8_t mutex_init(mutex_t * mutex) {
    if (mutex) *mutex = 0xfe; else return 1;
    return 0;
}

#if defined(__TARGET_gb) || defined(__TARGET_ap) || defined(__TARGET_megaduck)
extern uint8_t mutex_try_lock(mutex_t * mutex) __preserves_regs(b, c, d) __sdcccall(0);
extern void mutex_lock(mutex_t * mutex) __preserves_regs(b, c, d, e) __sdcccall(0);
extern void mutex_unlock(mutex_t * mutex) __preserves_regs(b, c, d, e) __sdcccall(0);
#elif defined(__TARGET_sms) || defined(__TARGET_gg) || defined(__TARGET_msxdos)
extern uint8_t mutex_try_lock(mutex_t * mutex) __z88dk_fastcall __preserves_regs(b, c, d, e, iyh, iyl);
extern void mutex_lock(mutex_t * mutex) __z88dk_fastcall __preserves_regs(b, c, d, e, iyh, iyl);
extern void mutex_unlock(mutex_t * mutex) __z88dk_fastcall __preserves_regs(b, c, d, e, iyh, iyl);
#else
#error Unrecognized port
#endif
inline void mutex_destroy(mutex_t * mutex) { mutex; }

#endif