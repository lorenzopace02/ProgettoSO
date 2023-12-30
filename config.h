/*GENERALS*/
int SO_DAYS;/*number of simulation days*/

/*PORTS*/
int SO_PORTI;  /*number of ports (int,>= 4)*/
int SO_BANCHINE; /*maximum number of docks (int)*/
int SO_FILL;  /*maximum goods capacity of the port*/
int SO_LOADSPEED; /*loading/unloading speed of ports (ton/days)*/

/*SHIPS*/
int SO_NAVI; /*number of ships (int,>=1)*/
double SO_SPEED ;  /*speed of ships (double or int)*/
int SO_CAPACITY ; /*capacity of ships (ton)*/

/*GOODS*/
int SO_MERCI ; /*type of goods (int)*/
int SO_SIZE;   /*weight of goods (ton)*/
int SO_MIN_VITA ;  /*minimum expiry date (days)*/
int SO_MAX_VITA ; /*maximum expiry date (days)*/

/*MAP*/
double SO_LATO ;  /*side of the map (double)*/

/*WEATHER EVENTS*/
int SO_STORM_DURATION ;/*duration of storm (hours)*/
int SO_SWELL_DURATION ;/*duration of swell (hours)*/
int SO_MAELSTROM ;/*mealstrom repeat (hours)*/
 
/*KEY SHARE MEMORY*/
#define PORT_POS_KEY 57 /*key for shared memory contains array coords*/
#define SHIP_POS_KEY 13
#define GOODS_DUMP_KEY 31
#define PORT_DUMP_KEY 25
#define SHIP_DUMP_KEY 35
#define WEATHER_DUMP_KEY 7
#define GOODS_INFO_KEY 11

/*KEY MESSAGGE QUEUES*/
#define PORT_MASTER_KEY 23 /*key for shared memory contains array coords*/

/*KEY SEMAPHORE*/
#define DUMP_KEY 42
#define SY_KEY 1
#define SHIP_KEY 40
#define MUTEX_DOCK 15




/*TEST ERROR FUNCTION*/
#define TEST_ERROR if(errno) {fprintf(stderr, \
    "%s:%d: PID=%5d: Error %d (%s)\n",\
    __FILE__,\
    __LINE__,\
    getpid(),\
    errno,\
    strerror(errno));}


