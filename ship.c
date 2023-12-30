/*System libraries*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h> /* For portability */
#include <sys/sem.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <float.h> /*for double compare in arrContains*/
#include <math.h>  /* for abs of value (double compare function)*/
#include <signal.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <strings.h>
/*Own libraries or definitions*/
#include "config.h"
#include "struct.h"

/*Function declaration*/
struct coords generateRandCoords();
double distance(double,double,double,double);
void docking(pid_t);
void undocking(pid_t);
int getSupply(pid_t);
struct port* near_ports(pid_t,double,int *);
struct coords getCoordFromPid(int);
int nearPort(pid_t,pid_t ,struct coords);
int get_index_from_pid(pid_t);
void deallocateResources();
int getMaxExpiryDate(struct shmSinglePort[]);
void reloadExpiryDate();
int variableUpdate();
void storm_sleep();
int getPositionByShipPid();
void restoreDemand();
void swell();

/*Global variables*/
int dumpSem;
int sem_ship;
struct good goods_on; 
struct shmSinglePort * shmPort;
struct ship_condition * ships;
struct port *ports;
pid_t demandPort;
struct ship_dump *ship_d;
struct goods_states* good_d;
struct port_states* port_d;
int size;
int dockSem;

/*Handler Ship*/
void signalHandler(int signal){
    struct sembuf ship_dump;
    
    switch(signal){
        case SIGUSR2:
            storm_sleep();
        break;
        case SIGUSR1:
            reloadExpiryDate();
        break;
        case SIGQUIT:
            swell();
        break;
        case SIGALRM:
            if(semctl(sem_ship,0,GETPID) == getpid() && semctl(sem_ship,0,GETVAL) == 0)
                ships[getPositionByShipPid(getpid())].port = -1;
            else{ 
                ship_dump.sem_flg=0;
                ship_dump.sem_op=-1;
                ship_dump.sem_num=0;
                semop(sem_ship,&ship_dump,1);
                ships[getPositionByShipPid(getpid())].port = -1;
                ship_dump.sem_op = 1;
                semop(sem_ship,&ship_dump,1);
            }
            restoreDemand();    
            deallocateResources();
            exit(EXIT_SUCCESS);
        break;
        case SIGTERM:
            deallocateResources();
            exit(EXIT_SUCCESS);
        break;

    }
    
}

