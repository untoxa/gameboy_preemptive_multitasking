#include <gbdk/platform.h>
#include <gbdk/console.h>
#include <gbdk/font.h>
#include <stdio.h>
#include <string.h>
#include <rand.h>

#include "threads.h"

// task1
const unsigned char const sprite[] = {0x00,0x00,0x00,0x3C,0x00,0x66,0x00,0x5E,0x00,0x5E,0x00,0x7E,0x00,0x3C,0x00,0x00};
typedef struct { UBYTE idx, x, dx, y, dy, wait; } sprite_t;
sprite_t sprite1 = {0, 0, 1, 0, 1, 7};
sprite_t sprite2 = {1, 64, 0, 32, 1, 5};
void task1(void * arg, void * ctx) __sdcccall(0) {
    ctx;
    sprite_t * spr = (sprite_t *)arg;
    __critical {
        // these functions are non-reentrant on SMS/GG
        set_sprite_data(spr->idx, 1, sprite);
        set_sprite_tile(spr->idx, 0);
    }
    while (TRUE) {
        if (spr->dx) { 
            spr->x++; if (spr->x >= ((DEVICE_SCREEN_WIDTH - 1) * 8 - 1)) spr->dx = 0; 
        } else { 
            spr->x--; if (spr->x <= 1) spr->dx = 1;             
        }
        if (spr->dy) { 
            spr->y++; if (spr->y >= ((DEVICE_SCREEN_HEIGHT - 1) * 8 - 1)) spr->dy = 0; 
        } else { 
            spr->y--; if (spr->y <= 1) spr->dy = 1;             
        }
        move_sprite(spr->idx, DEVICE_SPRITE_PX_OFFSET_X + spr->x, DEVICE_SPRITE_PX_OFFSET_Y + spr->y);
        delay(spr->wait);
    }
}
context_t task1_1_context, task1_2_context;

// task2
int task2_value = 0;
void task2(void * arg, void * ctx) __sdcccall(0) {
    arg; ctx;
    while (TRUE) { task2_value++; }                // increment as fast as possible
}
context_t task2_context;

// task3
UBYTE task3_value = 0;
void task3(void * arg, void * ctx) __sdcccall(0) {
    arg; ctx;                                        // suppress warning
    while (TRUE) {
        task3_value = rand() & 0x0f;
        switch_to_thread();                     // leave the rest of a quant to other tasks
    }
}
context_t task3_context;

// --- main ---------------------------------------
void main() {
    initrand(73);

    font_init();                                // Initialize font
    font_set(font_load(font_spect));            // Set and load the font

    __critical {
        create_thread(&task1_1_context, DEFAULT_STACK_SIZE, &task1, (void *)&sprite1);
        create_thread(&task1_2_context, DEFAULT_STACK_SIZE, &task1, (void *)&sprite2);
        create_thread(&task2_context,   DEFAULT_STACK_SIZE, &task2, 0);
        create_thread(&task3_context,   DEFAULT_STACK_SIZE, &task3, 0);
    }
        
#if defined(__TARGET_gb) || defined(__TARGET_ap)
    add_TIM(&supervisor);                       // may be any interrupt, but supervisor MUST be the last 
                                                // in the vector, because it never returns (handlers 
                                                // after supervisor() never get called)

    TMA_REG = 0xC0U;                            // nearly 50 hz? not sure
    TAC_REG = 0x04U;
    set_interrupts(VBL_IFLAG | TIM_IFLAG);      // RUN!
#elif defined(__TARGET_sms) || defined(__TARGET_gg)
    add_VBL(&supervisor);                       // we have no timer on SMS/GG
    set_interrupts(VBL_IFLAG); 
#endif

    SHOW_SPRITES;
    
    uint8_t lines = 0;
    while (TRUE) {
        // most of the library is not "thread-safe". for example division/modulus, rand and
        // others. you must watch VERY cerefully, what functions you are calling in your
        // tasks. you may put unsafe parts into __critical { } blocks, but this affects
        // the "smoothness" of parallel execution. it's also turns out tricky to catch the 
        // moments when you can write to vram, because of unexpected task switching. if you 
        // use loops then __critical { } solves the problem
        if (++lines == (DEVICE_SCREEN_HEIGHT - 1)) {
            cls();
            lines = 0;
        }
        // printf() is non-reentrant on SMS/GG, but we have no conflicting calls in the other threads
        printf("t2:0x%x t3:%d\n", task2_value, (int)task3_value);
        delay(100);
    }
}
