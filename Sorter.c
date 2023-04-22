#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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
#define WAITING 3
#define __FINAL__ 4

#define SIGRT_SORTED (SIGRTMIN+1)
#define SIGRT_START (SIGRTMIN+2)

struct array* segment;

volatile sig_atomic_t state_sorter = __START__
sigset_t mask;
pid_t sender_pid;
union sigval envelope;
struct sigaction descriptor;

struct array{
    sem_t sema;
    pid_t pid;
    signed int* tab;
    unsigned int tab_size;
}

int main(int argc, char* argv[]){
    key_t ipc_key = ftok("application-shm", 4242);
    if(ipc_key == -1){
        perror("ftok");
        return EXIT_SUCCESS;
    }

    int shmid = shmget(ipc_key,TAB_MAX_SIZE + 4 + 4 + sizeof(sem_t), IPC_CREAT | 0666);
    if(shmid == -1){
        perror("shmget");
        return EXIT_FAILURE;
    }

    segment = shmat(shmid,NULL,0);
    if(segment ==(void*) -1){
        perror("shmat");
    }
    else{
        sigfillset(&mask);
        sigprocmask(SIG_SETMASK, &mask, NULL);

        descriptor.sa_flags = SA_SIGINFO;

        descriptor.sa_sigaction = handlerStart;
        sigaction(SIGRT_START, &descriptor, NULL);

        descriptor.sa_sigaction = handler;
        sigaction(SIGINT, &descriptor, NULL);

        &segment->sema* = sem_open("/sema", O_CREATE, 0600, 1);
        &segment->pid = getpid();

        state_sorter = EXPECTING;
        sigdelset(&mask, SIGRT_START);
        sigprocmask(SIG_SETMASK, &mask, NULL);
    }
    while(1){
        pause();
    }
    shmctl(segment);
    sem_unlink("/sema");
    state_sorter = __FINAL__;
    return EXIT_SUCCESS;
}

void handlerStart(int signum, siginfo_t* info, void* unused){
    state_sorter = OPERATING;
    sender_pid = info->si_pid;
    sort_tab();
}

void handler(int signum, siginfo_t* info, void* unused){
    if(signum == SIGINT){
        shmctl(segment);
        sem_unlink("/sema");
        state_sorter = __FINAL__;
        exit(EXIT_SUCCESS);
    }
}

void* sort_tab(){
    qsort(&segment->tab,&segment->tab_size,4,signedIntComparator);
    sigqueue(sender_pid,SIGRT_SORTED,envelope);
    state_sorter = EXPECTING;
}


int signedIntComparator(const void* first, const void* second){
    signed int firstInt = * (const signed int*) first;
    signed int secondInt = * (const signed int*) second;
    return firstInt - secondInt;
}