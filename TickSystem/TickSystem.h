#ifndef tick_H
#define tick_H   // these form a guard


typedef struct {
    int Destination[2][2];//tilex,y.. pixelx,y
    char MovementId[17];
    
} MovementOrders;

typedef struct {
    MovementOrders** list;
    int count;
    int capacity;
} MovementList;


typedef struct {
    int index;  //index of the building in the buildinglist
    int x;      //all access to tiles should be through the cache
    int y;
} ConstructionOrders;

typedef struct {
    ConstructionOrders** list;
    int count;
    int capacity;
} ConstructionList;

typedef struct {
    char username[256];//what user has training that needs work
} UnitTrainingOrders;

typedef struct {
    UnitTrainingOrders** list;
    int count;
    int capacity;
} UTList;

typedef struct {
    MovementList Movements;
    ConstructionList Constructions;
    UTList UnitTrainings;

} Bucket;

typedef struct {
    Bucket Buckets[5];
    int currBucket;
} TickSystem;

extern TickSystem TickS;

void initBucket(Bucket* b);

void initBuckets();

void IncrementTickSystem();


void AddMovementOrder(int cx,int cy,int px,int py);
void AddConstructionOrder(int index,int cx,int cy,int px,int py);
void AddUserWithTrainingOrders(char* username);

#endif