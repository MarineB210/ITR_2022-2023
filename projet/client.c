#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <mqueue.h>
#include <semaphore.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define _GNU_SOURCE
#define __START__ 0
#define __FINAL__ 1

#define OPERATING 1
#define WAITING 2

#define SIGRT_REQ (SIGRTMIN+1)
#define SIGRT_ANS (SIGRTMIN+2)

#define MAX_REQUESTS 10
#define MAX_PACKETS 10
#define MAX_TIME 10
#define MIN_TIME 1
#define REQUEST_TYPES 3 


struct request{
    unsigned int type_id;
    unsigned int serial_number;
    unsigned int process_time;
};

struct answer{
    unsigned int type_id;
    unsigned int serial_number;
};


unsigned int n_request;
unsigned int number_of_packets;
struct request* packet;     // Shared memory for requests
struct answer* answers; // Shared memory for answers
sigset_t mask;
union sigval envelope;
struct sigaction descriptor;
char buffer[10];
pid_t dispatcher_pid;
mqd_t queue;

unsigned int exit_prog = 1;
int priority = 0;
volatile sig_atomic_t state = __START__;


/*
 Generate the requests in a packet
*/
void generate_packet(unsigned int n){
    for(unsigned int i = 0; i < n; i++){
        unsigned type_id = (rand() % REQUEST_TYPES) + 1;
        unsigned int process_time = (rand() % 5) + 1; 
        struct request request = { .type_id = type_id, .serial_number = 0, .process_time = process_time};
        packet[i] = request;
    }
}

/*
 Send a packet to dispatcher. 
 To do so, the different requests are put in a shared memory and a signal is
 sent to the dispatcher to warn it that about them.
*/
void send_packet(unsigned int n){

    state = OPERATING;
    
    generate_packet(n);
    printf("Packet generated\n");
    printf("Number of requests : %u\n", n_request);
    envelope.sival_int = n; // Put the number of requests contained in the packet
    printf("Warning dispatcher\n");
    sigqueue(dispatcher_pid, SIGRT_REQ, envelope);

    // Waiting for the reception of all the answers.
    state = WAITING;
    sigdelset(&mask, SIGRT_ANS);
    sigprocmask(SIG_SETMASK, &mask, NULL);
    while(state == WAITING){
        pause();
    }
    
    printf("Answers received\n");
}

/*
Mainly used to make sure that everything is terminated correctly
*/
void handler(int signum, siginfo_t* info, void* unused){
    if(signum == SIGINT){
        exit_prog = 0;
        printf("\nClosing\n");
    }
}


/*
Behavior of the client.
Generate a number of packets that are going to be sent over to the dispatcher.
*/
void clientBehavior(){
    //number_of_packets = (rand() % MAX_PACKETS) + 1;
    number_of_packets = 2;
    printf("Number of packet to send : %u\n", number_of_packets);
    for(unsigned int i = 0; i < number_of_packets; i++){
        //n_request = (rand() % MAX_REQUESTS) + 1;
        n_request = 4;
        send_packet(n_request);
        unsigned int waiting_time = (rand() % MAX_TIME) + MIN_TIME;
        sleep(waiting_time);
    }
    memset(&packet,0,sizeof(packet));
    handler(SIGINT,NULL,NULL);
}

/*
When all the answers have been collected by the dispatcher, it sends
a signal to the client to warn him. 
*/
void handleAnswer(int signum, siginfo_t* info, void* unused){
    unsigned int size_of_answers = sizeof(struct answer) * MAX_REQUESTS;
    key_t ipc_key = ftok("guichet.c", 4242);
    if(ipc_key == -1){
        perror("ftok");
    }
    int shmid = shmget(ipc_key, size_of_answers, IPC_CREAT | 0666);
    if(shmid == -1){
        perror("shmget");
    }

    answers = (struct answer*) shmat(shmid,NULL,0);
    if(answers == (void*) -1){
        perror("shmat");
    }
    else{
        state = OPERATING;
    }
}


int main(int argc, char* argv[]){
    srand(getpid());

    unsigned int size_of_packet = sizeof(struct request) * MAX_REQUESTS;
    key_t ipc_key = ftok("client.c", 4242);
    if(ipc_key == -1){
        perror("ftok");
        return EXIT_SUCCESS;
    }
    int shmid = shmget(ipc_key, size_of_packet, IPC_CREAT | 0666);
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
        sigdelset(&mask, SIGINT);
        sigprocmask(SIG_SETMASK, &mask, NULL);
        memset(&descriptor,0,sizeof(descriptor));

        descriptor.sa_flags = SA_SIGINFO;

        descriptor.sa_sigaction = handler;
        sigaction(SIGINT, &descriptor, NULL);

        descriptor.sa_sigaction = handleAnswer;
        sigaction(SIGRT_ANS, &descriptor, NULL);

        // Get the pid of the dispatcher

        queue = mq_open("/dispatcher",O_RDONLY);
        if(queue == -1){
            perror("mq_open");
            return EXIT_FAILURE;
        }

        ssize_t amount = mq_receive(queue, buffer, 8192, &priority);

        if(amount == -1){
            perror("mq_receive");
        }

        dispatcher_pid = atoi(buffer);
        printf("Clients received dispatcher's pid : %i\n",dispatcher_pid);
        
        clientBehavior();
        
        while(exit_prog){
            pause();
        }
        
    }
    printf("Process client ended\n");
    mq_close(queue);
    shmdt(packet);
    shmdt(answers);
    return EXIT_SUCCESS;
}
