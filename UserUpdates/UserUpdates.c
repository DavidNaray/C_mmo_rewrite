#include "UserUpdates.h"

#include "../MongoDBReadWriteCache/Cache.h"
#include "../MongoDBReadWriteCache/Schema/UserBreakdown.h"
#include "../MongoDBReadWriteCache/ReadUser.h"
#include "../MongoDBReadWriteCache/UserUtils.h"
#include "../MongoDBReadWriteCache/Cache.h"
#include <mongoc/mongoc.h>

#include <cJSON.h>

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

    pthread_mutex_unlock(&GlobalCache->lock);
}

void ConstructionUpdateTask(void *arg){
    
    UUpdate us=*(UUpdate *) arg;
    free(arg);

}

void TrainingUpdateTask(void *arg){
    
    UUpdate us=*(UUpdate *) arg;
    free(arg);

}