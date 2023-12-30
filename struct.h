typedef struct{
    unsigned short sem_num;
    short sem_op;
    short sem_flg;
}sembuf;

typedef struct coords{
    double x;
    double y;
}structCoords;

typedef struct port{
    pid_t pidPort;
    struct coords coord;
}structPort;

typedef struct good{
    int type;
    int quantity;
    int date_expiry;
}structGood;

typedef struct shmSinglePort{
    struct good supply;
    int demandGoods;
    int supplyGoods;
}structShmSingleSupply;

typedef struct msgDemand{
    long type;
    int quantity;
}structMsgDemand;

typedef struct goods_states{
    int goods_in_port;
    int goods_on_ship;
    int goods_delivered;
    int goods_expired_port;
    int goods_expired_ship;
}struct_goods_states;

typedef struct port_states{
    int goods_sended;
    int goods_receved;
    int goods_offer;
    int goods_demand;
    int dock_occuped;
    int dock_total;
    int swell;
}struct_port_states;

typedef struct ship_dump{
    int ship_sea_goods;
    int ship_in_port;
    int ship_sea_no_goods;
}struct_ship_dump;

typedef struct ship_condition{
    int ship;
    int port;
}struct_ship_condition;

typedef struct msgSupply{
    long pid;
    int type;
    int quantity;
}struct_msgSupply;

typedef struct weather_states{
    int storm;
    int maelstrom;
}struct_weather_states;

typedef struct good_info{
    int size;
    int life;
}struct_good_info;
