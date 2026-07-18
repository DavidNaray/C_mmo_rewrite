#ifndef UUP_H
#define UUP_H   // these form a guard


typedef struct {
    char sockid[32];
    char username[256];
} UUpdate;

void TechUpdateTask(void *arg);

void ConstructionUpdateTask(void *arg);

void TrainingUpdateTask(void *arg);
#endif