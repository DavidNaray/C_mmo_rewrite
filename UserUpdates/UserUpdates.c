#include "UserUpdates.h"

#include "../MongoDBReadWriteCache/Cache.h"
#include "../MongoDBReadWriteCache/Schema/UserBreakdown.h"
#include "../MongoDBReadWriteCache/ReadUser.h"
#include "../MongoDBReadWriteCache/UserUtils.h"
#include "../MongoDBReadWriteCache/Cache.h"
#include <mongoc/mongoc.h>

#include <cJSON.h>
#include <windows.h>

static RegimentTemplate RegimentTemplates[MAX_REGIMENS] = {
    { "ARCHER",     {1, 0, 0} },
    { "SPEARMAN",   {0, 1, 0} },
    { "SWORDSMAN",  {0, 0, 1} }
    // user-defined templates will be loaded here later
};

static cJSON* tech_to_json(const Tech* t){
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(obj, "unlocked", t->unlocked);
    cJSON_AddStringToObject(obj, "Description", t->Description);
    return obj;
}

static cJSON* technologies_to_json(const Technologies* T){
    cJSON* root = cJSON_CreateObject();

    cJSON_AddItemToObject(root, "Bows", tech_to_json(&T->Bows));
    cJSON_AddItemToObject(root, "Swords", tech_to_json(&T->Swords));
    cJSON_AddItemToObject(root, "Shields", tech_to_json(&T->Shields));
    cJSON_AddItemToObject(root, "Spears", tech_to_json(&T->Spears));
    cJSON_AddItemToObject(root, "LeatherArmour", tech_to_json(&T->LeatherArmour));
    cJSON_AddItemToObject(root, "BatteringRam", tech_to_json(&T->BatteringRam));
    cJSON_AddItemToObject(root, "WagonFort", tech_to_json(&T->WagonFort));
    cJSON_AddItemToObject(root, "WoodWall", tech_to_json(&T->WoodWall));
    cJSON_AddItemToObject(root, "StoneWall", tech_to_json(&T->StoneWall));
    cJSON_AddItemToObject(root, "WoodGate", tech_to_json(&T->WoodGate));
    cJSON_AddItemToObject(root, "StoneGate", tech_to_json(&T->StoneGate));
    cJSON_AddItemToObject(root, "WoodenTower", tech_to_json(&T->WoodenTower));
    cJSON_AddItemToObject(root, "StoneTower", tech_to_json(&T->StoneTower));
    cJSON_AddItemToObject(root, "WoodenKeep", tech_to_json(&T->WoodenKeep));
    cJSON_AddItemToObject(root, "StoneKeep", tech_to_json(&T->StoneKeep));
    cJSON_AddItemToObject(root, "WoodHouse", tech_to_json(&T->WoodHouse));
    cJSON_AddItemToObject(root, "StoneHouse", tech_to_json(&T->StoneHouse));
    cJSON_AddItemToObject(root, "Pavement", tech_to_json(&T->Pavement));

    cJSON_AddItemToObject(root, "CivilianFactory", tech_to_json(&T->CivilianFactory));
    cJSON_AddItemToObject(root, "MilitaryFactory", tech_to_json(&T->MilitaryFactory));
    cJSON_AddItemToObject(root, "Farm", tech_to_json(&T->Farm));
    cJSON_AddItemToObject(root, "Quarry", tech_to_json(&T->Quarry));
    cJSON_AddItemToObject(root, "LumberMill", tech_to_json(&T->LumberMill));
    cJSON_AddItemToObject(root, "Barracks", tech_to_json(&T->Barracks));
    cJSON_AddItemToObject(root, "Market", tech_to_json(&T->Market));
    cJSON_AddItemToObject(root, "TownHall", tech_to_json(&T->TownHall));
    cJSON_AddItemToObject(root, "Warehouse", tech_to_json(&T->Warehouse));

    cJSON_AddItemToObject(root, "ChainArmour", tech_to_json(&T->ChainArmour));
    cJSON_AddItemToObject(root, "PlateArmour", tech_to_json(&T->PlateArmour));
    cJSON_AddItemToObject(root, "Crossbows", tech_to_json(&T->Crossbows));
    cJSON_AddItemToObject(root, "Trebuchet", tech_to_json(&T->Trebuchet));
    cJSON_AddItemToObject(root, "Catapult", tech_to_json(&T->Catapult));
    cJSON_AddItemToObject(root, "Ballista", tech_to_json(&T->Ballista));

    cJSON_AddItemToObject(root, "StandardisedParts", tech_to_json(&T->StandardisedParts));
    cJSON_AddItemToObject(root, "RobustSupplyChains", tech_to_json(&T->RobustSupplyChains));
    cJSON_AddItemToObject(root, "WorkerShifts", tech_to_json(&T->WorkerShifts));
    cJSON_AddItemToObject(root, "FortifiedSettlements", tech_to_json(&T->FortifiedSettlements));
    cJSON_AddItemToObject(root, "CropRotation", tech_to_json(&T->CropRotation));

    return root;
}


