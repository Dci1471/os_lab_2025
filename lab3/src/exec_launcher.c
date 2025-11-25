#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <seed> <array_size>\n", argv[0]);
        return 1;
    }

    char *seed = argv[1];
    char *array_size = argv[2];

    // Создаем дочерний процесс
    pid_t child_pid = fork();
    
    if (child_pid == -1) {
        perror("fork failed");
        return 1;
    }
    else if (child_pid == 0) {
        // ДОЧЕРНИЙ ПРОЦЕСС
        printf("Child process: Preparing to execute sequential_min_max...\n");
        
        // Аргументы для sequential_min_max
        char *args[] = {"./sequential_min_max", seed, array_size, NULL};
        
        // Заменяем текущий процесс на sequential_min_max
        execvp("./sequential_min_max", args);
        
        // Если дошли сюда, значит execvp failed
        perror("execvp failed");
        exit(1);
    }
    else {
        // РОДИТЕЛЬСКИЙ ПРОЦЕСС
        // printf("Parent process: PID = %d\n", getpid());
        // printf("Parent process: Created child process with PID = %d\n", child_pid);
        // printf("Parent process: Waiting for child to complete...\n\n");
        
        int status;
        // Ждем завершения дочернего процесса
        waitpid(child_pid, &status, 0);
        
        if (WIFEXITED(status)) {
            printf("Parent process: Child exited with status %d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Parent process: Child terminated by signal %d\n", WTERMSIG(status));
        }
    }
    
    return 0;
}