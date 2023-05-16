#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <semaphore.h>
#include <mqueue.h>


#define __START__ 0
#define __FINAL__ 1

#define OPERATING 1
#define WAITING 2

#define SIGRT_REQ (SIGRTMIN+1)
#define SIGRT_ANS (SIGRTMIN+2)

unsigned int id;
unsigned int min_time;
unsigned int max_time;
unsigned int n_request;

struct request{
    unsigned int type_id;
    unsigned int serial_number;
    unsigned int process_time;
};

struct request* packets;    //For future use, when client can send multiple packets
struct request* packet;     //shared memory segment for requests
sem_t* sema;
sigset_t mask;
union sigval envelope;
struct sigaction descriptor;
const char buffer[1024];
pid_t dispatcher_pid;

int priority = 0;
volatile sig_atomic_t state = __START__;

void handlerAnswer(int signum, siginfo_t* info, void* unused){
    state = OPERATING;
}

void generate_packet(unsigned int n){
    for(unsigned int i = 0; i < n; i++){
        //TO-DO : Assigne a type_id, serial_number and process_time to requests
        unsigned type_id = ;
        unsigned int serial_number = ;
        unsigned int process_time = ;
        struct request request = { .type_id = type_id, .serial_number = serial_number, .process_time = process_time};
        packet[i] = request;
    }
}

void send_packet(unsigned int n){
    state = WAITING;
    sem_wait(sema);
    state = OPERATING;
    generate_packet(n);
    printf("Packet sent\n")
    sigqueue(dispatcher_pid, SIGRT_REQ, envelope);

    state = WAITING;
    sigdelset(&mask, SIGRT_ANS);
    sigprocmask(SIG_SETMASK, &mask, NULL);
    while(state == WAITING){
        pause();
    }
    printf("Answer received\n");
    sem_post(sema);
}

int main(int argc, char* argv[]){
    srand(getpid());
    unsigned int number_of_request = 4;
    unsigned int size_of_packet = sizeof(struct request) * number_of_request;
    key_t ipc_key = ftok("client.c", 4242);
    if(ipc_key == -1){
        perror("ftok");
        return EXIT_SUCCESS;
    }
    int shmid = shmget(ipc_key,size_of_packet, IPC_CREAT | 0666);
    if(shmid == -1){
        perror("shmget");
        return EXIT_FAILURE;
    }

    packet = (struct request*) shmat(shmid,NULL,0);
    if(packet == (void*) -1){
        perror("shmat");
    }
    else{
        sigfillset(&mask);
        sigprocmask(SIG_SETMASK, &mask, NULL);
        memset(&descriptor,0,sizeof(descriptor));

        descriptor.sa_flags = SA_SIGINFO;

        descriptor.sa_sigaction = handlerAnswer;
        sigaction(SIGRT_ANS, &descriptor, NULL);

        // Get the pid of the dispatcher

        mqd_t queue = mq_open("/message_queue", O_CREAT | O_RDONLY);
        if(queue == -1){
            perror("mq_open");
            return EXIT_FAILURE;
        }

        ssize_t amount = mq_receive(queue, buffer, 1024, &priority);
        if(amount == -1){
            perror("mq_receive");
        }

        dispatcher_pid = atoi(buffer);
        sema = sem_open("/sema", O_CREAT, 0600, 1);
        //TO-DO: generate multiple packets to send with waiting time
        send_packet(number_of_request);
    }
    mq_unlink("/message_queue");
    shmdt(packet);
    return EXIT_SUCCESS;
}