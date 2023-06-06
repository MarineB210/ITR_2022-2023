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
#define WAITING 1
#define OPERATING 2
#define DONE 3
#define __FINAL__ 4

#define SIGRT_REQ (SIGRTMIN+1)
#define SIGRT_ANS (SIGRTMIN+2)
#define SIGRT_PING (SIGRTMIN+3)
#define SIGRT_OK (SIGRTMIN+4)
#define SIGRT_START (SIGRTMIN+5)


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

struct request requests_type_1[2];
struct request requests_type_2[2];
struct request requests_type_3[2];

sem_t* sema;
sigset_t mask;
union sigval envelope;
struct sigaction descriptor;
pid_t dispatcher_pid;
char buffer[10];
mqd_t queue;

unsigned int exit_prog = 1;
unsigned int counter = 0;
int priority = 0;

volatile sig_atomic_t state = __START__;

void handler(int signum, siginfo_t* info, void* unused){
    if(signum == SIGINT){
        exit_prog = 0;
        printf("\nClosing\n");
    }
}


void handleOK(int signum, siginfo_t* info, void* unused){
    state = DONE;
}


/*
    Put the requests received in array based on their type id.
*/
int putInQueue(struct request request){
    unsigned int type_id = request.type_id;
    if(type_id == 1){
        if(requests_type_1[0].type_id == 0){
            requests_type_1[0] = request;
            return 1;
        }
        else if(requests_type_1[1].type_id == 0){
            requests_type_1[1] = request;
            return 1;
        }
        else{
            printf("Erreur\n");
            return 0;
        }
    }
    else if(type_id == 2){
        if(requests_type_2[0].type_id == 0){
            requests_type_2[0] = request;
            return 1;
        }
        else if(requests_type_2[1].type_id == 0){
            requests_type_2[1] = request;
            return 1;
        }
        else{
            printf("Erreur\n");
            return 0;
        }
    }
    else if(type_id == 3){
        if(requests_type_3[0].type_id == 0){
            requests_type_3[0] = request;
            return 1;
        }
        else if(requests_type_3[1].type_id == 0){
            requests_type_3[1] = request;
            return 1;
        }
        else{
            printf("Erreur\n");
            return 0;
        }
    } 
}


void sendAnswer(struct request* requests){
    for(unsigned int i = 0; i < 2; i++){
        
        state = OPERATING;
        if(requests[i].type_id != 0){
            // Process time of the request.
            printf("Dealing with request %u\n",requests[i].serial_number);
            printf("Processing time : %u\n",requests[i].process_time);
            sleep(requests[i].process_time);

            struct answer answer = {.type_id = requests[i].type_id, .serial_number = requests[i].serial_number};
            int status = mq_send(queue, (const char *) &answer, sizeof(struct answer),0);

            if(status == -1){
                perror("mq_send");
            }
            sigdelset(&mask, SIGRT_OK);
            sigprocmask(SIG_SETMASK, &mask, NULL);
            sigqueue(dispatcher_pid, SIGRT_ANS, envelope);
            state = WAITING;
            
            while(state == WAITING){
                pause();
            } 
            
            requests[i].type_id = 0;
            sigaddset(&mask, SIGRT_OK);
            sigprocmask(SIG_SETMASK, &mask, NULL);
        }
    }
}


/*
    START signal is received when all the requests the dispatcher could send were sent.
    The guichet begin the treatment of the requests.
*/
void handleStart(int signum, siginfo_t* info, void* unused){
    printf("All requests have been received\n");
    sem_wait(sema);
    
    sendAnswer(requests_type_1);
    sendAnswer(requests_type_2);
    sendAnswer(requests_type_3);
    
    sem_post(sema);
}


void handleRequest(int signum, siginfo_t* info, void* unused){
    
    state = OPERATING;
    
    struct request request;
    ssize_t amount = mq_receive(queue,(char *) &request, 8192, &priority);
    if(amount == -1){
        perror("mq_receive");
    }
    printf("Received request %u with type %u\n",request.serial_number,request.type_id);
    putInQueue(request);
    
    sigqueue(dispatcher_pid, SIGRT_OK, envelope);
    printf("Confirmation sent to dispatcher\n");
}


/*
    Artificial memset used to fill the requests array with an initial request of type 0.
*/
void fillRequests(struct request* requests, unsigned int size){
    struct request request = {.type_id = 0, .serial_number = 0, .process_time = 0};
    for(unsigned int i = 0; i < size; i++){
       requests[i] = request;
    }
}


int main(int argc, char* argv[]){
    srand(getpid());
    memset(buffer,0,sizeof(buffer));
    fillRequests(requests_type_1,2);
    fillRequests(requests_type_2,2);
    fillRequests(requests_type_3,2);
        
    sigfillset(&mask);
    sigdelset(&mask, SIGINT);
    sigprocmask(SIG_SETMASK, &mask, NULL);
    memset(&descriptor,0,sizeof(descriptor));

    descriptor.sa_flags = SA_SIGINFO;

    descriptor.sa_sigaction = handleRequest;
    sigaction(SIGRT_REQ, &descriptor, NULL);
    
    descriptor.sa_sigaction = handleStart;
    sigaction(SIGRT_START, &descriptor, NULL);

    descriptor.sa_sigaction = handleOK;
    sigaction(SIGRT_OK, &descriptor, NULL);
    
    descriptor.sa_sigaction = handler;
    sigaction(SIGINT, &descriptor, NULL);

    sema = sem_open("/memory", O_CREAT, 0600, 1);

    queue = mq_open("/dispatcher", O_CREAT | O_RDWR, 0666, NULL);
    if(queue == -1){
        perror("mq_open");
        return EXIT_FAILURE;
    }
    
    ssize_t amount = mq_receive(queue, buffer, 8192, &priority);
    if(amount == -1){
        perror("mq_receive");
    }
    
    printf("Guichet received dispatcher's pid : %s\n", buffer);    
    dispatcher_pid = atoi(buffer);
    
    // Warn dispatcher that its pid was correctly received
    sigqueue(dispatcher_pid, SIGRT_PING, envelope);

    // Ready to receive requests
    state = WAITING;
    sigdelset(&mask, SIGRT_REQ);
    sigdelset(&mask, SIGRT_START);
    sigprocmask(SIG_SETMASK, &mask, NULL);
    
    while(exit_prog){
        pause();
    }
    printf("Guichet process ended\n");
    sem_unlink("/memory");
    mq_unlink("/dispatcher");
}