/*ship main*/
int main(){
    int shm_ports;
    struct timespec req;
    struct timespec rem ;
    int sySem;
    int shmPort;
    struct sembuf sops; 
    int prevPort;  
    struct coords ship_coords;
    int nextPort;
    int currentPort;
    struct sigaction sa;
    sigset_t mask;
    int i = 0;
    int shm_dump_port;
    int shm_dump_ship;
    int shm_dump_goods; 
    double distanza;
    int shmShip;
    struct sembuf sops_dump;
    
    if(variableUpdate()){
        printf("Error set all variable\n");
        return 0;
    }

    /*Generation coords ship*/
    ship_coords=generateRandCoords();
    
    /*Shared memory ports*/
    if((shm_ports = shmget(PORT_POS_KEY,0,0666)) == -1){
        fprintf(stderr,"Error shared memory ship_ports, %d: %s\n",errno,strerror(errno));
        exit(EXIT_FAILURE);
    }
    ports= (struct port *) shmat(shm_ports,NULL,SHM_RDONLY);
    if (ports == (void *) -1){
        fprintf(stderr,"Error shared memory ports in ship, %d: %s\n",errno,strerror(errno));
        exit(EXIT_FAILURE);
    }

    /*shm for ships attached to a port */
    if((shmShip = shmget(SHIP_POS_KEY,0,0666)) == -1){
        TEST_ERROR; 
    }

    ships =shmat(shmShip,NULL,0);
    
    /*shm_mem for dump_port */
    if((shm_dump_port=shmget(PORT_DUMP_KEY,sizeof(struct port_states),0666))==-1)
        TEST_ERROR;

    port_d=(struct port_states*)shmat(shm_dump_port,NULL,0);
    TEST_ERROR;


    /*shm_mem for dump_ship*/
    if((shm_dump_ship=shmget(SHIP_DUMP_KEY,sizeof(struct ship_dump),0666))==-1)
        TEST_ERROR;

    ship_d=(struct ship_dump*)shmat(shm_dump_ship,NULL,0);
    TEST_ERROR;

    /*Dump  for goods*/
    
    if((shm_dump_goods = shmget(GOODS_DUMP_KEY,sizeof(struct goods_states),0666 )) == -1){
        fprintf(stderr,"Error shared memory creation  shm_dump_goods in ship, %d: %s\n",errno,strerror(errno));
        exit(EXIT_FAILURE);
    }

    good_d=(struct goods_states*)shmat(shm_dump_goods,NULL,0);

    /*Sem for dump*/
    if((dumpSem = semget(DUMP_KEY,1,0666)) == -1){
        fprintf(stderr,"Error semaphore creation, %d: %s\n",errno,strerror(errno));
        exit(EXIT_FAILURE);
    }

    if((sem_ship = semget(SHIP_KEY,1,0666)) == -1){
        fprintf(stderr,"Error semaphore creation, %d: %s\n",errno,strerror(errno));
        exit(EXIT_FAILURE);
    }

    /*Sy Semaphore*/
    if((sySem = semget(SY_KEY,1,0666)) == -1){
        fprintf(stderr,"Error sy semaphore creation, %d: %s\n",errno,strerror(errno));
        exit(EXIT_FAILURE);
    }

    /*Setup handler*/
    bzero(&sa, sizeof(sa));
    sa.sa_handler = signalHandler;
    sa.sa_flags=0;
    sa.sa_flags= SA_NODEFER;
    sigaction(SIGUSR2,&sa,NULL);
    sigaction(SIGUSR1,&sa,NULL);
    sigaction(SIGTERM,&sa,NULL);
    sigaction(SIGQUIT,&sa,NULL);
    sigaction(SIGALRM,&sa,NULL);
    sigemptyset(&mask);

    /*Global variable initialization*/
    goods_on.type = 0;
    goods_on.quantity = 0;
    goods_on.date_expiry = SO_MAX_VITA;

    /*waiting for synchronization*/
    sops.sem_num=0;
    sops.sem_op=-1;
    sops.sem_flg=0;
    semop(sySem,&sops,1);

    sops.sem_op=0;
    semop(sySem,&sops,1);
    
    /*At start Ship goes to nearest port*/
    currentPort = prevPort = nearPort(0,0,ship_coords);

    distanza=distance(ports[currentPort].coord.x,ports[currentPort].coord.y,ship_coords.x,ship_coords.y)/SO_SPEED;
    rem.tv_sec=0;
    rem.tv_nsec=0;
    req.tv_sec=(int)distanza;
    req.tv_nsec=(distanza-(int)distanza)*10000000;
    while(nanosleep(&req,&rem)<0){
        if(errno!=EINTR){
            TEST_ERROR;
        }else{
            req.tv_sec=rem.tv_sec;
            req.tv_nsec=rem.tv_nsec;
        }
    }

    /*iterative process for ship management*/   
    do{
        docking(ports[currentPort].pidPort);
        nextPort = getSupply(ports[currentPort].pidPort);
        undocking(ports[currentPort].pidPort);
        
        if(nextPort == -1){/*No goods found*/
            nextPort = nearPort(ports[prevPort].pidPort,ports[currentPort].pidPort,ports[currentPort].coord);
            distanza=distance(ports[nextPort].coord.x,ports[nextPort].coord.y,ports[nextPort].coord.x,ports[nextPort].coord.y)/SO_SPEED;
            req.tv_sec=(int)distanza;
            req.tv_nsec=(distanza-(int)distanza)*1000000000;
            prevPort = currentPort;
            currentPort = nextPort;
            while(nanosleep(&req,&rem)<0){
                if(errno!=EINTR){
                    TEST_ERROR;
                }else{
                    req.tv_sec=rem.tv_sec;
                    req.tv_nsec=rem.tv_nsec;
                }
            }
            
        }else{/*Goods found*/
            distanza=distance(ports[nextPort].coord.x,ports[nextPort].coord.y,ports[currentPort].coord.x,ports[currentPort].coord.y)/SO_SPEED;            
            
            req.tv_sec=(int)distanza;
            req.tv_nsec=(distanza-(int)distanza)*1000000000;

            while(nanosleep(&req,&rem)<0){
                if(errno!=EINTR){
                    TEST_ERROR;
                }else{
                    req.tv_sec=rem.tv_sec;
                    req.tv_nsec=rem.tv_nsec;
                }
            }

            prevPort = currentPort;
            currentPort = nextPort;
    
            docking(ports[currentPort].pidPort);

            if(goods_on.date_expiry > 0 && goods_on.quantity > 0){
                distanza=(goods_on.quantity*size)/SO_LOADSPEED;
                req.tv_sec=(int)distanza;
                req.tv_nsec= (distanza-(int)distanza)*1000000000;
                while(nanosleep(&req,&rem)<0){
                    if(errno!=EINTR){
                        TEST_ERROR;
                    }else{
                        req.tv_sec=rem.tv_sec;
                        req.tv_nsec=rem.tv_nsec;
                    }
                }

                sops_dump.sem_num=0;
                sops_dump.sem_flg=0;
                sops_dump.sem_op=-1;
                while(semop(dumpSem,&sops_dump,1)<0){
                    if(errno!=EINTR){
                        break;
                    }
                }
                
                /*Dump goods*/
                good_d[goods_on.type-1].goods_delivered += goods_on.quantity*size;
                good_d[goods_on.type-1].goods_on_ship -= goods_on.quantity*size;
                /*Dump ports*/
                port_d[currentPort].goods_receved +=goods_on.quantity*size;
                port_d[currentPort].goods_demand -=goods_on.quantity*size;
                sops_dump.sem_op=1;
                semop(dumpSem,&sops_dump,1);
                

                goods_on.quantity = 0;
                goods_on.date_expiry = -1;
            }
            
            undocking(ports[currentPort].pidPort);
        }

    }while(1);

    return 0;

}


