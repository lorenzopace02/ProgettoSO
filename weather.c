/*System libraries*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h> /* For portability*/
#include <sys/sem.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <float.h> /*for double compare in arrContains*/
#include <math.h> /* for abs of value (double compare function)*/
#include <sys/shm.h>
#include <signal.h>
#include <sys/msg.h>
/*Own libraries or definitions*/
#include "config.h"
#include "struct.h"
#include <strings.h>
/*Function declaration*/
int variableUpdate();
void storm();
void swell();
void maelstrom();
void deallocateResources();
pid_t * ships_in_sea(int*);
int getIndexFromPid(pid_t);

/*Global variables*/
struct port *ports;
struct ship_condition * ships;
pid_t * ship_in_sea;
int dumpSem;
int sem_ship;
struct weather_states* weather_d;
struct port_states *port_d;

/*Handler for SIGUSR1 and SIGTERM*/
void signalHandler(int signal){
    switch(signal){
        case SIGUSR1:
            swell();
            storm();
        break;
        case SIGTERM:
            deallocateResources();
            exit(EXIT_SUCCESS);
        break;
    }
}

int main(){
    int sySem;
    int shmPort;
    int shmShip;
    int shmWeather;
    int shm_dump_port;
    struct sembuf sops; 
    struct sigaction sa;
    sigset_t mask;
    struct timespec req;
    struct timespec rem;
    double time_in_sec;

    if(variableUpdate()){
        printf("Error set all variable\n");
        return 0;
    }
    /*SHM ports*/
    if((shmPort = shmget(PORT_POS_KEY,0,0666 )) == -1){
        fprintf(stderr,"Error shared memory creation, %d: %s\n",errno,strerror(errno));
        exit(EXIT_FAILURE);
    }
    ports = (struct port*) shmat(shmPort,NULL,SHM_RDONLY);

    if (ports == (void *) -1){
        fprintf(stderr,"Error assing ports to shared memory, %d: %s\n",errno,strerror(errno));
        exit(EXIT_FAILURE);
    }
    /*Sy Ship*/
    if((sem_ship = semget(SHIP_KEY,1,0666)) == -1){
        fprintf(stderr,"Error semaphore creation, %d: %s\n",errno,strerror(errno));
        exit(EXIT_FAILURE);
    }
    /*SHM Ship*/
    if((shmShip = shmget(SHIP_POS_KEY,0,0666)) == -1){
        fprintf(stderr,"Error shared memory ship creation weather, %d: %s\n",errno,strerror(errno));
        exit(EXIT_FAILURE);
    }


    ships = (struct ship_condition *) shmat(shmShip,NULL,SHM_RDONLY);
    
    if (ships == (void *) -1){
        fprintf(stderr,"Error assing ships to shared memory, %d: %s\n",errno,strerror(errno));
        exit(EXIT_FAILURE);
    }

    /*Sem for dump*/
    if((dumpSem = semget(DUMP_KEY,1,0666)) == -1){
        fprintf(stderr,"Error semaphore creation, %d: %s\n",errno,strerror(errno));
        exit(EXIT_FAILURE);
    }

    /*SHM dump weather*/
    if((shmWeather = shmget(WEATHER_DUMP_KEY,0, 0666)) == -1){
        fprintf(stderr,"Error shared memory creation weather, %d: %s\n",errno,strerror(errno));
        exit(EXIT_FAILURE);
    }

    weather_d = (struct weather_states *) shmat(shmWeather,NULL,0);
    
    if (weather_d == (void *) -1){
        fprintf(stderr,"Error assing ships to shared memory, %d: %s\n",errno,strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    /*SHM dump port*/
    if((shm_dump_port=shmget(PORT_DUMP_KEY,0,0666))==-1)
        TEST_ERROR;

    port_d=(struct port_states*)shmat(shm_dump_port,NULL,0);

    /*Semaphore for sync*/
    if((sySem = semget(SY_KEY,1,0666)) == -1){
            fprintf(stderr,"Error semaphore creation, %d: %s\n",errno,strerror(errno));
        exit(EXIT_FAILURE);
    }

    /*Setup handler*/
    bzero(&sa, sizeof(sa));
    sa.sa_handler = signalHandler;
    sa.sa_flags= SA_NODEFER;
    sigaction(SIGUSR1,&sa,NULL);
    sigaction(SIGTERM,&sa,NULL);

    sigemptyset(&mask);

    /*waiting for synchronization*/
    sops.sem_num=0;
    sops.sem_op=-1;
    sops.sem_flg=0;
    semop(sySem,&sops,1); 

    sops.sem_op=0;
    semop(sySem,&sops,1);

    time_in_sec=SO_MAELSTROM/24.0;
    rem.tv_sec=0;
    rem.tv_nsec=0;
    do{        
        req.tv_sec=(int) time_in_sec;
        req.tv_nsec=(time_in_sec-(int)time_in_sec)*1000000000;
        while(nanosleep(&req,&rem)<0){
            
            if(errno!=EINTR){
                TEST_ERROR;
            }else{
                req.tv_sec=rem.tv_sec;
                req.tv_nsec=rem.tv_nsec;
            }
        }
       maelstrom();
    }while(1);

    return 0;
}
/*Functions definitions*/

/*
Input: int*
Output: int
Desc: Given an input number returns an array containing the pids of the ships at sea
*/
pid_t * ships_in_sea(int* length){
    int i;
    pid_t *ships_sea;
    int j = 0;
    ships_sea=(pid_t*)malloc(sizeof(pid_t));  
          
    for(i=0;i<SO_NAVI;i++){
        if(ships[i].port == 0){
            ships_sea[j] = ships[i].ship;

            j++;
            ships_sea=(pid_t*)realloc(ships_sea,(j+1)*sizeof(pid_t));
        }
    }

    *length = j;
    if(j == 0){
        return NULL;
    }else
        return ships_sea;  
    
}


/*
Input: void
Output: void
Desc: Given a random port sends a SIGURS2 signal 
*/
void swell(){
    int posPort;
    struct sembuf sops_dump;
    srand(time(NULL));
    posPort = rand()%SO_PORTI;
    kill(ports[posPort].pidPort,SIGUSR2);
}

/*
Input: void
Output: void
Desc: Select a ship at sea from an array and send a SIGURS2 signal
*/
void storm(){
    int index;
    int length = 0;
    struct sembuf sops_dump;
    struct sembuf ship_dump;
    pid_t *ships_sea;

    if(semctl(sem_ship,0,GETPID) == getpid() && semctl(sem_ship,0,GETVAL) == 0){

        ships_sea = ships_in_sea(&length);

        if(length != 0){
            srand(time(NULL));
            index=rand()%length;
            kill(ships_sea[index],SIGUSR2);

            /*Dump*/
            if(semctl(dumpSem,0,GETPID) == getpid() && semctl(dumpSem,0,GETVAL) == 0){
                weather_d->storm += 1;
            }else{
                sops_dump.sem_op=-1;
                sops_dump.sem_num=0;
                sops_dump.sem_flg=0;
                while(semop(dumpSem,&sops_dump,1)<0){
                    if(errno!=EINTR){
                        fprintf(stderr,"Error in semop, %d: %s\n",errno,strerror(errno));
                        break;
                    }
                }
                weather_d->storm += 1;
                sops_dump.sem_op=1;
                semop(dumpSem,&sops_dump,1);   
            }
        }

        free(ships_sea);

    }else{

        ship_dump.sem_num=0;
        ship_dump.sem_flg=0;
        ship_dump.sem_op=-1;

        while(semop(sem_ship,&ship_dump,1)<0){
            if(errno!=EINTR){
                break;
            }
        }

        ships_sea = ships_in_sea(&length);
        if(length != 0){
            srand(time(NULL));
            index=rand()%length;
            kill(ships_sea[index],SIGUSR2);

            ship_dump.sem_op=1;
            semop(sem_ship,&ship_dump,1);
            /*Dump*/
            if(semctl(dumpSem,0,GETPID) == getpid() && semctl(dumpSem,0,GETVAL) == 0){
                weather_d->storm += 1;
            }else{
                sops_dump.sem_op=-1;
                sops_dump.sem_num=0;
                sops_dump.sem_flg=0;
                while(semop(dumpSem,&sops_dump,1)<0){
                    if(errno!=EINTR){
                        fprintf(stderr,"Error in semop, %d: %s\n",errno,strerror(errno));
                        break;
                    }
                }
                weather_d->storm += 1;
                sops_dump.sem_op=1;
                semop(dumpSem,&sops_dump,1);   
            }
        }

        free(ships_sea);

    }


}

/*
Input: void
Output: void
Desc: Select a ship at sea from an array and send a SIGALRM signal
*/
void maelstrom(){
    int randShip;
    int length = 0;
    struct sembuf sops_dump;
    struct sembuf ship_dump;
    pid_t *ships_sea;


    ship_dump.sem_num=0;
    ship_dump.sem_flg=0;
    ship_dump.sem_op=-1;

    while(semop(sem_ship,&ship_dump,1)<0){
        if(errno!=EINTR){
            break;
        }
    }

    ships_sea = ships_in_sea(&length);
    ship_dump.sem_num=0;
    ship_dump.sem_flg=0;
    srand(time(NULL));
    if(ships_sea != NULL){
        randShip=rand()%length;  
        kill(ships_sea[randShip],SIGALRM);
        
        ship_dump.sem_op=1;
        semop(sem_ship,&ship_dump,1);
        /*Dump*/
        sops_dump.sem_op=-1;
        sops_dump.sem_num=0;
        sops_dump.sem_flg=0;
        while(semop(dumpSem,&sops_dump,1)<0){
            if(errno!=EINTR){               
                break;
            }
        }
        weather_d->maelstrom += 1;
        sops_dump.sem_op=1;
        semop(dumpSem,&sops_dump,1);   
    }
    free(ships_sea);
}

/*
Input: pid_t
Output: int
Desc: given a pid returns the position in the array
*/
int getIndexFromPid(pid_t pidPort){
    int i;
    for(i = 0;i<SO_PORTI;i++)
        if(ports[i].pidPort == pidPort)
            return i;
}

/*
Input: void
Output: int
Desc: returns 0 if variable loading has been performed, 1 otherwise
*/
int variableUpdate(){
    char buffer[256];
	char *variable;
    char * value;
    FILE *f;
    int VarValue;
	f= fopen("variableFile.txt", "r");

    while(fgets(buffer, 256, f) != NULL){
        variable = strtok(buffer, "=");
        value = strtok(NULL, "=");
        if(strcmp(variable,"SO_PORTI")== 0){
            SO_PORTI = atoi(value);
            if(SO_PORTI < 4)
                return 1;
        }
        if(strcmp(variable,"SO_NAVI")== 0){
            SO_NAVI = atoi(value);
            if(SO_NAVI < 1)
                return 1;
        }
        if(strcmp(variable,"SO_MAELSTROM") == 0){
            SO_MAELSTROM = atoi(value);
        }    

    }

	fclose(f);
    return 0;
}

/*
Input: void
Output: void
Desc: detach shared memory
*/
void deallocateResources(){

    shmdt(ships);
    shmdt(ports);
    shmdt(port_d);
    shmdt(weather_d);

}  