void TechUpdateTask(void *arg){
    UUpdate us=*(UUpdate *) arg;
    pthread_mutex_lock(&GlobalCache->lock);

    User* u=cache_get_user(GlobalCache,us.username);

    pthread_mutex_unlock(&GlobalCache->lock);

    char header[256];
    int header_len = snprintf(
        header, sizeof(header),
        "{\"type\":\"TechDetails\",\"username\":\"%s\",\"details\":",
        us.username
    );
    
    cJSON* tech_json = technologies_to_json(&u->Technologies);
    char* tech_str  = cJSON_PrintUnformatted(tech_json);

    size_t tech_len = strlen(tech_str);
    size_t total_len = header_len + tech_len + 2;
    
    char* final = malloc(total_len);
    memcpy(final, header, header_len);
    memcpy(final + header_len, tech_str, tech_len);
    final[header_len + tech_len] = '}';
    final[header_len + tech_len + 1] = '\0';

    send_message(final);
    
    free(tech_str);
    free(final);
    free(arg);
    cJSON_Delete(tech_json);

}


static cJSON *TrainingList_to_json(const RegimenTrainingList *TL){
    cJSON *root = cJSON_CreateObject();
    cJSON *regimens = cJSON_AddArrayToObject(root, "regimens");

    static const char *unitNames[] = {
        "archer",
        "spearman",
        "swordsman"
    };

    for (int i = 0; i < MAX_REGIMENS; i++) {
        const RegimenTraining *r = &TL->regimens[i];

        if (!r->active){continue;}

        cJSON *reg = cJSON_CreateObject();
        cJSON_AddItemToArray(regimens, reg);
        cJSON_AddStringToObject(reg, "regName", r->name);
        cJSON_AddNumberToObject(reg, "slot", i);

        cJSON *tile = cJSON_AddArrayToObject(reg, "deployTile");
        cJSON_AddItemToArray(tile, cJSON_CreateNumber(r->deployTile[0]));
        cJSON_AddItemToArray(tile, cJSON_CreateNumber(r->deployTile[1]));

        cJSON *pixel = cJSON_AddArrayToObject(reg, "deployPixel");
        cJSON_AddItemToArray(pixel, cJSON_CreateNumber(r->deployPixel[0]));
        cJSON_AddItemToArray(pixel, cJSON_CreateNumber(r->deployPixel[1]));



        cJSON *units = cJSON_AddObjectToObject(reg, "units");

        for (int j = 0; j < UNIT_MAX; j++) {
            cJSON *u = cJSON_AddObjectToObject(units, unitNames[j]);

            cJSON_AddNumberToObject(u, "count", r->units[j].count);
            cJSON_AddNumberToObject(u, "progress", r->units[j].progress);
            cJSON_AddNumberToObject(u, "finish", r->units[j].finish);
        }
    }

    return root;
}


void TrainingUpdateTask(void *arg){
    
    UUpdate us=*(UUpdate *) arg;

    pthread_mutex_lock(&GlobalCache->lock);
    User* u=cache_get_user(GlobalCache,us.username);
    pthread_mutex_unlock(&GlobalCache->lock);

    char header[256];
    int header_len = snprintf(
        header, sizeof(header),
        "{\"type\":\"TrainingDetails\",\"username\":\"%s\",\"details\":",
        us.username
    );

    cJSON* TL_json = TrainingList_to_json(&u->regimenTrainingList);
    char* TL_str  = cJSON_PrintUnformatted(TL_json);

    size_t TL_len = strlen(TL_str);
    size_t total_len = header_len + TL_len + 2;
    
    char* final = malloc(total_len);
    memcpy(final, header, header_len);
    memcpy(final + header_len, TL_str, TL_len);
    final[header_len + TL_len] = '}';
    final[header_len + TL_len + 1] = '\0';

    send_message(final);

    free(TL_str);
    free(final);
    free(arg);
    cJSON_Delete(TL_json);
}

