#include "UserUpdates.h"

#include "../MongoDBReadWriteCache/Cache.h"
#include "../MongoDBReadWriteCache/Schema/UserBreakdown.h"
#include "../MongoDBReadWriteCache/Schema/TileSchema.h"
#include "../MongoDBReadWriteCache/ReadUser.h"
#include "../MongoDBReadWriteCache/UserUtils.h"
#include "../MongoDBReadWriteCache/Cache.h"
#include <mongoc/mongoc.h>

#include <cJSON.h>
#include <windows.h>


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
    cJSON_AddItemToObject(root, "Pavement", tech_to_json(&T->Pavement));

    cJSON_AddItemToObject(root, "ChainArmour", tech_to_json(&T->ChainArmour));
    cJSON_AddItemToObject(root, "PlateArmour", tech_to_json(&T->PlateArmour));
    cJSON_AddItemToObject(root, "Crossbows", tech_to_json(&T->Crossbows));
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
    // printf("uh should be ok with new reg?%d%s",freeIndex,us.regName);
    
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
        int templateIndex=-1;
        for (int j = 0; j < UNIT_MAX; j++) {
            if (strcmp(RegimenTemplates[j].name, us.regName) == 0) {templateIndex=j;}
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
            *rt = RegimenTemplates[templateIndex];
            rt->active = true;
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



static cJSON* Buildings_to_json(const BuildingsTypes* T){
    cJSON* root = cJSON_CreateObject();

    cJSON_AddItemToObject(root, "Barracks", tech_to_json(&T->Barracks));
    cJSON_AddItemToObject(root, "Factory", tech_to_json(&T->Factory));
    cJSON_AddItemToObject(root, "Farm", tech_to_json(&T->Farm));
    cJSON_AddItemToObject(root, "LumberMill", tech_to_json(&T->LumberMill));
    cJSON_AddItemToObject(root, "Market", tech_to_json(&T->Market));
    cJSON_AddItemToObject(root, "Quarry", tech_to_json(&T->Quarry));
    cJSON_AddItemToObject(root, "StoneGate", tech_to_json(&T->StoneGate));
    cJSON_AddItemToObject(root, "StoneHouse", tech_to_json(&T->StoneHouse));
    cJSON_AddItemToObject(root, "StoneKeep", tech_to_json(&T->StoneKeep));
    cJSON_AddItemToObject(root, "StoneTower", tech_to_json(&T->StoneTower));

    cJSON_AddItemToObject(root, "StoneWall", tech_to_json(&T->StoneWall));

    cJSON_AddItemToObject(root, "TownHall", tech_to_json(&T->TownHall));
    cJSON_AddItemToObject(root, "warehouse", tech_to_json(&T->warehouse));
    cJSON_AddItemToObject(root, "WoodenKeep", tech_to_json(&T->WoodenKeep));
    cJSON_AddItemToObject(root, "WoodenTower", tech_to_json(&T->WoodenTower));

    cJSON_AddItemToObject(root, "WoodGate", tech_to_json(&T->WoodGate));
    cJSON_AddItemToObject(root, "WoodHouse", tech_to_json(&T->WoodHouse));
    cJSON_AddItemToObject(root, "WoodWall", tech_to_json(&T->WoodWall));

    return root;
}

void ConstructionUpdateTask(void *arg){
    // printf("huh construct \n");
    UUpdate us=*(UUpdate *) arg;
    pthread_mutex_lock(&GlobalCache->lock);

    User* u=cache_get_user(GlobalCache,us.username);

    pthread_mutex_unlock(&GlobalCache->lock);

    char header[256];
    int header_len = snprintf(
        header, sizeof(header),
        "{\"type\":\"ConstructionOptions\",\"username\":\"%s\",\"details\":",
        us.username
    );
    
    cJSON* Build_json = Buildings_to_json(&u->Buildings);
    char* build_str  = cJSON_PrintUnformatted(Build_json);

    size_t build_len = strlen(build_str);
    size_t total_len = header_len + build_len + 2;
    
    char* final = malloc(total_len);
    memcpy(final, header, header_len);
    memcpy(final + header_len, build_str, build_len);
    final[header_len + build_len] = '}';
    final[header_len + build_len + 1] = '\0';

    send_message(final);
    
    free(build_str);
    free(final);
    free(arg);
    cJSON_Delete(Build_json);

}

bool canplacebuilding(WalkMapPoint buffer[512][512], Building template,int xp,int yp){
    //building width and height
    int bw = template.base.width;
    int bh = template.base.height;

    //bounds
    int left   = xp - bw/2;
    int right  = xp + bw/2;
    int top    = yp - bh/2;
    int bottom = yp + bh/2;
    if (left < 0 || right >= 512 || top < 0 || bottom >= 512){return false;}

    for (int y = top; y <= bottom; y++) {
        for (int x = left; x <= right; x++) {
            WalkMapPoint *cell = &buffer[y][x];
            if (!cell->walkability){return false;}
            if (cell->object != NULL){return false;}
    }   }

    return true;
}

void BuildingPosUpdateTask(void *arg){

    //you have the username and pos
    //you can get the chunk it belongs to by its offset to the users origin tile

    //tiles are 7.5 tiles across, centered on the origin so so thats +3.75 since its moved left
        //in other words seeing 0,0,0 is actually 3.75,0,3.75

    //the -0.00001f is to prevent a bug since the edges are shifted so 3.75 turns to 7.5 meaning /7.5=1
        //but this means the exact right edge of tile 0 shifts into tile 1, which will cause problems

    BuildPlacement us=*(BuildPlacement *) arg;
    
    pthread_mutex_lock(&GlobalCache->lock);
    User* u=cache_get_user(GlobalCache,us.username);

    double pixelsPerUnit = 512.0 / 7.5;   // ≈ 68.2666667
    double px = (us.position[0]+3.75f) * pixelsPerUnit;
    double py = (us.position[2]+3.75f) * pixelsPerUnit;

    int pxf=(int)px;
    int pyf=(int)py;
    // printf("%d,%d\n",pxf,pyf);

    double xchunk=pxf/512.0;//(us.position[0] + 3.75f - 0.00001f) / 7.5f;
    double ychunk=pyf/512.0;//(us.position[2] + 3.75f - 0.00001f) / 7.5f;

    int xfloored=(int)xchunk;
    int yfloored=(int)ychunk;

    int tilepixelx=pxf - 512*xfloored;
    int tilepixely=pyf - 512*yfloored;
    // printf("%d,%d\n",xfloored,yfloored);
    Tile* focusTile = cache_get_tile(GlobalCache, xfloored, yfloored);
    pthread_mutex_unlock(&GlobalCache->lock);

    //get the info for the appropriate building
    //and buffer for the tile
    //get the pixel positions

    // int xPixel=(xchunk - xfloored);
    // int yPixel=(ychunk - yfloored);

    bool permission=canplacebuilding(
        focusTile->Buffer,
        BuildingTemplates[bTypeFromString(us.buildingname)],
        tilepixelx,tilepixely
    );

    printf("commed the id %s\n",us.taskId);
    char msg[256];
    snprintf(
        msg, sizeof(msg),
        "{\"type\":\"PlacementMovement\","
        "\"username\":\"%s\","
        "\"permission\":%d,"
        "\"id\":\"%s\","
        "\"px\":%d,"
        "\"py\":%d,"
        "\"cx\":%d,"
        "\"cy\":%d,"
        "\"building\":\"%s\"}",
        us.username,
        permission,
        us.taskId,
        tilepixelx,
        tilepixely,
        xfloored,
        yfloored,
        us.buildingname
    );

    send_message(msg);

}

int get_unique_usernames(char usernames[9][256], char out[9][256]) {
    int count = 0;

    for (int i = 0; i < 9; i++) {
        if (usernames[i][0] == '\0') continue;  // skip empty

        // check if already added
        bool exists = false;
        for (int j = 0; j < count; j++) {
            if (strcmp(out[j], usernames[i]) == 0) {
                exists = true;
                break;
            }
        }

        if (!exists) {
            strcpy(out[count], usernames[i]);
            count++;
        }
    }

    return count;
}


void BuildingPlaceTask(void *arg){
    BuildPlacement us=*(BuildPlacement *) arg;
    
    pthread_mutex_lock(&GlobalCache->lock);
    User* u=cache_get_user(GlobalCache,us.username);

    double pixelsPerUnit = 512.0 / 7.5;   // ≈ 68.2666667
    double px = (us.position[0]+3.75f) * pixelsPerUnit;
    double py = (us.position[2]+3.75f) * pixelsPerUnit;

    int pxf=(int)px;
    int pyf=(int)py;

    double xchunk=pxf/512.0;
    double ychunk=pyf/512.0;

    int xfloored=(int)xchunk;
    int yfloored=(int)ychunk;

    int tilepixelx=pxf - 512*xfloored;
    int tilepixely=pyf - 512*yfloored;

    Tile* focusTile = cache_get_tile(GlobalCache, xfloored, yfloored);

    bool enoughRoom=canplacebuilding(
        focusTile->Buffer,
        BuildingTemplates[bTypeFromString(us.buildingname)],
        tilepixelx,tilepixely
    );
    // printf("hello");

    //add the building to the tile
    if(enoughRoom){
        cache_addbuilding_tile(focusTile,
            tilepixelx,
            tilepixely,
            BuildingTemplates[bTypeFromString(us.buildingname)]
        );
    }
    int count=focusTile->buildings.count;
    int ServerId=focusTile->buildings.list[count-1]->base.ServerId;
    printf("%d\n",ServerId);
    pthread_mutex_unlock(&GlobalCache->lock);

    //message the all users observing the tile
    char uniquenames[9][256];
    int uniquecount=get_unique_usernames(focusTile->usernames,uniquenames);

    char informPart[512];
    informPart[0] = '\0';  // start empty
    strcat(informPart, "\"inform\":[");
    for (int u = 0; u < uniquecount; u++) {
        strcat(informPart, "\"");
        strcat(informPart, uniquenames[u]);
        strcat(informPart, "\"");
        if (u < uniquecount - 1) strcat(informPart, ",");
    }
    strcat(informPart, "]");

    char detailsPart[512];
    snprintf(
        detailsPart, sizeof(detailsPart),
        "\"details\":{"
            "\"px\":%d,"
            "\"py\":%d,"
            "\"cx\":%d,"
            "\"cy\":%d,"
            "\"ServerId\":%d,"
            "\"building\":\"%s\""
        "}",
        tilepixelx,
        tilepixely,
        xfloored,
        yfloored,
        ServerId,
        us.buildingname
    );

    char msg[1024];
    snprintf(
        msg, sizeof(msg),
        "{\"type\":\"BuildingPlaced\",%s,%s}",
        informPart,
        detailsPart
    );

    send_message(msg);
}