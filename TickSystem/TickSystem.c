#include "TickSystem.h"

#include "../MongoDBReadWriteCache/Cache.h"
#include "../MongoDBReadWriteCache/Schema/UserBreakdown.h"
// #include "../MongoDBReadWriteCache/Schema/TileSchema.h"
#include "../MongoDBReadWriteCache/ReadUser.h"
#include "../MongoDBReadWriteCache/Cache.h"
#include <mongoc/mongoc.h>

#include <cJSON.h>
#include <windows.h>

#include "BuildingComponent.h"
#include "MovementComponent.h"

TickSystem TickS;

void initBucket(Bucket* b) {
    b->Movements.list = NULL;
    b->Movements.count = 0;
    b->Movements.capacity = 0;

    b->Constructions.list = NULL;
    b->Constructions.count = 0;
    b->Constructions.capacity = 0;
}

void initBuckets() {
    for (int i=0;i<5;i++){initBucket(&TickS.Buckets[i]);}
    TickS.currBucket=0;
}

void IncrementTickSystem(){
    Bucket* b = &TickS.Buckets[TickS.currBucket];

    //construction
    BuildingLoop(b);


    //unit training
    pthread_mutex_lock(&GlobalCache->lock);
    for (int i = 0; i < b->UnitTrainings.count; i++) {
        UnitTrainingOrders* ut = b->UnitTrainings.list[i];

        User* u=cache_get_user(GlobalCache,ut->username);
        if (!u){continue;}

        bool stillTraining = false;

        for (int j = 0; j < MAX_REGIMENS; j++) {
            RegimenTraining* rt = &u->regimenTrainingList.regimens[j];
            if(!rt->active || rt->deployable){continue;}//if false then nothing to do here

            stillTraining = true;

            int totalProgress = 0;
            int totalFinish = 0;
            bool allDone = true;
            for (int t = 0; t < UNIT_MAX; t++) {
                UnitTraining* tr = &rt->units[t];
                if (tr->count <= 0){continue;}//no units to train of this type, skip
                tr->progress++;
                totalProgress += tr->progress;
                totalFinish += tr->finish;

                if (tr->progress <= tr->finish) {allDone=false;}
            }

            int completeness = 0;
            completeness = (totalProgress * 100) / totalFinish;
            if (completeness > 100){completeness = 100;}

            if (allDone) {
                rt->deployable = true;

                char msg[256];
                snprintf(
                    msg, sizeof(msg),
                    "{\"type\":\"RegimenReady\","
                    "\"username\":\"%s\","
                    "\"slot\":%d}",
                    ut->username,
                    j
                );

                send_message(msg);
            }
            else {
                char msg[256];
                snprintf(
                    msg, sizeof(msg),
                    "{\"type\":\"RegimenUpdate\","
                    "\"username\":\"%s\","
                    "\"slot\":%d,"
                    "\"done\":%d}",
                    ut->username,
                    j,
                    completeness
                );

                send_message(msg);
            }

        }
    
        if(!stillTraining){
            free(ut);
            for (int k = i; k < b->UnitTrainings.count - 1; k++) {
                b->UnitTrainings.list[k] = b->UnitTrainings.list[k + 1];
            }
            b->UnitTrainings.count--;
            i--;
        }
    }
    pthread_mutex_unlock(&GlobalCache->lock);

    MovementLoop(b);
    //movement
    // for (int i = 0; i < b->Movements.count; i++) {

    // }

    //next bucket
    TickS.currBucket = (TickS.currBucket + 1) % 5;
}

static void* grow_list(void* list, int* capacity, size_t elemSize) {
    if (*capacity == 0) {
        *capacity = 4;
    } else {
        *capacity *= 2;
    }
    return realloc(list, (*capacity) * elemSize);
}

void AddMovementOrder(MovementCommand* Morder) {
    Bucket* b = &TickS.Buckets[TickS.currBucket];
    MovementList* ml = &b->Movements;

    if (ml->count >= ml->capacity) {
        ml->list = grow_list(ml->list, &ml->capacity, sizeof(MovementOrders*));
    }

    MovementOrders* mo = malloc(sizeof(MovementOrders));

    mo->order=Morder;

    // MovementId generation left to you
    generate_task_id(mo->MovementId);

    ml->list[ml->count++] = mo;
}

void AddConstructionOrder(int index,int cx,int cy,int px,int py) {
    Bucket* b = &TickS.Buckets[TickS.currBucket];
    ConstructionList* cl = &b->Constructions;

    if (cl->count >= cl->capacity) {
        cl->list = grow_list(cl->list, &cl->capacity, sizeof(ConstructionOrders*));
    }

    ConstructionOrders* co = malloc(sizeof(ConstructionOrders));
    co->index = index;
    co->x = cx;
    co->y = cy;

    cl->list[cl->count++] = co;
    // printf("added buildingorder to bucket\n");
}


bool user_exists_in_any_bucket(char* username) {
    for (int b = 0; b < 5; b++) {
        UTList* ul = &TickS.Buckets[b].UnitTrainings;
        for (int i = 0; i < ul->count; i++) {
            if (strcmp(ul->list[i]->username, username) == 0) {
                return true;
    }   }   }
    return false;
}

void AddUserWithTrainingOrders(char* username) {

    //check if username is already accounted for in any of the buckets
    bool exists=user_exists_in_any_bucket(username);

    if(!exists){
        Bucket* b = &TickS.Buckets[TickS.currBucket];
        UTList* ul = &b->UnitTrainings;

        if (ul->count >= ul->capacity) {
            ul->list = grow_list(ul->list, &ul->capacity, sizeof(UnitTrainingOrders*));
        }

        UnitTrainingOrders* uto = malloc(sizeof(UnitTrainingOrders));
        strncpy(uto->username, username, sizeof(uto->username));
        uto->username[sizeof(uto->username)-1] = '\0';
        // memcpy(uto->username, username, 256);

        ul->list[ul->count++] = uto;
    }
}
