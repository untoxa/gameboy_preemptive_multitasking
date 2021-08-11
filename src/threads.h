#ifndef __THREADS_H_INCLUDE
#define __THREADS_H_INCLUDE

#define ENABLE_WAIT_STAT
#ifdef DISABLE_WAIT_STAT
    #undef ENABLE_WAIT_STAT
#endif

#include <gb/gb.h>

#define DEFAULT_STACK_SIZE 0

#define CONTEXT_STACK_SIZE 256                 // stack size in bytes, must be sufficent for your threads, set it with care
#define CONTEXT_STACK_SIZE_IN_WORDS (CONTEXT_STACK_SIZE >> 1)
typedef void (* threadproc_t)(void * arg, void * ctx); // prototype of a threadproc()
typedef struct context_t {                     // context of a thread
    unsigned char * task_sp;                   // current stack pointer of a thread
    struct context_t * next;                   // next context
    void * queue;                              // queue of the context
    void * userdata;                           // user data of the context
    UINT8 thread_id;                           // identifier of a thread
    UINT8 terminated;                          // thread termination signal 
    UINT8 finished;                            // thread finished signal
    UINT16 stack[CONTEXT_STACK_SIZE_IN_WORDS]; // context stack size
} context_t;
typedef struct {                               // context of main(): stack is "crt-native"
    unsigned char * task_sp;                   // current stack pointer of main()
    struct context_t * next;                   // next context
} main_context_t;

extern main_context_t main_context;            // this is a main() context  
extern context_t * first_context;              // start of a context chain 

extern void supervisor();
extern void switch_to_thread();

extern void create_thread(context_t * context, int stack_size, threadproc_t threadproc, void * arg);
extern void destroy_thread(context_t * context);

extern context_t * get_thread_by_id(UINT8 id);

extern void terminate_thread(context_t * context);
extern void join_thread(context_t * context); 

typedef UINT8 mutex_t;

inline UINT8 mutex_init(mutex_t * mutex) {
    if (mutex) *mutex = 0xfe; else return 1;
    return 0;
}
extern UINT8 mutex_try_lock(mutex_t * mutex) __preserves_regs(b, c, d);
extern void mutex_lock(mutex_t * mutex) __preserves_regs(b, c, d, e);
extern void mutex_unlock(mutex_t * mutex) __preserves_regs(b, c, d, e);
inline void mutex_destroy(mutex_t * mutex) { mutex; }

#endif