void NewRegimenTask(void *arg){


    RegUpdate us=*(RegUpdate *) arg;
    free(arg);

    pthread_mutex_lock(&GlobalCache->lock);
    User* u=cache_get_user(GlobalCache,us.username);
    pthread_mutex_unlock(&GlobalCache->lock);

    int freeIndex = -1;
    for (int i = 0; i < MAX_REGIMENS; i++) {
        if (!u->regimenTrainingList.regimens[i].active) {
            freeIndex = i;
            break;
    }   }
    
    if (freeIndex == -1) {// no available training slot
        char header[256];
        int header_len = snprintf(
            header, sizeof(header),
            "{\"type\":\"NewTrainingFailed\",\"username\":\"%s\"}",
            us.username
        );
        send_message(header);
    }
    else{
        //add the regimen to those in training
        //name must correspond to a template type
        int templateIndex=-1;
        for (int i = 0; i < MAX_REGIMENS; i++) {
            if (strcmp(RegimentTemplates[i].name, us.regName) == 0) {templateIndex=i;}
        }
        if(templateIndex==-1){
            char header[256];
            int header_len = snprintf(
                header, sizeof(header),
                "{\"type\":\"NewTrainingFailed\",\"username\":\"%s\"}",
                us.username
            );
            send_message(header);
        }
        else{
            pthread_mutex_lock(&GlobalCache->lock);
            RegimenTraining* rt = &u->regimenTrainingList.regimens[freeIndex];
            rt->active = true;
            
            strncpy(rt->name, us.regName, sizeof(rt->name));
            rt->name[sizeof(rt->name)-1] = '\0';

            // initialize units
            for (int i = 0; i < UNIT_MAX; i++) {
                rt->units[i].count = RegimentTemplates[templateIndex].units[i];
                rt->units[i].progress = 0;
                rt->units[i].finish = 60;//just some kind of timer, 60 seconds
            }

            pthread_mutex_unlock(&GlobalCache->lock);

            char msg[256];
            snprintf(
                msg, sizeof(msg),
                "{\"type\":\"NewTrainingSuccess\",\"username\":\"%s\",\"slot\":%d,\"name\":\"%s\"}",
                us.username, freeIndex, us.regName
            );
            send_message(msg);
        }
    }
}



void add_tile_json(Tile * t,cJSON* root){
    //append the information on top of the root
    cJSON *tiles = cJSON_GetObjectItem(root, "tiles");

    // create tile object
    cJSON *tileObj = cJSON_CreateObject();
    cJSON_AddNumberToObject(tileObj, "x", t->x);
    cJSON_AddNumberToObject(tileObj, "y", t->y);

    // textures object
    cJSON *textures = cJSON_CreateObject();
    cJSON_AddStringToObject(textures, "texturemapUrl", t->textures.texturemapUrl);
    cJSON_AddStringToObject(textures, "heightmapUrl", t->textures.heightmapUrl);

    cJSON_AddItemToObject(tileObj, "textures", textures);

    // cJSON *usernames = cJSON_CreateArray();
    // for (int i = 0; i < 9; i++) {
    //     if (t->usernames[i][0] != '\0') {   // skip empty entries
    //         cJSON_AddItemToArray(usernames, cJSON_CreateString(t->usernames[i]));
    // }   }

    // cJSON_AddItemToObject(tileObj, "usernames", usernames);

    // append tile to tiles array
    cJSON_AddItemToArray(tiles, tileObj);
}

int visited_contains(Tile** visited, int visitedCount, Tile* t) {
    for (int i = 0; i < visitedCount; i++) {
        if (visited[i] == t) return 1;
    }
    return 0;
}

