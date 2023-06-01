#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <mqueue.h>
#include <semaphore.h>

#define _GNU_SOURCE
#define __START__ 0
#define WAITING 1
#define OPERATING 2
#define __FINAL__ 3

#define SIGRT_REQ (SIGRTMIN+1)
#define SIGRT_ANS (SIGRTMIN+2)
#define SIGRT_PING (SIGRTMIN+3)
#define SIGRT_OK (SIGRTMIN+4)


#define REQUEST_TYPES 3 


double threads_id[6];
unsigned int type_id[6];

struct request{
    unsigned int type_id;
    unsigned int serial_number;
    unsigned int process_time;
};

struct answer{
    unsigned int type_id;
    unsigned int serial_number;
};


sem_t* sema;
sigset_t mask;
union sigval envelope;
struct sigaction descriptor;
pid_t dispatcher_pid;
const char buffer[1024];
mqd_t queue;
struct request request;


volatile sig_atomic_t states[] = {__START__,__START__, __START__, __START__,__START__,__START__};

unsigned int get_thread_num(double id){
    for(unsigned int i = 0; i < 4; i++){
        if(threads_id[i] == id){
            return i;
        }
    }
    printf("get_thread_num error\n");
}

void* behaviorType1(void* argument){
    // Send confirmation to the dispatcher that its pid was correctly received
    unsigned int index = get_thread_num(pthread_self());
    type_id[index] = 1;
    envelope = type_id;
    sigqueue(dispatcher_pid, SIGRT_PING, envelope);

    // Ready to receive requests
    states[index] = WAITING;
    sigdelset(SIGRT_REQ, &mask);
    sigprocmask(SIG_SETMASK, &mask, NULL);
    while(1){
        pause();
    }
    
}

void* behaviorType2(void* argument){
    // Send confirmation to the dispatcher that its pid was correctly received
    unsigned int index = get_thread_num(pthread_self());
    type_id[index] = 2;
    envelope = type_id;
    sigqueue(dispatcher_pid, SIGRT_PING, envelope);

    // Ready to receive requests
    states[index] = WAITING;
    sigdelset(SIGRT_REQ, &mask);
    sigprocmask(SIG_SETMASK, &mask, NULL);
    while(1){
        pause();
    }
    
}

void* behaviorType3(void* argument){
    // Send confirmation to the dispatcher that its pid was correctly received
    unsigned int index = get_thread_num(pthread_self());
    type_id[index] = 3;
    envelope = type_id;
    sigqueue(dispatcher_pid, SIGRT_PING, envelope);

    // Ready to receive requests
    states[index] = WAITING;
    sigdelset(SIGRT_REQ, &mask);
    sigprocmask(SIG_SETMASK, &mask, NULL);
    while(1){
        pause();
    }
    
}

void handleOK(int signum, siginfo_t* info, void* unused){
    unsigned int index = get_thread_num(pthread_self());
    sema_post(sema);
    states[index] == WAITING;
}

void handleRequest(int signum, siginfo_t* info, void* unused){
    ssize_t amount = mq_receive(queue, request, sizeof(struct request), &priority);
    if(amount == -1){
        perror("mq_receive");
    }
    sigqueue(dispatcher_pid, SIGRT_OK, envelope);
    sem_wait(sema);
    
    // Process time of the request.
    sleep(request.process_time);

    struct answer answer = {.type_id = request.type_id, .serial_number = request.serial_number};
    int status = mq_send(queue, answer, sizeof(struct answer),0);
    if(status == -1){
        perror("mq_send");
    }
    sigqueue(dispatcher_pid, SIGRT_ANS, envelope);

}


int main(int argc, char* argv[]){
    srand(getpid());
    sigfillset(&mask);
    sigprocmask(SIG_SETMASK, &mask, NULL);
    memset(&descriptor,0,sizeof(descriptor));

    descriptor.sa_flags = SA_SIGINFO;

    descriptor.sa_sigaction = handleRequest;
    sigaction(SIGRT_REQ, &descriptor, NULL);

    descriptor.sa_sigaction = handleOK;
    sigaction(SIGRT_OK, &descriptor, NULL);

    sema = sem_open("/memory", O_CREAT, 0600, 1);

    mqd_t queue = mq_open("/message_queue", O_CREAT);
    if(queue == -1){
        perror("mq_open");
        return EXIT_FAILURE;
    }

    dispatcher_pid = atoi(buffer);
    
    pthread_t primary = pthread_self();
    pthread_t first;
    pthread_t secondary;
    pthread_t third;
    pthread_t fourth;
    pthread_t fifth;
    pthread_t sixth;

    pthread_create(&first, NULL, behaviorType3, NULL);
    pthread_create(&secondary, NULL, behaviorType1, NULL);
    pthread_create(&third, NULL, behaviorType2, NULL);
    pthread_create(&fourth, NULL, behaviorType3, NULL);
    pthread_create(&fifth, NULL, behaviorType1, NULL);
    pthread_create(&sixth, NULL, behaviorType2, NULL);

    threads_id[0] = first;
    threads_id[1] = secondary;
    threads_id[2] = third;
    threads_id[3] = fourth;
    threads_id[4] = fifth;
    threads_id[5] = sixth;

    while(1){
        pause();
    }
    mq_unlink("/message_queue");
}