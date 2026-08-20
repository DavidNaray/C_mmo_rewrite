#include "TickSystem.h"

#include "../MongoDBReadWriteCache/Cache.h"
#include "../MongoDBReadWriteCache/Schema/UserBreakdown.h"
#include "../MongoDBReadWriteCache/Schema/TileSchema.h"
#include "../MongoDBReadWriteCache/ReadUser.h"
#include "../MongoDBReadWriteCache/Cache.h"
#include <mongoc/mongoc.h>

#include <cJSON.h>
#include <windows.h>



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
    pthread_mutex_lock(&GlobalCache->lock);
    for (int i = 0; i < b->Constructions.count; i++) {
        ConstructionOrders* co = b->Constructions.list[i];
        
        Tile * ftile=cache_get_tile(GlobalCache,co->x,co->y);

        Building* building = ftile->buildings.list[co->index];
        building->base.health++;
        int completeness = (building->base.health * 100) / building->base.maxHealth;
        if (completeness > 100) completeness = 100;
        
        char uniquenames[9][256];
        int uniquecount=TileObservers(ftile,uniquenames);
        
        //alert users of bumped health
        char informPart[512];
        informPart[0] = '\0';  // start empty
        strcat(informPart, "\"inform\":[");
        for(int k = 0; k <  uniquecount; k++){
            strcat(informPart, "\"");
            strcat(informPart, uniquenames[k]);
            strcat(informPart, "\"");
            if (k < uniquecount - 1) strcat(informPart, ",");
        }
        strcat(informPart, "]");
        
        char detailsPart[512];
        snprintf(
            detailsPart, sizeof(detailsPart),
            "\"details\":{"
                "\"Health\":%d,"
                "\"percent\":%d,"
                "\"cx\":%d,"
                "\"cy\":%d,"
                "\"ServerId\":%d,"
                "\"building\":\"%s\""
            "}",
            building->base.health,
            completeness,
            ftile->x,
            ftile->y,
            building->base.ServerId,
            StringFrombType(building->whichBuilding)
        );

        char msg[1024];
        snprintf(
            msg, sizeof(msg),
            "{\"type\":\"BuildingConstructionUpdate\",%s,%s}",
            informPart,
            detailsPart
        );

        send_message(msg);

        if (building->base.health >= building->base.maxHealth) {
            // construction complete
            snprintf(
                detailsPart, sizeof(detailsPart),
                "\"details\":{"
                    "\"cx\":%d,"
                    "\"cy\":%d,"
                    "\"ServerId\":%d,"
                    "\"building\":\"%s\""
                "}",
                ftile->x,
                ftile->y,
                building->base.ServerId,
                StringFrombType(building->whichBuilding)
            );

            snprintf(
                msg, sizeof(msg),
                "{\"type\":\"BuildingOperational\",%s,%s}",
                informPart,
                detailsPart
            );

            send_message(msg);

            //remove the build order
            for (int j = i; j < b->Constructions.count - 1; j++) {
                b->Constructions.list[j] = b->Constructions.list[j + 1];
            }
            b->Constructions.count--;
            i--;   // stay at same index after shift

        }

    }
    pthread_mutex_unlock(&GlobalCache->lock);


    //unit training
    pthread_mutex_lock(&GlobalCache->lock);
    for (int i = 0; i < b->UnitTrainings.count; i++) {
        UnitTrainingOrders* ut = b->UnitTrainings.list[i];

        User* u=cache_get_user(GlobalCache,ut->username);
        if (!u){continue;}

        for (int j = 0; j < MAX_REGIMENS; j++) {
            RegimenTraining* rt = &u->regimenTrainingList.regimens[j];
            if(!rt->active || rt->deployable){continue;}//if false then nothing to do here

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
    }
    pthread_mutex_unlock(&GlobalCache->lock);


    //movement
    for (int i = 0; i < b->Movements.count; i++) {
        // MovementOrders* mo = b->Movements.list[i];

        // Task t;
        // t.func = AStarTask;
        // t.arg = mo;
        // push_task(&scheduler.queues[1], t);
    }

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

void AddMovementOrder(int cx,int cy,int px,int py) {
    Bucket* b = &TickS.Buckets[TickS.currBucket];
    MovementList* ml = &b->Movements;

    if (ml->count >= ml->capacity) {
        ml->list = grow_list(ml->list, &ml->capacity, sizeof(MovementOrders*));
    }

    MovementOrders* mo = malloc(sizeof(MovementOrders));
    mo->Destination[0][0] = cx;
    mo->Destination[0][1] = cy;
    mo->Destination[1][0] = px;
    mo->Destination[1][1] = py;

    // MovementId generation left to you
    memset(mo->MovementId, 0, sizeof(mo->MovementId));

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
