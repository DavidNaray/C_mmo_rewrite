#include "TickSystem.h"
#include "BuildingComponent.h"

#include "../MongoDBReadWriteCache/Cache.h"


void PingHealthUpdate(Tile * ftile,Building* building,int completeness){
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
}

void PingConstructionComplete(Tile * ftile,Building* building){
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

    char msg[1024];
    snprintf(
        msg, sizeof(msg),
        "{\"type\":\"BuildingOperational\",%s,%s}",
        informPart,
        detailsPart
    );

    send_message(msg);
}


void BuildingBenefit(Tile * ftile,Building* building){
    switch (building->whichBuilding) {
        case Barracks:
            break;
        case Factory:
            break;
        case Farm:
            break;
        case LumberMill:
            break;
        case Market:
            break;
        case Quarry:
            break;
        case StoneHouse:
            break;
        case StoneKeep:
            break;
        case TownHall:
            GetCityCenters(ftile->usernames[4]);//alert user of citycenters
            break;
        case warehouse:
            break;
        case WoodenKeep:
            break;
        case WoodenTower:
            break;
        case WoodHouse:
            break;
        default:
            // all the other buildings that dont actually add specific values/ resources
            break;
    }
}


void BuildingLoop(Bucket* b){
    //construction
    pthread_mutex_lock(&GlobalCache->lock);

    for (int i = 0; i < b->Constructions.count; i++) {

        ConstructionOrders* co = b->Constructions.list[i];
        
        Tile * ftile=cache_get_tile(GlobalCache,co->x,co->y);

        Building* building = ftile->buildings.list[co->index];
        building->base.health++;

        int completeness = (building->base.health * 100) / building->base.maxHealth;
        if (completeness > 100){completeness = 100;}//clamp the completeness
        
        //alert relevant users about the health change of the building
        PingHealthUpdate(ftile,building,completeness);

        if (building->base.health >= building->base.maxHealth) {
            // construction complete
            building->built=true;//flag to say it was built for the first time
            
            PingConstructionComplete(ftile,building);

            //add the benefits of the building for the user
            BuildingBenefit(ftile,building);

            //remove the build order now that it is completed
            for (int j = i; j < b->Constructions.count - 1; j++) {
                b->Constructions.list[j] = b->Constructions.list[j + 1];
            }
            b->Constructions.count--;
            i--;   // stay at same index after shift
        }
    }

    pthread_mutex_unlock(&GlobalCache->lock);
}


