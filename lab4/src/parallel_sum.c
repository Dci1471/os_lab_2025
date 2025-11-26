#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <pthread.h>

#include "sum.h"
#include "utils.h"

void *ThreadSum(void *args) {
  struct SumArgs *sum_args = (struct SumArgs *)args;
  return (void *)(size_t)Sum(sum_args);
}

int main(int argc, char **argv) {
  uint32_t threads_num = 0;
  uint32_t array_size = 0;
  uint32_t seed = 0;

  // Парсинг аргументов командной строки
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--threads_num") == 0 && i + 1 < argc) {
      threads_num = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
      seed = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--array_size") == 0 && i + 1 < argc) {
      array_size = atoi(argv[++i]);
    } else {
      fprintf(stderr, "Usage: %s --threads_num <num> --seed <num> --array_size <num>\n", argv[0]);
      return 1;
    }
  }

  // Проверка валидности аргументов
  if (threads_num == 0 || array_size == 0) {
    fprintf(stderr, "Error: All parameters must be positive integers\n");
    fprintf(stderr, "Usage: %s --threads_num <num> --seed <num> --array_size <num>\n", argv[0]);
    return 1;
  }

  // Выделение памяти для массива
  int *array = malloc(sizeof(int) * array_size);
  if (!array) {
    fprintf(stderr, "Error: Memory allocation failed for array\n");
    return 1;
  }

  // Генерация массива с использованием функции из utils.h (не входит в замер времени)
  GenerateArray(array, array_size, seed);

  // Подготовка аргументов для потоков
  pthread_t threads[threads_num];
  struct SumArgs args[threads_num];
  
  int segment_size = array_size / threads_num;
  int remainder = array_size % threads_num;
  int current_start = 0;

  for (uint32_t i = 0; i < threads_num; i++) {
    args[i].array = array;
    args[i].begin = current_start;
    
    // Распределяем остаток по первым потокам
    int current_end = current_start + segment_size + (i < remainder ? 1 : 0);
    args[i].end = (current_end > array_size) ? array_size : current_end;
    
    current_start = current_end;
  }

  // Замер времени начала вычисления суммы
  struct timespec start_time, end_time;
  clock_gettime(CLOCK_MONOTONIC, &start_time);

  // Создание потоков
  for (uint32_t i = 0; i < threads_num; i++) {
    if (pthread_create(&threads[i], NULL, ThreadSum, (void *)&args[i])) {
      fprintf(stderr, "Error: pthread_create failed!\n");
      free(array);
      return 1;
    }
  }

  // Ожидание завершения потоков и сбор результатов
  int total_sum = 0;
  for (uint32_t i = 0; i < threads_num; i++) {
    void *thread_result;
    if (pthread_join(threads[i], &thread_result)) {
      fprintf(stderr, "Error: pthread_join failed!\n");
      free(array);
      return 1;
    }
    total_sum += (int)(size_t)thread_result;
  }

  // Замер времени окончания вычисления суммы
  clock_gettime(CLOCK_MONOTONIC, &end_time);

  // Вычисление времени выполнения
  double elapsed_time = (end_time.tv_sec - start_time.tv_sec) + 
                       (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

  // Вывод результатов
  printf("Total sum: %d\n", total_sum);
  printf("Elapsed time: %.6f seconds\n", elapsed_time);
  printf("Array size: %u\n", array_size);
  printf("Threads number: %u\n", threads_num);
  printf("Seed: %u\n", seed);

  free(array);
  return 0;
}