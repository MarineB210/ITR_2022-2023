#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <mqueue.h>
#include <semaphore.h>


#define CLOSED 0 
#define OPEN 1
#define OPERATING 3
#define __START__ 4
#define __FINAL__ 5

#define SIGRT_REQ (SIGRTMIN+1)
#define SIGRT_ANS (SIGRTMIN+2)

struct request{
    unsigned int type_id;
    unsigned int serial_number;
    unsigned int process_time;
};

struct answer{
    unsigned int type_id;
    unsigned int serial_number;
};



struct request* packet;     //shared memory segment for requests
sem_t* sema;
sigset_t mask;
union sigval envelope;
struct sigaction descriptor;

volatile sig_atomic_t state = __START__;

int main(int argc, char* argv){
    srand(getpid());
    mqd_t queue = mq_open("/message_queue", O_WRONLY);
    key_t ipc_key = ftok("client.c", 4242);
    if(ipc_key == -1){
        perror("ftok");
        return EXIT_SUCCESS;
    }
    int shmid = shmget(ipc_key,segment_size , IPC_CREAT | 0666);
    if(shmid == -1){
        perror("shmget");
        return EXIT_FAILURE;
    }

    packet = (struct request*) shmat(shmid,NULL,0);
    if(packet ==(void*) -1){
        perror("shmat");
    }
    else{
        sigfillset(&mask);
        sigprocmask(SIG_SETMASK, &mask, NULL);
        memset(&descriptor,0,sizeof(descriptor));

        descriptor.sa_flags = SA_SIGINFO;

        descriptor.sa_sigaction = handleRequest;
        sigaction(SIGRT_REQ, &descriptor, NULL);

        descriptor.sa_sigaction = handleAnswer;
        sigaction(SIGRT_ANS, &descriptor, NULL);

        sigdelset(&mask, SIGRT_READY);
        sigprocmask(SIG_SETMASK, &mask, NULL);

        // First we give out the pid of our dispatcher to our clients using mqueue
        if(queue == -1){
            perror("mq_open");
            return EXIT_FAILURE;
        }

        int status = mq_send(queue,getpid(),1024,0);
        if(status == -1){
            perror("mq_send");
        }

        sema = sem_open("/sema", O_CREAT, 0600, 1);
    }
    mq_close(queue);
    shmdt(segment);
}