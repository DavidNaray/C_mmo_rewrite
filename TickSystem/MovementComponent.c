#include "TickSystem.h"
#include "BuildingComponent.h"

#include "../MongoDBReadWriteCache/Cache.h"
#include "../../DistributedNodes/scheduler.h"
#include "../Pathfinding/Pathfinding.h"


void getAbstractPathofMean(User** u,MovementCommand* order,AStarResult** AbsPath){
    //get average, mean position
    double sumX = 0.0;
    double sumY = 0.0;
    int count = 0;

    pthread_mutex_lock(&GlobalCache->lock);    
    *u=cache_get_user(GlobalCache,order->username);
    for (int i = 0; i < order->selectedCount; i++) {
        SelectedUnit *selected = &order->selectedUnits[i];
        Regimen *regiment = NULL;
        for (int r = 0; r < (*u)->regimens.count; r++) {
            if ((*u)->regimens.regimens[r]->id == selected->regiment) {
                regiment = (*u)->regimens.regimens[r];
                break;
        }   }

        if (!regiment){continue;}

        UnitBlock *block = regiment->units[selected->UType];

        if (!block){continue;}

        if (selected->index < 0 || selected->index >= block->count){continue;}

        Unit *unit = &block->individuals[selected->index];
        sumX += unit->position[0] + 512*unit->tile[0];
        sumY += unit->position[1] + 512*unit->tile[1];

        count++;
    }
    pthread_mutex_unlock(&GlobalCache->lock);
    
    if (count == 0){return;}

    double centerX = sumX / count;
    double centerY = sumY / count;

    int tileX = (int)centerX / 512.0;
    int tileY = (int)centerY / 512.0;
    int pixelX = (int)centerX - tileX * 512.0;
    int pixelY = (int)centerY - tileY * 512.0;


    //get the abstractPath from the average to the goal
    WalkMapPoint* Spoint = malloc(sizeof(WalkMapPoint));
    Spoint->x=pixelX;Spoint->y=pixelY;
    Spoint->tx=tileX;Spoint->ty=tileY;

    WalkMapPoint* Epoint = malloc(sizeof(WalkMapPoint));
    Epoint->x=order->pixel[0];Epoint->y=order->pixel[1];
    Epoint->tx=order->tile[0];Epoint->ty=order->tile[1];

    pthread_mutex_lock(&GlobalCache->lock);
    Tile* ST = cache_get_tile(GlobalCache, tileX, tileY);
    Tile* ET = cache_get_tile(GlobalCache, order->tile[0], order->tile[1]);
    
    if (!ST || !ET) {
        pthread_mutex_unlock(&GlobalCache->lock);
        free(Spoint);
        free(Epoint);
        return;
    }

    SubgridPortalRecord* sp=findAbstractPortal(&ST->abstractMap,*Spoint);
    SubgridPortalRecord* ep=findAbstractPortal(&ET->abstractMap,*Epoint);
    pthread_mutex_unlock(&GlobalCache->lock);
    
    if (!sp || !ep) {
        free(Spoint);
        free(Epoint);
        return;
    }

    //abstract path the formation is built around
    *AbsPath=AbstractAStar(sp->localPortal,ep->localPortal);
    free(Spoint);
    free(Epoint);
}

