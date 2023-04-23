#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>

#define SHM_RDWR 0
#define SEM_SHARED 1
#define TAB_MAX_SIZE 65536

#define __START__ 0
#define EXPECTING 1
#define OPERATING 2
#define __FINAL__ 3

#define SIGRT_SORTED (SIGRTMIN+1)
#define SIGRT_START (SIGRTMIN+2)

int* segment;
volatile unsigned int exit_prog = 1;

volatile sig_atomic_t state_sorter = __START__;
sigset_t mask;
pid_t sender_pid;
union sigval envelope;
struct sigaction descriptor;

unsigned int tab_max_space = TAB_MAX_SIZE * sizeof(signed int);

void handlerStart(int signum, siginfo_t* info, void* unused){
    state_sorter = OPERATING;
    sender_pid = info->si_pid;
    printf("Array to sort received\n");
    int* tab = segment + sizeof(pid_t) + sizeof(unsigned int);
    sort_tab(tab);
}

void handler(int signum, siginfo_t* info, void* unused){
    if(signum == SIGINT){
        exit_prog = 0;
        state_sorter = __FINAL__;
        printf("\nClosing sorter\n");
    }
}

int signedIntComparator(const void* first, const void* second){
    signed int firstInt = * (const signed int*) first;
    signed int secondInt = * (const signed int*) second;
    if(firstInt > secondInt){
        return 1;
    }
    else if(firstInt < secondInt){
        return -1;
    }
    else{
        return 0;
    }
}

void sort_tab(int* tab){
    printf("Sorting the array\n");
    qsort(tab,segment[0],4,signedIntComparator);
    printf("Array sorted, sending signal\n");
    sigqueue(sender_pid,SIGRT_SORTED,envelope);
    state_sorter = EXPECTING;
}


int main(int argc, char* argv[]){
    key_t ipc_key = ftok("Sorter.c", 4242);
    if(ipc_key == -1){
        perror("ftok");
        return EXIT_SUCCESS;
    }

    int shmid = shmget(ipc_key,tab_max_space + sizeof(pid_t) + sizeof(unsigned int), IPC_CREAT | 0666);
    if(shmid == -1){
        perror("shmget");
        return EXIT_FAILURE;
    }

    segment = (int*) shmat(shmid,NULL,0);
    if(segment ==(void*) -1){
        perror("shmat");
    }
    else{
        sigfillset(&mask);
        sigdelset(&mask,SIGINT);
        sigprocmask(SIG_SETMASK, &mask, NULL);
        
        memset(&descriptor,0,sizeof(descriptor));
        descriptor.sa_flags = SA_SIGINFO;

        descriptor.sa_sigaction = handlerStart;
        sigaction(SIGRT_START, &descriptor, NULL);

        descriptor.sa_sigaction = handler;
        sigaction(SIGINT, &descriptor, NULL);

        sem_t* sema = sem_open("/sema", O_CREAT, 0600, 1); //Only used to free it at the end.
        segment[1] = getpid();

        state_sorter = EXPECTING;
        sigdelset(&mask, SIGRT_START);
        sigprocmask(SIG_SETMASK, &mask, NULL);
    }
    while(exit_prog){
        pause();
    }
    shmdt(segment);
    sem_unlink("/sema");
    state_sorter = __FINAL__;
    return EXIT_SUCCESS;
}