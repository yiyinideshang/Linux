#include <stdio.h>
#include <signal.h>
#include <unistd.h>

void sigint_handler(int sig) {
    write(STDOUT_FILENO, "\nCaught SIGINT!\n", 16);
}

int main() {
    struct sigaction sa = { .sa_handler = sigint_handler };
    sigaction(SIGINT, &sa, NULL);

    sigset_t newmask, oldmask;
    sigemptyset(&newmask);
    sigaddset(&newmask, SIGINT);

    sigprocmask(SIG_BLOCK, &newmask, &oldmask);
    printf("SIGINT blocked. You have 5 seconds to press Ctrl+C...\n");
    sleep(5);   // 在这期间按 Ctrl+C，信号被挂起

    printf("Restoring mask...\n");
    sigprocmask(SIG_SETMASK, &oldmask, NULL);   // 解除阻塞，挂起的 SIGINT 立即递送
    // 注意：此处可能不会立即执行下一行，因为信号处理函数会先运行

    printf("Mask restored. Doing another sleep...\n");
    sleep(5);
    return 0;
}