#ifndef UUP_H
#define UUP_H   // these form a guard


typedef struct {
    char username[256];
} UUpdate;

typedef struct {
    char regName[32];
    char username[256];
} RegUpdate;

typedef struct {
    char name[32];
    int units[3]; // composition array
} RegimentTemplate;

typedef struct {
    char buildingname[32];
    double position[3]; // composition array
    char username[256];
    char taskId[17];
} BuildPlacement;

void TechUpdateTask(void *arg);


void ConstructionUpdateTask(void *arg);

void BuildingPosUpdateTask(void *arg);

void BuildingPlaceTask(void *arg);


void TrainingUpdateTask(void *arg);

void NewRegimenTask(void *arg);

void GetUserTiles(void *arg);
#endif