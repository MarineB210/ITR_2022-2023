#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#define __START__ 0
#define WAITING 1
#define OPERATING 2
#define __FINAL__ 3

unsigned int id;
unsigned int type_id;

struct request{
    unsigned int type_id;
    unsigned int serial_number;
    unsigned int process_time;
};

struct answer{
    unsigned int type_id;
    unsigned int serial_number;
};

volatile sig_atomic_t state = __START__;


void process(){
    //TO-DO 
}


int main(int argc, char* argv[]){

}