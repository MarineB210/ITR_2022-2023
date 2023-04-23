#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
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


volatile sig_atomic_t state_sender = __START__;
sigset_t mask;
union sigval envelope;
struct sigaction descriptor;
int* segment;
sem_t* sema;

unsigned int tab_max_space = TAB_MAX_SIZE * sizeof(signed int);

void handlerSorted(int signum, siginfo_t* info, void* unused){
    state_sender = OPERATING;
}

void send_tab(int* segment){
    printf("Waiting for turn : %ld\n", (long)getpid());
    state_sender = WAITING;

    sem_wait(sema);
    state_sender = OPERATING;
    printf("Semaphore acquired : %ld\n", (long)getpid());
    
    //segment[0] = 15; Might want to use this indeed for better visibility 
    segment[0] = (rand() % TAB_MAX_SIZE) + 1;
    int* tab = segment + sizeof(pid_t) + sizeof(unsigned int);
    printf("Unsorted array : ");
    for(unsigned int i = 0; i < segment[0]; i++){
        if(rand() % 5 == 0){    //Give a chance to the number to be negative
            tab[i] = - rand();  // Reduce the maximum value here by writing -(rand() % (MAX_VALUE + 1))
        }
        else{
            tab[i] = rand();    // Reduce the maximum value here by writing rand() % (MAX_VALUE + 1)
        }
        printf("%i ",tab[i]);
    }

    printf("\nSending array\n");
    sigqueue(segment[1],SIGRT_START,envelope);
    state_sender = EXPECTING;
    sigdelset(&mask, SIGRT_SORTED);
    sigprocmask(SIG_SETMASK, &mask, NULL);
    while(state_sender == EXPECTING)
        pause();
        
    printf("Smaller element : %i\n",tab[0]);
    printf("Tab size : %u\n", segment[0]);
    printf("Sorted array : ");
    for(unsigned int i = 0; i < segment[0]; i++){
        printf("%d ", tab[i]);
    }
    printf("\n");
    state_sender = __FINAL__;
    sem_post(sema);
}

int main(int argc, char* argv[]){
    srand(getpid());
    key_t ipc_key = ftok("Sorter.c", 4242);
    if(ipc_key == -1){
        perror("ftok");
        return EXIT_SUCCESS;
    }
    int shmid = shmget(ipc_key,sizeof(pid_t) + sizeof(unsigned int) + tab_max_space, IPC_CREAT | 0666);
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
        sigprocmask(SIG_SETMASK, &mask, NULL);

        descriptor.sa_flags = SA_SIGINFO;

        descriptor.sa_sigaction = handlerSorted;
        sigaction(SIGRT_SORTED, &descriptor, NULL);

        sema = sem_open("/sema", O_CREAT, 0600, 1);
        send_tab(segment);
    }
    shmdt(segment);
    return EXIT_SUCCESS;
}