/*Functions definitions*/

/*
Input: pid_t, pid_t, struct coords
Output: struct port*
Desc: return struct port pointer to the nearest port with different pid than current and previous
*/
int nearPort(pid_t prevPort,pid_t pidPort,struct coords ship_coords){

    int min_distance=SO_LATO*sqrt(2);
    int i;
    int ris_distanza = 0;
    int index_min = 0;

    for(i=0;i<SO_PORTI;i++){
        ris_distanza=distance(ports[i].coord.x,ports[i].coord.y,ship_coords.x,ship_coords.y);
        if(ris_distanza<min_distance && ports[i].pidPort != pidPort && ports[i].pidPort != prevPort){
            min_distance=ris_distanza;
            index_min = i;
        }
    }          
    return index_min;
}

/*
Input: void
Output: struct coords
Desc: return random coordinates between 0 and SO_LATO
*/
struct coords generateRandCoords(){

    struct coords c;
    double div;
    srand(getpid());
    div = RAND_MAX / SO_LATO;
    c.x = rand() / div;
    c.y = rand() / div;

    return c;
}

/*
Input: double,double,double,double
Output: double
Desc: return distance between 2 points 
*/
double distance(double x_p,double y_p,double x_n,double y_n){
    return sqrt(pow(x_n-x_p,2)+pow(y_n-y_p,2));
}

/*
Input: pid_t
Output: void
Desc: procedures for docking at the port
*/
void docking(pid_t pid_port){
    struct sembuf sops;
    struct sembuf sops_dump;
    struct sembuf ship_dump;
    
    if((dockSem = semget(pid_port,1,0666)) == -1)
        TEST_ERROR;

    sops.sem_num=0;    
    sops.sem_flg=0;
    sops.sem_op = -1;
    while(semop(dockSem,&sops,1)<0){
        if(errno!=EINTR){
            break;
        }
    }


    /*Ships info port*/
    ship_dump.sem_op=-1;
    ship_dump.sem_num=0;
    ship_dump.sem_flg=0;

    while(semop(sem_ship,&ship_dump,1)<0){
        if(errno!=EINTR){
            break;
        }
    }
    
    ships[getPositionByShipPid()].port = pid_port;

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
    
    if(goods_on.date_expiry > 0 && goods_on.quantity > 0){
        ship_d->ship_in_port += 1;
        ship_d->ship_sea_goods -= 1;
    }else{
        ship_d->ship_in_port += 1;
        ship_d->ship_sea_no_goods -= 1;
    }

    /*Dump ports*/
    port_d[get_index_from_pid(pid_port)].dock_occuped += 1;

    sops_dump.sem_op=1;
    semop(dumpSem,&sops_dump,1);  
      
}

