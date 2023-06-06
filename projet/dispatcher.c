#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <mqueue.h>
#include <semaphore.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/time.h>
#include <sys/shm.h>
#include <fcntl.h>

#define _XOPEN_SOURCE_EXTENDED 1


#define __START__ 0
#define __FINAL__ 1
#define WAITING 2
#define OPERATING 3
#define EXPECTING 4

// Day and night states
#define CLOSED 0 
#define OPEN 1

#define SIGRT_REQ (SIGRTMIN+1)
#define SIGRT_ANS (SIGRTMIN+2)
#define SIGRT_PING (SIGRTMIN+3)
#define SIGRT_OK (SIGRTMIN+4)
#define SIGRT_START (SIGRTMIN+5)

#define MAX_REQUESTS 10

struct request{
    unsigned int type_id;
    unsigned int serial_number;
    unsigned int process_time;
};

struct answer{
    unsigned int type_id;
    unsigned int serial_number;
};

struct guichet_info{
    unsigned int id;
    unsigned int type_id;
    unsigned int busy; // Tell us if the guichet is already working on a request
};


struct timespec requested = {12,0};
struct timespec remaining;

struct request* packet; // Shared memory segment for requests
struct answer* answers; // Shared memory segment for answers.
struct request* requests_in_waiting;
sem_t* sema;
sigset_t mask;
union sigval envelope;
struct sigaction descriptor;
struct guichet_info guichets[6]; // 2 guichets for each type of demands
mqd_t queue;
unsigned int number_of_requests;
pid_t client_pid;
pid_t guichet_pid;

unsigned int exit_prog = 1;
int priority = 0;
unsigned int requests_in_waiting_counter = 0;
unsigned int counter = 0;
unsigned int counter_answers = 0;
unsigned int number_of_requests_sent = 0;

volatile sig_atomic_t daytime = OPEN;
volatile sig_atomic_t state = __START__;


void handler(int signum, siginfo_t* info, void* unused){
    if(signum == SIGINT){
        exit_prog = 0;
        printf("\nClosing\n");
    }
}


void handleAlarm(int signum, siginfo_t* info, void* unused){
    if(daytime == OPEN){
        daytime = CLOSED;
        // The dispatcher is closed for 12 seconds.
        printf("Night started\n");
        clock_nanosleep(CLOCK_REALTIME,0,&requested,&remaining);
        printf("Day started\n");
        daytime = OPEN;
    }
}


void sendRequest(struct request request, unsigned int pid){
    int status = mq_send(queue,(const char *) &request, sizeof(struct request),0);
    if(status == -1){
        perror("mq_send");
    }
    // Send request and wait for confirmation that the guichet received the request.
    state = WAITING;
    sigdelset(&mask,SIGRT_OK);
    sigprocmask(SIG_SETMASK, &mask, NULL);
    
    // Add the type id of the request to the enveloppe
    envelope.sival_int = request.type_id;
    sigqueue(guichet_pid, SIGRT_REQ, envelope);
    
    while(state == WAITING){
        pause();
    }
    number_of_requests_sent++;
    sigaddset(&mask, SIGRT_OK);
    sigprocmask(SIG_SETMASK, &mask, NULL);
}


