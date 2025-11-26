CC=gcc
CFLAGS=-I.

all : zombie_process parallel_min_max process_memory parallel_sum
	@echo "Сборка всех программ"

parallel_min_max : utils.o find_min_max.o utils.h find_min_max.h
	$(CC) -o parallel_min_max utils.o find_min_max.o parallel_min_max.c $(CFLAGS)

parallel_sum : parallel_sum.c utils.o sum.o
	$(CC) -o parallel_sum utils.o parallel_sum.c sum.o $(CFLAGS)

sum.o : sum.h
	$(CC) -o sum.o -c sum.c $(CFLAGS)

utils.o : utils.h
	$(CC) -o utils.o -c utils.c $(CFLAGS)

zombie_process : zombie_process.c
	$(CC) -o zombie_process zombie_process.c $(CFLAGS)

find_min_max.o : utils.h find_min_max.h
	$(CC) -o find_min_max.o -c find_min_max.c $(CFLAGS)

process_memory : process_memory.c
	$(CC) -o process_memory process_memory.c $(CFLAGS)

clean :
	rm utils.o find_min_max.o  parallel_min_max zombie_process process_memory sum.o parallel_sum