void recursiveTilePiece(Tile * focusTile,char* username,Tile** visitedList,int* visitedCount,cJSON* root){
    visitedList[*visitedCount] = focusTile;
    (*visitedCount)++;

    //check all surrounding tiles (skip index 4)
    for(int rows=-1;rows<2;rows++){for(int cols=-1;cols<2;cols++){
        if(rows==0 && cols==0){continue;}
            
        //get the tile at the location 
        // pthread_mutex_lock(&GlobalCache->lock);
        Tile * ftile=cache_get_tile(GlobalCache,focusTile->x + cols,focusTile->y + rows);
        if(ftile == NULL){continue;}

        //check if neighbour in visited
        if (visited_contains(visitedList, *visitedCount, ftile)){continue;}

        //does the found tile contain the username?
        for(int index=0;index<9;index++){
            if (strcmp(ftile->usernames[index], username) == 0){
                //add the tile info to the json
                add_tile_json(ftile,root);

                // pthread_mutex_unlock(&GlobalCache->lock);

                //keep the search going
                recursiveTilePiece(ftile,username,visitedList,visitedCount,root);

                //no point in looking further if found their name
                break;
            }
        }

    }   }
}

void GetUserTiles(void *arg){
    UUpdate us=*(UUpdate *) arg;
    free(arg);
    // printf("do i even get ehre \n");
    
    pthread_mutex_lock(&GlobalCache->lock);
    User* u=cache_get_user(GlobalCache,us.username);
    pthread_mutex_unlock(&GlobalCache->lock);

    // printf("user deets:%d,%d \n",u->originTile[0], u->originTile[1]);
    printf("LOOKUP KEY = '%d,%d'\n", u->originTile[0], u->originTile[1]);
    Tile* focusTile = NULL;
    for (int i = 0; i < 100; i++) {   // retry up to 100 times
        pthread_mutex_lock(&GlobalCache->lock);
        focusTile = cache_get_tile(GlobalCache, u->originTile[0], u->originTile[1]);//x,y
        pthread_mutex_unlock(&GlobalCache->lock);

        if (focusTile != NULL) {break;}
        Sleep(100); // sleep 100ms
    }

    if(focusTile == NULL){printf("bruh, null tile \n");return;}

    //start tracking what tiles have been visited
    int size=1024;
    Tile** visitedList = malloc(size * sizeof(Tile*));//list of pointers to tiles
    int visitedCount = 0;

    cJSON* root = cJSON_CreateObject();
    cJSON *tiles = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "tiles", tiles);

    cJSON *ogtile = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "origintile", ogtile);
    cJSON_AddItemToArray(ogtile, cJSON_CreateNumber(u->originTile[0]));
    cJSON_AddItemToArray(ogtile, cJSON_CreateNumber(u->originTile[1]));

    // printf("focus tile usernames \n");
    cJSON *usernames = cJSON_CreateArray();
    for (int i = 0; i < 9; i++) {
        if (focusTile->usernames[i][0] != '\0') {   // skip empty entries
            cJSON_AddItemToArray(usernames, cJSON_CreateString(focusTile->usernames[i]));
    }   }
    cJSON_AddItemToObject(root, "usernames", usernames);

    add_tile_json(focusTile,root);

    // printf("before recursivetilepiece \n");
    //starting at the origin tile, do a spread search
    recursiveTilePiece(focusTile,us.username,visitedList, &visitedCount,root);

    char* tiles_str  = cJSON_PrintUnformatted(root);
    
    char header[256];
    int header_len = snprintf(
        header, sizeof(header),
        "{\"type\":\"TileDetails\",\"username\":\"%s\",\"details\":",
        us.username
    );

    size_t tiles_len = strlen(tiles_str);
    size_t total_len = header_len + tiles_len + 2;
    
    char* final = malloc(total_len);
    memcpy(final, header, header_len);
    memcpy(final + header_len, tiles_str, tiles_len);
    final[header_len + tiles_len] = '}';
    final[header_len + tiles_len + 1] = '\0';

    send_message(final);

    free(tiles_str);
    free(final);
    free(visitedList);
    cJSON_Delete(root);
}


void ConstructionUpdateTask(void *arg){
    
    UUpdate us=*(UUpdate *) arg;
    free(arg);

}