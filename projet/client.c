#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <mqueue.h>

#define _GNU_SOURCE
#define __START__ 0
#define __FINAL__ 1

#define OPERATING 1
#define WAITING 2

#define SIGRT_REQ (SIGRTMIN+1)
#define SIGRT_ANS (SIGRTMIN+2)

#define MAX_REQUESTS 10
#define MAX_PACKETS 10
#define REQUEST_TYPES 3 


double threads_id[4]; // Is the client id (and also the thread id)
unsigned int min_time[4];
unsigned int max_time[4];
unsigned int n_request[4];
unsigned int number_of_packets[4];
pthread_mutex_t lock;
struct request* packet;     // Shared memory segment for requests

struct request{
    unsigned int type_id;
    unsigned int serial_number = 0;
    unsigned int process_time;
};

struct answer{
    unsigned int type_id;
    unsigned int serial_number;
};

struct answer* answers; // Shared memory for answers
sigset_t mask;
union sigval envelope;
struct sigaction descriptor;
const char buffer[sizeof(pid_t)];
pid_t dispatcher_pid;

int priority = 0;
volatile sig_atomic_t states[] = {__START__,__START__, __START__, __START__,__START__,__START__};

unsigned int get_thread_num(double id){
    for(unsigned int i = 0; i < 4; i++){
        if(threads_id[i] == id){
            return i;
        }
    }
    printf("get_thread_num error\n");
}

void* behavior(void* argument){
    unsigned int index = get_thread_num(pthread_self());
    number_of_packets[index] = (rand() % MAX_PACKETS) + 1;
    for(unsigned int i = 0; i < number_of_packets[index]; i++){
        n_request[index] = (rand() % MAX_REQUESTS) + 1;
        send_packet(n_request[index]);
        unsigned int waiting_time = (rand() % max_time[index]) + min_time[index];
        sleep(waiting_time);
    }
    memset(&packet,0,sizeof(packet));
    pthread_exit(EXIT_SUCCESS);
}

void handleAnswer(int signum, siginfo_t* info, void* unused){
    unsigned int index = get_thread_num(pthread_self());
    key_t ipc_key = ftok("guichet.c", 4242);
    if(ipc_key == -1){
        perror("ftok");
        return EXIT_SUCCESS;
    }
    int shmid = shmget(ipc_key, size_of_answers, IPC_CREAT | 0666);
    if(shmid == -1){
        perror("shmget");
        return EXIT_FAILURE;
    }

    answers = (struct answer*) shmat(shmid,NULL,0);
    if(answers == (void*) -1){
        perror("shmat");
    }
    else{
        states[index] == OPERATING;
    }
}

void generate_packet(unsigned int n){
    for(unsigned int i = 0; i < n; i++){
        unsigned type_id = (rand() % REQUEST_TYPES) + 1;
        unsigned int process_time = (rand() % 5) + 1; 
        struct request request = { .type_id = type_id, .serial_number = serial_number, .process_time = process_time};
        packet[i] = request;
    }
}

void send_packet(unsigned int n){
    unsigned int index = get_thread_num(pthread_self());

    states[index] = WAITING;
    pthread_mutex_lock(&lock);    
    states[index] = OPERATING;
    
    generate_packet(n);
    printf("Packet generated\n")
    printf("Number of requests : %u\n", n_request[index]);
    envelope = n; // Put the number of requests contained in the packet
    printf("Warning dispatcher\n");
    sigqueue(dispatcher_pid, SIGRT_REQ, envelope);

    // Waiting for the reception of all the answers.
    states[index] = WAITING;
    sigdelset(&mask, SIGRT_ANS);
    sigprocmask(SIG_SETMASK, &mask, NULL);
    while(states[index] == WAITING){
        pause();
    }

    printf("Answer received\n");
    pthread_mutex_unlock(&lock);
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
        sigprocmask(SIG_SETMASK, &mask, NULL);
        memset(&descriptor,0,sizeof(descriptor));

        descriptor.sa_flags = SA_SIGINFO;

        descriptor.sa_sigaction = handleAnswer;
        sigaction(SIGRT_ANS, &descriptor, NULL);

        // Get the pid of the dispatcher

        mqd_t queue = mq_open("/message_queue", O_CREAT | O_RDONLY);
        if(queue == -1){
            perror("mq_open");
            return EXIT_FAILURE;
        }

        ssize_t amount = mq_receive(queue, buffer, sizeof(pid_t), &priority);
        if(amount == -1){
            perror("mq_receive");
        }
        dispatcher_pid = atoi(buffer);

        if (pthread_mutex_init(&lock, NULL) != 0){
            printf("\n mutex init failed\n");
            return 1;
        } 
       
        pthread_t primary = pthread_self();
        pthread_t first;
        pthread_t secondary;
        pthread_t third;
        pthread_t fourth;

        pthread_create(&first, NULL, behavior, NULL);
        pthread_create(&secondary, NULL, behavior, NULL);
        pthread_create(&third, NULL, behavior, NULL);
        pthread_create(&fourth, NULL, behavior, NULL);
        threads_id[0] = first;
        threads_id[1] = secondary;
        thread_id[2] = third;
        thread_id[3] = fourth;

    }
    pthread_mutex_destroy(&lock);
    shmdt(packet);
    return EXIT_SUCCESS;
}