/*
Input: pid_t
Output: int
Desc: procedures for undocking at the port
*/
void undocking(pid_t pid_port){
    struct sembuf sops;
    struct sembuf sops_dump;
    struct sembuf ship_dump;

    /*Ship out port*/
    ship_dump.sem_op=-1;
    ship_dump.sem_num=0;
    ship_dump.sem_flg=0;
    while(semop(sem_ship,&ship_dump,1)<0){
        if(errno!=EINTR){
            break;
        }
    }
    
    ships[getPositionByShipPid()].port = 0;
    ship_dump.sem_op=1;
    semop(sem_ship,&ship_dump,1);
    
    sops.sem_flg = 0;
    sops.sem_num = 0;
    sops.sem_op = 1;
    semop(dockSem,&sops,1);


    /*Dump ships*/
    sops_dump.sem_flg=0;
    sops_dump.sem_num=0;
    sops_dump.sem_op=-1;
    while(semop(dumpSem,&sops_dump,1)<0){
        if(errno!=EINTR){
            break;
        }
    }
    
    if(goods_on.quantity == 0){
        ship_d->ship_in_port -= 1;
        ship_d->ship_sea_no_goods += 1;
    }else{
        ship_d->ship_in_port -= 1;
        ship_d->ship_sea_goods += 1;
    }

    /*Dump ports*/
    port_d[get_index_from_pid(pid_port)].dock_occuped -= 1;
    sops_dump.sem_op=1;
    semop(dumpSem,&sops_dump,1);
    
}

/*
Input: struct good []
Output: int
Desc: return the maximum expiration date of a supply array
*/
int getMaxExpiryDate(struct shmSinglePort shmPort[]){
    int i;
    int maxDate = 0;
    for(i = 0;i<SO_MERCI;i++){
        if(shmPort[i].supply.date_expiry >= maxDate)
            maxDate = shmPort[i].supply.date_expiry;
    }
    return maxDate;
}