void handleAnswer(int signum, siginfo_t* info, void* unused){
    // If the dispatcher received answers while it's closed, the answers treatment
    // is put on hold until its opening.    
    if(daytime == CLOSED){
        clock_nanosleep(CLOCK_REALTIME,0,&remaining,&remaining);
        printf("Day started\n");
        daytime = OPEN;
    }

    struct answer answer;

    ssize_t amount = mq_receive(queue, (char *) &answer, 8192, &priority);
    printf("Answer %u received\n", answer.serial_number);
    if(amount == -1){
        perror("mq_receive");
    }

    answers[counter_answers] = answer;
    counter_answers++;
    for(unsigned int i = 0; i < 6; i++){
        if(guichets[i].type_id == answer.type_id && guichets[i].busy == 1){
            guichets[i].busy = 0;
            sigqueue(guichet_pid, SIGRT_OK, envelope);
            break;
        }
    }
    // When all the requets have been answered
    if(counter_answers == number_of_requests){
        printf("All requests have been received, sending to client\n");
        counter_answers = 0;
        requests_in_waiting_counter = 0;
        number_of_requests_sent = 0;
        free(requests_in_waiting);
        sigqueue(client_pid, SIGRT_ANS, envelope);
    }
    //If we received all the answers from the requests sent and there are still some that are unsent we try to send them
    else if(number_of_requests_sent == counter_answers){
        // We check if it's now not possible to send a request that was once on hold
        sem_wait(sema);
        requests_in_waiting_counter = 0;
        for(unsigned int i = 0; i < 6; i++){
            for(unsigned int j = 0; j < number_of_requests; j++){
                if(requests_in_waiting[j].type_id == guichets[i].type_id && guichets[i].busy == 0){
                    sendRequest(requests_in_waiting[j],guichet_pid);
                    printf("Dispatcher : request %u of type %u has been sent to %u treating type %u\n", packet[i].serial_number, packet[i].type_id, guichets[j].id, guichets[j].type_id);
                    requests_in_waiting[j].type_id = 0;
                    guichets[i].busy = 1;
                    break;
                }
                if(j == 5){
                    printf("Dispatcher : no guichet with type %u was available for request %u with type %u\n", packet[i].type_id, packet[i].serial_number, packet[i].type_id);
                    requests_in_waiting[requests_in_waiting_counter] = packet[i];
                    requests_in_waiting_counter++;
                }
            }      
        }
        sem_post(sema);
        sigqueue(guichet_pid, SIGRT_START, envelope);
    }
}


/*
    PING signal received after the guichet correctly received the dispatcher's pid.
    Will generate informations about the guichets.
*/
void handlePing(int signum, siginfo_t* info, void* unused){
    for(unsigned int i = 0; i < 2; i++){
        guichets[counter].type_id = 1;
        guichets[counter].id = rand() % 1000; // Assign an id to the guichet;
        guichets[counter].busy = 0;
        printf("Guichet %u with type %u ready\n",guichets[counter].id, guichets[counter].type_id);
        counter++;
    }
    for(unsigned int i = 0; i < 2; i++){
        guichets[counter].type_id = 2;
        guichets[counter].id = rand() % 1000; // Assign an id to the guichet;
        guichets[counter].busy = 0;
        printf("Guichet %u with type %u ready\n",guichets[counter].id, guichets[counter].type_id);
        counter++;
    }
    for(unsigned int i = 0; i < 2; i++){
        guichets[counter].type_id = 3;
        guichets[counter].id = rand() % 1000; // Assign an id to the guichet;
        guichets[counter].busy = 0;
        printf("Guichet %u with type %u ready\n",guichets[counter].id, guichets[counter].type_id);
        counter++;
    }
    guichet_pid = info->si_pid;
    state = EXPECTING;
}


void handleOK(int signum, siginfo_t* info, void* unused){
    state = OPERATING;
}


