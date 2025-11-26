
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

void sigchld_handler(int sig) {
    // Пустой обработчик – просто игнорируем сигнал,
    // чтобы родитель НЕ вызывал wait() и зомби оставался.
    (void)sig;
}

int main(void) {
    // Устанавливаем обработчик, который ничего не делает
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {               // дочерний процесс
        printf("Child (PID %d) exiting...\n", getpid());
        _exit(0);                 // завершаемся без вызова atexit
    } else {                      // родительский процесс
        printf("Parent (PID %d) sleeping 10 seconds...\n", getpid());
        sleep(10);                // пока спим, дочерний уже завершён
        // После сна проверяем таблицу процессов
        printf("After sleep, run `ps -o pid,ppid,stat,cmd` to see a zombie.\n");
        // Не вызываем wait() → зомби остаётся
        // Чтобы увидеть его, откройте другой терминал и выполните:
        //   ps -ef | grep Z
        // Затем завершите родителя, чтобы зомби исчез:
        //   kill -TERM %d\n", getpid());
        pause();                  // ждём сигнала (например, Ctrl+C)
    }
    return 0;
}