/*
Input: pid_t
Output: int
Desc: returns a pid port containing the next port or an empty port if it finds nothing
*/
int getSupply(pid_t pid_port){
        key_t supplyKey;
        int semSupply;
        int shmSupply;
        struct sembuf sops; 
        int i;
        int min_exipry = SO_MAX_VITA+1;
        int portSize = 0;
        int msgDemand;
        struct port *portNear;
        struct msgDemand demandGood;
        int type;
        int findGood;
        int min_exipry_prev;
        short trovato = 0;
        int min_expry=SO_MAX_VITA+1;
        int flagGood = 0;
        struct port* sendPort;
        struct timespec req;
        struct timespec rem;
        struct sembuf sops_dump;
        double time_nanosleep;
        int shm_good_info;
        struct good_info *matrixtGood;

        /*Get shm semaphore*/
        if((semSupply = semget(pid_port*2,1,0666)) == -1){
            TEST_ERROR;
        }
        
        sops.sem_num=0;
        sops.sem_op = -1;
        sops.sem_flg=0;
        while(semop(semSupply,&sops,1)<0){
            if(errno!=EINTR){
                fprintf(stderr,"Error in semop, %d: %s\n",errno,strerror(errno));
                break;
            }
        }

        

        /*Access to supply shm mstruct port[] near_ports()od*)shmat(shmSupply,NULL,0);*/
        if((shmSupply = shmget(pid_port,0,0666)) == -1){
            TEST_ERROR; 
        }
        
        shmPort = (struct shmSinglePort *)shmat(shmSupply,NULL,0);
        if(shmPort == (void *) -1){
            TEST_ERROR;
        }
        /*Search for min date expiry in current port*/
        for(i=0;i<SO_MERCI;i++){
            if((shmPort[i].supplyGoods == 1 )&&(shmPort[i].supply.date_expiry < min_exipry && shmPort[i].supply.date_expiry > 0)){
                trovato = 1;
                min_exipry = shmPort[i].supply.date_expiry;
                type = i;

            }
        }
        /*If I don’t find anything I’ll leave right away*/
        if(!trovato){
            
            sops.sem_num=0;
            sops.sem_op =1;
            sops.sem_flg=0;
            semop(semSupply,&sops,1);

            goods_on.quantity = 0;
            goods_on.date_expiry = -1;
            shmdt(shmPort);
            return -1;
        }

        /*Set min exipry to prev min exipry*/
        min_exipry_prev = min_exipry;
        min_exipry = SO_MAX_VITA+1;
        
        do{
            portNear = near_ports(pid_port,SO_SPEED*shmPort[type].supply.date_expiry,&portSize); 

            /*Set for demand in near ports*/
            for(i=0;i<portSize && flagGood == 0;i++){
                if((msgDemand = msgget(portNear[i].pidPort,0666)) == -1){
                    fprintf(stderr,"Error messagge queue demand in ship, %d: %s\n",errno,strerror(errno));
                    exit(EXIT_FAILURE);
                }
                
                /*Find goods*/
                if((findGood = msgrcv(msgDemand,&demandGood,sizeof(int),(type+1),IPC_NOWAIT)) != -1){
                    /*Found*/
                    if((shm_good_info = shmget(GOODS_INFO_KEY,0,0666)) == -1)
                        TEST_ERROR;
                    matrixtGood = (struct good_info*)shmat(shm_good_info,NULL,SHM_RDONLY);
                    if(matrixtGood == (void *) -1)
                        TEST_ERROR;
                    size = matrixtGood[type].size;
                    shmdt(matrixtGood);

                    if(shmPort[type].supply.quantity>=demandGood.quantity){
                        if(demandGood.quantity<(SO_CAPACITY/size)){
                            goods_on.date_expiry=shmPort[type].supply.date_expiry;
                            goods_on.quantity = demandGood.quantity;
                            shmPort[type].supply.quantity-=demandGood.quantity;
                        }else{
                            goods_on.date_expiry=shmPort[type].supply.date_expiry;
                            shmPort[type].supply.quantity-= (SO_CAPACITY/size);
                            demandGood.quantity -= (SO_CAPACITY/size); 
                            demandGood.type = type+1;
                            goods_on.quantity = (SO_CAPACITY/size);
                            msgsnd(msgDemand,&demandGood,sizeof(int),IPC_NOWAIT);
                        }
                    }else{
                        if(shmPort[type].supply.quantity<(SO_CAPACITY/size)){
                            goods_on.date_expiry=shmPort[type].supply.date_expiry;
                            shmPort[type].supply.date_expiry = -1;
                            shmPort[type].supplyGoods = 0;
                            demandGood.quantity -= shmPort[type].supply.quantity;
                            demandGood.type = type+1;
                            goods_on.quantity = shmPort[type].supply.quantity;
                            msgsnd(msgDemand,&demandGood,sizeof(int),IPC_NOWAIT);
                        }else{
                            goods_on.date_expiry=shmPort[type].supply.date_expiry;
                            shmPort[type].supply.quantity-= (SO_CAPACITY/size);    
                            demandGood.quantity -= (SO_CAPACITY/size);
                            demandGood.type = type+1;
                            goods_on.quantity = (SO_CAPACITY/size);
                            msgsnd(msgDemand,&demandGood,sizeof(int),IPC_NOWAIT);                       
                        }
                    }
                    /*Dump ports*/ 
                    goods_on.type=type+1;
                    sops_dump.sem_num=0;
                    sops_dump.sem_op=-1;
                    sops_dump.sem_flg=0;
                    while(semop(dumpSem,&sops_dump,1)<0){
                        if(errno!=EINTR){
                            break;
                        }
                    }
                    
                    port_d[get_index_from_pid(pid_port)].goods_sended+=goods_on.quantity*size; 
                    port_d[get_index_from_pid(pid_port)].goods_offer-=goods_on.quantity*size; 
                    
                    sops_dump.sem_op=1;
                    semop(dumpSem,&sops_dump,1);
                    
                    
                    time_nanosleep=(goods_on.quantity*size)/SO_LOADSPEED;
                    req.tv_sec=(int)time_nanosleep;
                    req.tv_nsec= (time_nanosleep-(int)time_nanosleep)*1000000000;
                    /*Time load*/
                    while(nanosleep(&req,&rem)<0){
                        if(errno!=EINTR){
                            TEST_ERROR;
                        }else{
                            req.tv_sec=rem.tv_sec;
                            req.tv_nsec=rem.tv_nsec;
                        }
                    }

                    sops_dump.sem_op=-1;
                     while(semop(dumpSem,&sops_dump,1)<0){
                        if(errno!=EINTR){
                            break;
                        }
                    }
                    
                    good_d[goods_on.type-1].goods_on_ship += goods_on.quantity*size;
                    good_d[goods_on.type-1].goods_in_port -= goods_on.quantity*size;
                    sops_dump.sem_op=1;
                    semop(dumpSem,&sops_dump,1);
                    
                    flagGood = 1;
                    break;
                }

            }
            
            if(!flagGood){
                /*Find next min exipry date*/
                for(i=0;i<SO_MERCI;i++){
                    
                    if((shmPort[i].supplyGoods == 1)&&(shmPort[i].supply.date_expiry > min_exipry_prev && shmPort[i].supply.date_expiry < min_exipry) ){
                        min_exipry= shmPort[i].supply.date_expiry;
                        type = i;
                    }
                }
                /*Set min exipry to prev min exipry*/
                min_exipry_prev = min_exipry;
                min_exipry = SO_MAX_VITA+1;
            }

            
        }while(getMaxExpiryDate(shmPort)  == min_exipry_prev && flagGood == 0);
        
        sops.sem_num=0;
        sops.sem_op= 1;
        sops.sem_flg=0;
        semop(semSupply,&sops,1);
        shmdt(shmPort);


        if(flagGood){
            demandPort = portNear[i].pidPort;
            free(portNear);
            return i;
        }else{
            /*Return empty port*/
            goods_on.quantity = 0;
            goods_on.date_expiry = -1;
            demandPort = 0;
            free(portNear);
            return -1;
        }
        
}

