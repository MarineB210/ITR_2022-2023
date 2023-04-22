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

volatile sig_atomic_t state_sender = __START__
sigset_t mask;
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

        descriptor.sa_sigaction = handlerSorted;
        sigaction(SIGRT_SORTED, &descriptor, NULL);

        &segment->sema* = sem_open("/sema", O_CREATE, 0600, 1);
        send_tab(segment);
    }
    shmdt(segment);
    return EXIT_SUCCESS;
}

void handlerSorted(int signum, siginfo_t* info, void* unused){
    state_sender = OPERATING;
}

void* send_tab(signed int* segment){
    state_sender = WAITING;

    sem_wait(&segment->sema);
    state_sender = OPERATING;
    
    &segment->tab_size = (rand() % TAB_MAX_SIZE) + 1;
    &segment->tab[&segment->tab_size];
    for(unsigned int i = 0; i < tab_size; i++){
        &segment->tab[i] = rand(); // Only generate unsigned int
    }

    sigqueue(&segment->pid,SIGRT_START,envelope);
    state_sender = EXPECTING;
    sigdelset(&mask, SIGRT_SORTED);
    sigprocmask(SIG_SETMASK, &mask, NULL);
    while(state_sender == EXPECTING)
        pause();
    
    printf("Smaller element : %i\n",&segment->tab[0]);
    printf("Tab size : %u\n", &segment->tab_size);
    printf("Sorted array : ");
    for(unsigned int i = 0; i < &segment->tab_size; i++){
        printf("%d ", &segment->tab[i]);
    }
    printf("\n")
    state_sender = __FINAL__
    sem_post(&segment->sema);
    exit(EXIT_SUCCESS);
}