void MovementOrderTask(void *arg){
    MovementOrders us=*(MovementOrders *) arg;
    // free(arg);//dont free arg as its reused to create the task anew each tick

    MovementCommand* order = us.order;
    if (!order || order->selectedCount <= 0) {
        //delete the task
        return;
    }

    AStarResult* AbsPath=NULL;
    User* u=NULL;
    getAbstractPathofMean(&u,order,&AbsPath);

    printf("before unit section?\n");
    if(!u){printf("user is garbage");}
    //dealing with each unit now
    for (int i = 0; i < order->selectedCount; i++) {
        SelectedUnit *selected = &order->selectedUnits[i];
        Regimen *regiment = NULL;
        for (int r = 0; r < u->regimens.count; r++) {
            if (u->regimens.regimens[r]->id == selected->regiment) {
                regiment = u->regimens.regimens[r];
                break;
        }   }

        if (!regiment){continue;}
        UnitBlock *block = regiment->units[selected->UType];
        if (!block){continue;}
        if (selected->index < 0 || selected->index >= block->count){continue;}
        
        Unit* unit = &block->individuals[selected->index];

        WalkMapPoint* Spoint = malloc(sizeof(WalkMapPoint));
        Spoint->x=unit->position[0];Spoint->y=unit->position[1];
        Spoint->tx=unit->tile[0];Spoint->ty=unit->tile[1];

        // -----------swap out for formation point--------
        WalkMapPoint* Epoint = malloc(sizeof(WalkMapPoint));
        Epoint->x=order->pixel[0];Epoint->y=order->pixel[1];
        Epoint->tx=order->tile[0];Epoint->ty=order->tile[1];
        //------------------------------------------------

        pthread_mutex_lock(&GlobalCache->lock);
        Tile* ST = cache_get_tile(GlobalCache, unit->tile[0], unit->tile[1]);
        Tile* ET = cache_get_tile(GlobalCache, order->tile[0], order->tile[1]);
        if (!ST || !ET) {pthread_mutex_unlock(&GlobalCache->lock);free(Spoint);free(Epoint);continue;}

        SubgridPortalRecord* sp=findAbstractPortal(&ST->abstractMap,*Spoint);
        SubgridPortalRecord* ep=findAbstractPortal(&ET->abstractMap,*Epoint);
        pthread_mutex_unlock(&GlobalCache->lock);

        if (!sp || !ep) {free(Spoint);free(Epoint);continue;}

        //abstract path from unit to a destination point
        AStarResult* UnitAbsPath=AbstractAStar(sp->localPortal,ep->localPortal);
        if(UnitAbsPath->count==0){free(Spoint);free(Epoint);continue;}

        //replace the start and end of the path since those were transformed into portals
        UnitAbsPath->route[0]=*Spoint;
        UnitAbsPath->route[UnitAbsPath->count-1]=*Epoint;
        
        //run A* on up to the first 3 nodes of that path, combining segment etc
        ExtractRegion* extractRegion=NULL;
        int nextCount = UnitAbsPath->count < 3 ? UnitAbsPath->count : 3;

        for(int k = 0; k < nextCount; k++){
            Tile* T = cache_get_tile(GlobalCache, UnitAbsPath->route[k].tx, UnitAbsPath->route[k].ty);
            int subx = (int)UnitAbsPath->route[k].x / 32;
            int suby = (int)UnitAbsPath->route[k].y / 32;

            ExtractRegion* currex=extractRegionFunc(T,32*subx,32*suby,32,32);

            if(extractRegion!=NULL){extractRegion=combineSegments(extractRegion,currex);}
            else{extractRegion=currex;}
        }
        
        AStarResult* unitResult=AStarPathCost(
            extractRegion,
            *Spoint,
            UnitAbsPath->route[nextCount-1]
        );

        

        int speed=1;
        if (unitResult->count > 0) {
            WalkMapPoint* oldPos = &unitResult->route[0];

            int newIndex = speed;
            if (newIndex >= unitResult->count) {newIndex = unitResult->count - 1;}
            WalkMapPoint* newPos = &unitResult->route[newIndex];

            pthread_mutex_lock(&GlobalCache->lock);
            Tile* oldTile = cache_get_tile(GlobalCache,oldPos->tx,oldPos->ty);
            if (oldTile) {oldTile->Buffer[oldPos->y][oldPos->x].object = NULL;}

            Tile* newTile = cache_get_tile(GlobalCache,newPos->tx,newPos->ty);
            if (newTile) {
                newTile->Buffer[newPos->y][newPos->x].object = unit;
                // Update the unit's own location
                unit->tile[0] = newPos->tx;
                unit->tile[1] = newPos->ty;
                unit->position[0] = newPos->x;
                unit->position[1] = newPos->y;
            }
            pthread_mutex_unlock(&GlobalCache->lock);

        }
        printf("reached or nost cost;%d,%d\n",unitResult->cost,unitResult->count);
        
        free(Spoint);
        free(Epoint);
        free(UnitAbsPath);
    }
    
    
    free(AbsPath);
}


void MovementLoop(Bucket* b){
    // pthread_mutex_lock(&GlobalCache->lock);
    // pthread_mutex_unlock(&GlobalCache->lock);
    MovementList *ml = &b->Movements;

    for (int i = 0; i < ml->count; i++) {
        MovementOrders *mo = ml->list[i];
        // MovementCommand *order = mo->order;

        //queue 1 dedicated to movement orders
        //is there another task with the same id,,in queue 1 delete it
        remove_task_from_queue(&scheduler.queues[1],mo->MovementId);

        //create a new movementtask for the movementorder into queue
        Task t = {
            .func = MovementOrderTask,
            .arg = mo
        };
        push_task(&scheduler.queues[1], t);
    }
}