/*
Input: int
Output: struct coords
Desc: returns struct coords from port pid
*/
struct coords getCoordFromPid(int pid){
    int i;
    struct coords c;
    
    for(i = 0;i < SO_PORTI;i ++){
        if(ports[i].pidPort == pid)
            return ports[i].coord;
    }

    return c;
}

/*
Input: pid_t, double, int
Output: struct port*
Desc: returns an array containing the ports included in the max_distance and updates the int length
*/
struct port* near_ports(pid_t currPort,double max_distance,int *length){
    struct port *port_return;
    int i;
    int x;
    int y;
    int j=0;
    struct coords currCoord;

    port_return=(struct port*)malloc(sizeof(struct port));
    currCoord = (getCoordFromPid(currPort));

    for(i=0;i<SO_PORTI;i++){          
        if((ports[i].pidPort != currPort) && (distance(currCoord.x,currCoord.y,ports[i].coord.x,ports[i].coord.y)<max_distance)){
            port_return[j].coord.x=ports[i].coord.x;
            port_return[j].coord.y=ports[i].coord.y;
            port_return[j].pidPort=ports[i].pidPort;
            j++;     
            port_return=(struct port*)realloc(port_return,(j+1)*sizeof(struct port));
        }
    }
    *length = j;
    return port_return;  
}        
      

/*
Input: pid_t
Output: void
Desc: return the index of port in array ports, -1 otherwise
*/
int get_index_from_pid(pid_t pidPort){
    int i;
    for(i=0;i<SO_PORTI;i++){
        if(ports[i].pidPort==pidPort)
            return i;
    }
    return -1;
}

/*
Input: void
Output: void
Desc: return the index of ship in array ships, -1 otherwise
*/
int getPositionByShipPid(){
    int i;
    for(i = 0; i<SO_NAVI;i++)
        if(ships[i].ship == getpid())
            return i;

    return -1;   
}

