/* hello —— 被 execve 加载的独立 ELF 程序（证明 ELF 装载器活着） */
#include "ulib.h"

void user_main(void) {
    uprint("[hello] ELF exec OK, pid=");
    uprint_dec(sys(2, 0, 0, 0));
    uprint("\n");
    sys(3, 0, 0, 0);
    for (;;) ;
}