void handleRequest(int signum, siginfo_t* info, void* unused){
    // If the dispatcher received requests while it's closed, the requests treatment
    // is put on hold until its opening.
    if(daytime == CLOSED){
        clock_nanosleep(CLOCK_REALTIME,0,&remaining,&remaining);
        printf("Day started\n");
        daytime = OPEN;
    }

    // We use a semaphore to make sure all (or most) requests have been sent
    // before the guichets can send their answers.
    sem_wait(sema);

    state == OPERATING;
    
    
    number_of_requests = info->si_value.sival_int;
    printf("Dispatcher : %u requests received\n", number_of_requests); 
    client_pid = info->si_pid;
    
    // We allocate some space to put the requests that might not have been able to be sent
    requests_in_waiting = (struct request *) malloc(number_of_requests * sizeof(struct request));

    for(unsigned int i = 0; i < number_of_requests; i++){
        unsigned int type_id = packet[i].type_id;
        packet[i].serial_number = rand() % 1000; // Assign a serial number to the request
        // We search for an available guichet to handle our request
        for(unsigned int j = 0; j < 6; j++){
            if(guichets[j].type_id == packet[i].type_id && guichets[j].busy == 0){
                sendRequest(packet[i], guichet_pid);
                printf("Dispatcher : request %u of type %u has been sent to %u treating type %u\n", packet[i].serial_number, packet[i].type_id, guichets[j].id, guichets[j].type_id);
                guichets[j].busy = 1;
                break;
            }
            // If we reach this point it means there werent any guichet available
            // Thus we place them in a list to send them later.
            if(j == 5){
                printf("Dispatcher : no guichet with type %u was available for request %u with type %u\n", packet[i].type_id, packet[i].serial_number, packet[i].type_id);
                requests_in_waiting[requests_in_waiting_counter] = packet[i];
                requests_in_waiting_counter++;
            }
        }
    }
    state == EXPECTING;
    sem_post(sema);
    // When the dispatcher finished sending all the requests, it sends the guichet a signal
    sigqueue(guichet_pid, SIGRT_START, envelope);
}


int main(int argc, char* argv){
    printf("Dispatcher started \n");
    srand(getpid());
    queue = mq_open("/dispatcher", O_CREAT | O_RDWR, 0666, NULL);
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
    if(packet ==(void*) -1){
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
        
        descriptor.sa_sigaction = handleAlarm;
        sigaction(SIGALRM, &descriptor, NULL);

        descriptor.sa_sigaction = handleRequest;
        sigaction(SIGRT_REQ, &descriptor, NULL);

        descriptor.sa_sigaction = handleOK;
        sigaction(SIGRT_OK, &descriptor, NULL);

        descriptor.sa_sigaction = handleAnswer;
        sigaction(SIGRT_ANS, &descriptor, NULL);

        descriptor.sa_sigaction = handlePing;
        sigaction(SIGRT_PING, &descriptor, NULL);

        sema = sem_open("/memory", O_CREAT, 0600, 1);

        // First we send the pid of our dispatcher to the guichet using mqueue
        if(queue == -1){
            perror("mq_open");
            return EXIT_FAILURE;
        }

        pid_t pid = getpid();
        printf("Dispatcher pid : %i\n",pid);
        char pidchar[10];
        snprintf(pidchar, 10,"%d",(int)getpid());

        int status = mq_send(queue, pidchar, sizeof(pidchar),0);

        if(status == -1){
            perror("mq_send");
        }

        state = WAITING;
        sigdelset(&mask, SIGRT_PING);
        sigprocmask(SIG_SETMASK, &mask, NULL);

        // We wait for the confirmation of the guichet 
        while(state == WAITING){
            pause();
        }

        // We send the pid to the client.
        status = mq_send(queue, pidchar, sizeof(pidchar),0);
        if(status == -1){
            perror("mq_send");
        }
        
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
        
        // Dispatcher now ready to receive requests and answers.
        printf("Dispatcher ready\n");
        sigaddset(&mask, SIGRT_PING);
        sigdelset(&mask, SIGRT_REQ);
        sigdelset(&mask, SIGRT_ANS);
        sigdelset(&mask, SIGALRM);

        // The day will last 12 seconds
        struct itimerval cfg;
        cfg.it_value.tv_sec = 12; //Phase : 12, we start the night day cycle after 12 seconds
        cfg.it_value.tv_usec = 0;
        cfg.it_interval.tv_sec = 24; //Period : 24. I choose 24 seconds because nanosleep is used during the night instead of a SIGALRM
        cfg.it_interval.tv_usec = 0;
        setitimer(ITIMER_REAL, &cfg, NULL);

        printf("Day started\n");        
        sigprocmask(SIG_SETMASK, &mask, NULL);

        while(exit_prog){
            pause();
        }
        printf("Dispatcher process ended\n");
        mq_close(queue);
        shmdt(packet);
        shmdt(answers);
        sem_unlink("/memory");
        sem_destroy(sema);
        return EXIT_SUCCESS;
    }
}