/*
Input: void
Output: void
Desc: when recive signal of storm, it manages it
*/
void storm_sleep(){
    struct timespec req;
    struct timespec rem;
    double time_storm=SO_STORM_DURATION/24.0;
    rem.tv_sec=0;
    rem.tv_nsec=0;
    req.tv_sec=(int)time_storm;
    req.tv_nsec=(time_storm-(int)time_storm)*1000000000;
    while(nanosleep(&req,&rem)<0){
        if(errno!=EINTR){
            TEST_ERROR;
        }else{
            req.tv_sec=rem.tv_sec;
            req.tv_nsec=rem.tv_nsec;
        }
    }
}


/*
Input: void
Output: void
Desc: when recive signal of swell, it manages it
*/
void swell(){
    struct timespec req;
    struct timespec rem;
    double time_swell=SO_SWELL_DURATION/24.0;
    rem.tv_sec=0;
    rem.tv_nsec=0;
    req.tv_sec=(int)time_swell;
    req.tv_nsec=(time_swell-(int)time_swell)*1000000000;
    while(nanosleep(&req,&rem)<0){
        if(errno!=EINTR){
            TEST_ERROR;
        }else{
            req.tv_sec=rem.tv_sec;
            req.tv_nsec=rem.tv_nsec;
        }
    }
}

/*
Input: void
Output: void
Desc: when a ship has sunk restores the application at the port that required it
*/
void restoreDemand(){
    int idMsgDemand;
    struct msgDemand goods_lost;
    struct sembuf sops_dump;
    sops_dump.sem_num=0;
    sops_dump.sem_flg=0;    

    if(goods_on.quantity != 0){/*With goods*/
        if((idMsgDemand = msgget(demandPort,0666)) == -1){
            fprintf(stderr,"Error messagge queue demand in ship, %d: %s\n",errno,strerror(errno));
            exit(EXIT_FAILURE);
        }
        goods_lost.quantity = goods_on.quantity/size;
        goods_lost.type = goods_on.type;
        if(msgsnd(idMsgDemand,&goods_lost,sizeof(int),IPC_NOWAIT) == -1){
            fprintf(stderr,"Error send messagge queue demand ship, %d: %s\n",errno,strerror(errno));
            exit(EXIT_FAILURE);
        }

        if(semctl(dumpSem,0,GETPID) == getpid() && semctl(dumpSem,0,GETVAL) == 0 ){
            /*Dump ships*/  
            ship_d->ship_sea_goods -= 1;
            /*Dump good*/  
            good_d[goods_on.type-1].goods_on_ship -= goods_on.quantity*size;
            good_d[goods_on.type-1].goods_expired_ship += goods_on.quantity*size;
            
        }else{
            
            sops_dump.sem_op=-1;
            while(semop(dumpSem,&sops_dump,1)<0){
                if(errno!=EINTR){
                    fprintf(stderr,"Error in semop, %d: %s\n",errno,strerror(errno));
                    break;
                }
            }
            /*Dump ships*/  
            ship_d->ship_sea_goods -= 1;
            /*Dump good*/  
            good_d[goods_on.type-1].goods_on_ship -= goods_on.quantity*size;
            good_d[goods_on.type-1].goods_expired_ship += goods_on.quantity*size;

            sops_dump.sem_op=1;
            semop(dumpSem,&sops_dump,1);
        }


    }else{/*Without goods*/

        if(semctl(dumpSem,0,GETPID) == getpid() && semctl(dumpSem,0,GETVAL) == 0 ){
            /*Dump ships*/  
            ship_d->ship_sea_no_goods -= 1;
            
        }else{
            sops_dump.sem_op=-1;
            while(semop(dumpSem,&sops_dump,1)<0){
                if(errno!=EINTR){
                    fprintf(stderr,"Error in semop, %d: %s\n",errno,strerror(errno));
                    break;
                }
            }
            /*Dump ships*/  
            ship_d->ship_sea_no_goods -= 1;

            sops_dump.sem_op=1;
            semop(dumpSem,&sops_dump,1);
        }
    
    }
}

/*
Input: void
Output: void
Desc: detach from shared memory
*/
void deallocateResources(){
    
    shmdt(port_d);
    shmdt(ports);
    shmdt(ship_d);
    shmdt(good_d);
    shmdt(ships);
}

