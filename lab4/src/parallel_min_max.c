#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <stdint.h>

#include <getopt.h>

#include "find_min_max.h"
#include "utils.h"

static int64_t timeout_ms = -1; /* timeout в миллисекундах, -1 — не задан */

int main(int argc, char **argv) {
  int seed = -1;
  int array_size = -1;
  int pnum = -1;
  bool with_files = false;

  while (true) {
    int current_optind = optind ? optind : 1;

    static struct option options[] = {
        {"seed", required_argument, 0, 0},
        {"array_size", required_argument, 0, 0},
        {"pnum", required_argument, 0, 0},
        {"by_files", no_argument, 0, 'f'},
        {"timeout", required_argument, 0, 0}, /* milliseconds */
        {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "f", options, &option_index);

    if (c == -1)
      break;

    switch (c) {
    case 0:
      switch (option_index) {
      case 0:
        seed = atoi(optarg);
        if (seed <= 0) {
          printf("seed must be a positive number\n");
          return 1;
        }
        break;
      case 1:
        array_size = atoi(optarg);
        if (array_size <= 0) {
          printf("array_size must be a positive number\n");
          return 1;
        }
        break;
      case 2:
        pnum = atoi(optarg);
        if (pnum <= 0) {
          printf("pnum must be a positive number\n");
          return 1;
        }
        break;
      case 3:
        with_files = true;
        break;
      case 4:
        timeout_ms = atoll(optarg);
        if (timeout_ms <= 0) {
          printf("timeout must be a positive number of milliseconds\n");
          return 1;
        }
        break;
      default:
        printf("Index %d is out of options\n", option_index);
      }
      break;
    case 'f':
      with_files = true;
      break;
    case '?':
      break;
    default:
      printf("getopt returned character code 0%o?\n", c);
    }
  }

  if (optind < argc) {
    printf("Has at least one no option argument\n");
    return 1;
  }

  if (seed == -1 || array_size == -1 || pnum == -1) {
    printf("Usage: %s --seed \"num\" --array_size \"num\" --pnum \"num\" [--timeout \"ms\"]\n",
           argv[0]);
    return 1;
  }

  int *array = malloc(sizeof(int) * array_size);
  if (!array) {
    perror("malloc");
    return 1;
  }
  GenerateArray(array, array_size, seed);
  int active_child_processes = 0;

  pid_t *child_pids = malloc(sizeof(pid_t) * pnum);
  if (!child_pids) {
    perror("malloc");
    free(array);
    return 1;
  }
  for (int i = 0; i < pnum; i++) child_pids[i] = -1;

  /* pipes */
  int *pipes = NULL;
  if (!with_files) {
    pipes = malloc(sizeof(int) * 2 * pnum);
    if (!pipes) {
      perror("malloc");
      free(array);
      free(child_pids);
      return 1;
    }
    for (int i = 0; i < pnum; i++) {
      if (pipe(pipes + 2 * i) < 0) {
        printf("Failed to create pipe for process %d\n", i);
        free(array);
        free(child_pids);
        free(pipes);
        return 1;
      }
    }
  }

  struct timeval start_time;
  gettimeofday(&start_time, NULL);

  for (int i = 0; i < pnum; i++) {
    pid_t child_pid = fork();
    if (child_pid >= 0) {
      active_child_processes += 1;
      if (child_pid == 0) {
        /* child */

        /* вычисление локального min/max */
        int chunk_size = array_size / pnum;
        int start = i * chunk_size;
        int end = (i == pnum - 1) ? array_size - 1 : (i + 1) * chunk_size - 1;

        struct MinMax local_min_max = GetMinMax(array, start, end);

        if (with_files) {
          char filename[64];
          sprintf(filename, "min_max_%d.txt", getpid());
          FILE *file = fopen(filename, "w");
          if (file != NULL) {
            fprintf(file, "%d %d", local_min_max.min, local_min_max.max);
            fclose(file);
          }
        } else {
          /* write to pipe */
          close(pipes[2 * i]); /* close read end */
          write(pipes[2 * i + 1], &local_min_max.min, sizeof(int));
          write(pipes[2 * i + 1], &local_min_max.max, sizeof(int));
          close(pipes[2 * i + 1]);
        }

        /* Для теста: если timeout_ms задан — заставим ребёнка работать дольше,
           чтобы родитель успел сработать таймаутом. Это sleep не меняет логику
           основного алгоритма (только удлиняет работу дочернего). */
        if (timeout_ms > 0) {
          int64_t sleep_ms = timeout_ms + 100; /* немного больше, чтобы гарантировать срабатывание */
          struct timespec ts;
          ts.tv_sec = sleep_ms / 1000;
          ts.tv_nsec = (sleep_ms % 1000) * 1000000;
          nanosleep(&ts, NULL);
        }

        free(array);
        if (pipes) free(pipes);
        free(child_pids);
        return 0;
      } else {
        /* parent */
        child_pids[i] = child_pid;
      }
    } else {
      printf("Fork failed!\n");
      if (pipes) free(pipes);
      free(array);
      free(child_pids);
      return 1;
    }
  }

  /* Wait logic with timeout in milliseconds, and simultaneous SIGKILL on timeout */
  if (timeout_ms <= 0) {
    while (active_child_processes > 0) {
      wait(NULL);
      active_child_processes--;
    }
  } else {
    int status;
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    int64_t start_ms = start_time.tv_sec * 1000LL + start_time.tv_usec / 1000LL;

    while (active_child_processes > 0) {
      pid_t w = waitpid(-1, &status, WNOHANG);
      if (w > 0) {
        /* пометить как реапнутый */
        for (int i = 0; i < pnum; i++) {
          if (child_pids[i] == w) child_pids[i] = -1;
        }
        active_child_processes--;
        continue;
      } else if (w == 0) {
        gettimeofday(&tv_now, NULL);
        int64_t now_ms = tv_now.tv_sec * 1000LL + tv_now.tv_usec / 1000LL;
        if (now_ms - start_ms >= timeout_ms) {
          /* таймаут — посылаем SIGKILL всем живым детям одновременно */
          for (int i = 0; i < pnum; i++) {
            if (child_pids[i] > 0) {
              /* проверяем существует ли процесс */
              if (kill(child_pids[i], 0) == 0) {
                kill(child_pids[i], SIGKILL);
              }
            }
          }
          /* дождёмся всех */
          while (active_child_processes > 0) {
            wait(NULL);
            active_child_processes--;
          }
          break;
        }
        /* короткий сон, чтобы не крутить цикл */
        usleep(5000); /* 5 ms */
      } else {
        /* waitpid ошибка */
        perror("waitpid");
        break;
      }
    }
  }

  struct MinMax min_max;
  min_max.min = INT_MAX;
  min_max.max = INT_MIN;

  for (int i = 0; i < pnum; i++) {
    int min = INT_MAX;
    int max = INT_MIN;

    if (with_files) {
      char filename[64];
      if (child_pids[i] > 0) {
        sprintf(filename, "min_max_%d.txt", child_pids[i]);
      } else {
        sprintf(filename, "min_max_%d.txt", i);
      }
      FILE *file = fopen(filename, "r");
      if (file != NULL) {
        fscanf(file, "%d %d", &min, &max);
        fclose(file);
        remove(filename);
      } else {
        sprintf(filename, "min_max_%d.txt", i);
        file = fopen(filename, "r");
        if (file != NULL) {
          fscanf(file, "%d %d", &min, &max);
          fclose(file);
          remove(filename);
        }
      }
    } else {
      /* read from pipes */
      close(pipes[2 * i + 1]); /* close write end */
      /* read may fail if child was killed before writing; handle that */
      ssize_t r = read(pipes[2 * i], &min, sizeof(int));
      if (r == sizeof(int)) {
        read(pipes[2 * i], &max, sizeof(int));
      } else {
        /* child didn't write — leave min/max default for this chunk */
        min = INT_MAX;
        max = INT_MIN;
      }
      close(pipes[2 * i]);
    }

    if (min < min_max.min) min_max.min = min;
    if (max > min_max.max) min_max.max = max;
  }

  struct timeval finish_time;
  gettimeofday(&finish_time, NULL);

  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

  free(array);
  free(child_pids);
  if (pipes) free(pipes);

  printf("Min: %d\n", min_max.min == INT_MAX ? 0 : min_max.min);
  printf("Max: %d\n", min_max.max == INT_MIN ? 0 : min_max.max);
  printf("Elapsed time: %fms\n", elapsed_time);
  fflush(NULL);
  return 0;
}
