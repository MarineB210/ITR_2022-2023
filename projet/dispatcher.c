#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <mqueue.h>
#include <semaphore.h>


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
    pid_t pid;
    unsigned int id;
    unsigned int type_id;
    unsigned int busy = 0; // Tell us if the guichet is already working on a request
}


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

unsigned int counter = 0;
unsigned int counter_answers = 0;

volatile sig_atomic_t daytime = OPEN;
volatile sig_atomic_t state = __START__;

void handleAlarm(int signum, siginfo_t* info, void* unused){
    if(daytime == OPEN){
        daytime = CLOSED;
        // The dispatcher is closed for 12 seconds.
        alarm(12);
        pause();
    }
    else if(daytime == CLOSED){
        daytime = OPEN;
        // We restart an alarm for the day.
        alarm(12);
    }
}

void handleAnswer(int signum, siginfo_t* info, void* unused){
    // If the dispatcher received answers while it's closed, the answers treatment
    // is put on hold until its opening.    
    if(daytime == CLOSED){
        pause();
    }
    
    struct answer answer;
    unsigned int size_of_answers = sizeof(struct answer) * MAX_REQUESTS;
    ssize_t amount = mq_receive(queue, answer, sizeof(struct answer), &priority);
    if(amount == -1){
        perror("mq_receive");
    }
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
        answers[counter_answers] = answer;
        counter_answers++;
        for(unsigned int i = 0; i < 6; i++){
            if(guichets[i].pid == info->si_pid){
                guichets[i].busy = 0;
                for(unsigned int j = 0; j < number_of_requests; j++){
                    if(requests_in_waiting[j].type_id == guichets[i].type_id){
                        sendRequest(requests_in_waiting[j],guichets[i].pid);
                        guichets[i].busy = 1;
                    }
                }
            }
        }
        if(counter_answers == number_of_requests){
            sigqueue(client_pid, SIGRT_ANS, envelope);
            sigqueue(info->si_pid, SIGRT_OK, envelope);
            counter_answers = 0;
        }
        
    }

}

void handlePing(int signum, siginfo_t* info, void* unused){
    guichets[counter].pid = info->si_pid;
    guichets[counter].type_id = info->si_value;
    guichets[counter].id = counter;
    counter++;
    if(counter == 5){
        state = EXPECTING;
        counter = 0;
    }
}

void handleOK(int signum, siginfo_t* info, void* unused){
    state = OPERATING;
}

void sendRequest(struct request request, pid_t pid){
    int status = mq_send(queue, request, sizeof(struct request),0);
    if(status == -1){
        perror("mq_send");
    }
    // Send request and wait for confirmation that the guichet received the request.
    state = WAITING;
    sigdelset(SIGRT_OK, &mask);
    sigprocmask(SIG_SETMASK, &mask, NULL);
    sigqueue(pid, SIGRT_REQ, envelope);
    while(state == WAITING){
        pause();
    }

    sigaddset(SIGRT_OK, &mask);
    sigprocmask(SIG_SETMASK, &mask, NULL);
}


void handleRequest(int signum, siginfo_t* info, void* unused){
    // If the dispatcher received requests while it's closed, the requests treatment
    // is put on hold until its opening.
    if(daytime == CLOSED){
        pause();
    }

    // We use a semaphore to make sure all (or most) requests have been sent
    // before the guichets can send their answers.
    sem_wait(sema);

    state == OPERATING;
    
    printf("Dispatcher : %u requests received\n", number_of_requests); 
    number_of_requests = info->si_value;
    client_pid = info->si_pid;
    requests_in_waiting = malloc(number_of_requests * sizeof(struct request));

    for(unsigned int i = 0; i < number_of_requests; i++){
        unsigned int type_id = packet[i].type_id;
        packet[i].serial_number = rand() % 1000; // Assign a serial number to the request
        // We search for an available guichet to handle our request
        for(unsigned int j = 0; j < 6; j++){
            if(guichets[j].type_id == packet[i].type_id && guichets[j].busy == 0){
                sendRequest(packet[i], guichets[j].pid);
                printf("Dispatcher : request %u has been sent to %u", i, guichets[j].id);
                guichets[j].busy = 1;
                break;
            }
            // If we reach this point it means there werent any guichet available
            // Thus we place them in a list to send them later.
            if(j == 5){
                requests_in_waiting->packet[i];
                requests_in_waiting = requests_in_waiting + sizeof(struct request);                
            }
        }
    }
    state == EXPECTING;
    sem_post(sema);
}


int main(int argc, char* argv){
    srand(getpid());
    queue = mq_open("/message_queue", O_WRONLY);
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
        sigprocmask(SIG_SETMASK, &mask, NULL);
        memset(&descriptor,0,sizeof(descriptor));

        descriptor.sa_flags = SA_SIGINFO;

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

        // First we send the pid of our dispatcher to our clients and guichets using mqueue
        if(queue == -1){
            perror("mq_open");
            return EXIT_FAILURE;
        }

        int status = mq_send(queue, getpid(), sizeof(pid_t),0);
        if(status == -1){
            perror("mq_send");
        }

        state = WAITING;
        sigdelset(SIGRT_PING, &mask);
        sigprocmask(SIG_SETMASK, &mask, NULL);

        // We wait for the confirmation of the guichets 
        while(state == WAITING){
            pause();
        }

        // Dispatcher now ready to receive requests and answers.
        sigaddset(SIGRT_PING, &mask);
        sigdelset(SIGRT_REQ, &mask);
        sigdelset(SIGRT_ANS, &mask);
        sigdelset(SIGALRM, &mask);
        sigprocmask(SIG_SETMASK, &mask);
        // The day will last 12 seconds
        alarm(12);
        while(1){
            pause();
        }
    }
    mq_close(queue);
    shmdt(packet);
    return EXIT_SUCCESS;
}