/*SIGALRM
Desc: when receiving a signal updates the expiry date
*/
void reloadExpiryDate(){
    struct sembuf sops_dump;
    struct sembuf ship_dump;

    sops_dump.sem_flg=0;
    sops_dump.sem_num=0;
    if(semctl(sem_ship,0,GETPID) == getpid() && semctl(sem_ship,0,GETVAL) == 0){
        if(goods_on.quantity != 0 &&  ships[getPositionByShipPid(getpid())].port == 0){   
            if(goods_on.date_expiry >= 1){
                goods_on.date_expiry-=1;
            }else{/*expired*/
                goods_on.date_expiry = -1;
                goods_on.quantity = 0;
                /*Dump goods*/ 
                if(semctl(dumpSem,0,GETPID) == getpid() && semctl(dumpSem,0,GETVAL) == 0){  
                    good_d[goods_on.type-1].goods_expired_ship += goods_on.quantity*size;
                    ship_d->ship_sea_goods -= 1;
                    ship_d->ship_sea_no_goods += 1; 
                }else{
                    sops_dump.sem_op=-1;
                    while(semop(dumpSem,&sops_dump,1)<0){
                        if(errno!=EINTR){
                            break;
                        }
                    }
                    good_d[goods_on.type-1].goods_expired_ship += goods_on.quantity*size;
                    ship_d->ship_sea_goods -= 1;
                    ship_d->ship_sea_no_goods += 1; 

                    sops_dump.sem_op=1;
                    semop(dumpSem,&sops_dump,1);
                }
        

            }
        }
    }else{
        ship_dump.sem_num = 0;
        ship_dump.sem_flg = 0;
        ship_dump.sem_op = -1;
        while(semop(dumpSem,&sops_dump,1)<0){
            if(errno!=EINTR){
                fprintf(stderr,"Error in semop, %d: %s\n",errno,strerror(errno));
                break;
            }
        }

        if(goods_on.quantity != 0 && ships[getPositionByShipPid(getpid())].port == 0){   
            if(goods_on.date_expiry >= 1){
                goods_on.date_expiry-=1;
            }else{/*expired*/
                goods_on.date_expiry = -1;
                goods_on.quantity = 0;
                /*Dump goods*/ 
                if(semctl(dumpSem,0,GETPID) == getpid() && semctl(dumpSem,0,GETVAL) == 0){  
                    good_d[goods_on.type-1].goods_expired_ship += goods_on.quantity*size;
                    ship_d->ship_sea_goods -= 1;
                    ship_d->ship_sea_no_goods += 1; 
                }else{
                    sops_dump.sem_op=-1;
                    while(semop(dumpSem,&sops_dump,1)<0){
                        if(errno!=EINTR){
                            break;
                        }
                    }
                    good_d[goods_on.type-1].goods_expired_ship += goods_on.quantity*size;
                    ship_d->ship_sea_goods -= 1;
                    ship_d->ship_sea_no_goods += 1; 

                    sops_dump.sem_op=1;
                    semop(dumpSem,&sops_dump,1);
                }
        

            }
        }

        ship_dump.sem_op = 1;
        semop(sem_ship,&ship_dump,1);
    }

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
        if(strcmp(variable,"SO_LOADSPEED")== 0)
            SO_LOADSPEED = atof(value);
        if(strcmp(variable,"SO_NAVI")== 0){
            SO_NAVI = atoi(value);
            if(SO_NAVI < 1)
                return 1;
        }
        if(strcmp(variable,"SO_SPEED")== 0)
            SO_SPEED = atof(value);
        if(strcmp(variable,"SO_CAPACITY")== 0)
            SO_CAPACITY = atoi(value);
        if(strcmp(variable,"SO_MERCI")== 0)
            SO_MERCI = atoi(value);
        if(strcmp(variable,"SO_MAX_VITA")== 0)
            SO_MAX_VITA = atoi(value);
        if(strcmp(variable,"SO_LATO")== 0){
            SO_LATO = atof(value);
        }
        if(strcmp(variable,"SO_STORM_DURATION") == 0){
            SO_STORM_DURATION = atoi(value);
        }
        if(strcmp(variable,"SO_SWELL_DURATION") == 0){
            SO_SWELL_DURATION = atoi(value);
        }  

    }

	fclose(f);
    return 0;
}
