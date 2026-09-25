#include "TickSystem.h"
#include "BuildingComponent.h"

#include "../MongoDBReadWriteCache/Cache.h"
#include "../../DistributedNodes/scheduler.h"
#include "../Pathfinding/Pathfinding.h"


void getAbstractPathofMean(MovementDistilled* order,AStarResult** AbsPath){
    //get average, mean position
    double sumX = 0.0;
    double sumY = 0.0;
    int count = 0;

    for (int i = 0; i < order->selectedCount; i++) {
        Unit* unit = order->selectedUnits[i];
        if (!unit){continue;}

        sumX += unit->position[0] + 512*unit->tile[0];
        sumY += unit->position[1] + 512*unit->tile[1];

        count++;
    }

    
    if (count == 0){return;}

    double centerX = sumX / count;
    double centerY = sumY / count;

    int tileX = (int)centerX / 512.0;
    int tileY = (int)centerY / 512.0;
    int pixelX = (int)centerX - tileX * 512.0;
    int pixelY = (int)centerY - tileY * 512.0;


    //get the abstractPath from the average to the goal
    WalkMapPoint Spoint = {
        .x  = pixelX,
        .y  = pixelY,
        .tx = tileX,
        .ty = tileY
    };

    WalkMapPoint Epoint = order->TargetP;

    pthread_mutex_lock(&GlobalCache->lock);
    Tile* ST = cache_get_tile(GlobalCache, tileX, tileY);
    Tile* ET = cache_get_tile(GlobalCache, Epoint.tx, Epoint.ty);
    
    if (!ST || !ET) {pthread_mutex_unlock(&GlobalCache->lock);return;}

    SubgridPortalRecord* sp=findAbstractPortal(&ST->abstractMap,Spoint);
    SubgridPortalRecord* ep=findAbstractPortal(&ET->abstractMap,Epoint);
    pthread_mutex_unlock(&GlobalCache->lock);
    
    if (!sp || !ep) {return;}

    //abstract path the formation is built around
    *AbsPath=AbstractAStar(sp->localPortal,ep->localPortal);
}

WalkMapPoint advanceFocalPoint(WalkMapPoint current,WalkMapPoint target,int distance) {
    int currentX = current.tx * 512 + current.x;
    int currentY = current.ty * 512 + current.y;

    int targetX = target.tx * 512 + target.x;
    int targetY = target.ty * 512 + target.y;

    int dx = targetX - currentX;
    int dy = targetY - currentY;

    double length = sqrt((double)dx * dx + (double)dy * dy);

    // Already at the target.
    if (length <= 0.0) {return target;}

    // Don't overshoot the waypoint.
    double step = distance;
    if (step >= length) {return target;}

    double nx = dx / length;
    double ny = dy / length;

    int newX = currentX + (int)(nx * step);
    int newY = currentY + (int)(ny * step);

    WalkMapPoint result;

    result.tx = newX / 512;
    result.ty = newY / 512;
    result.x  = newX % 512;
    result.y  = newY % 512;

    result.cost = current.cost;
    result.walkability = true;
    result.object = NULL;

    return result;
}

WalkMapPoint** FormationTarget(MovementDistilled *order, WalkMapPoint focal){
    int n = order->selectedCount;
    WalkMapPoint** targets = malloc(sizeof(WalkMapPoint*) * 2);
    if (!targets) {return NULL;}

    targets[0] = malloc(sizeof(WalkMapPoint) * n);
    targets[1] = malloc(sizeof(WalkMapPoint) * n);
    if (!targets[0] || !targets[1]) {
        free(targets[0]);
        free(targets[1]);
        free(targets);
        return NULL;
    }

    switch (order->Form) {
        case Direct:
            for (int i = 0; i < (n); i++) {
                targets[0][i] = focal;             //local target
                targets[1][i] = order->TargetP;  //final target
            }
            break;
        // later:
        // case Line:
        // case Column:
        // case Wedge:
        // case Square:
    }
    return targets;
}

bool SamePosition(WalkMapPoint a, WalkMapPoint b){
    return a.tx == b.tx && a.ty == b.ty && a.x  == b.x  && a.y  == b.y;
}

void MovementOrderTask(void *arg){
    MovementOrders us=*(MovementOrders *) arg;
    // free(arg);//dont free arg as its reused to create the task anew each tick

    MovementDistilled* order = us.order;
    if (!order || order->selectedCount <= 0) {return;}

    AStarResult* AbsPath=NULL;
    getAbstractPathofMean(order,&AbsPath);
    if(!AbsPath){free(AbsPath);printf("bro, abstract path fail");return;}
    
    //make the final part of the path the target
    AbsPath->route[AbsPath->count - 1]=order->TargetP;

    //advance the focalpoint a bit
    int slowestUnitSpeed=1;
    WalkMapPoint focal = AbsPath->route[0];
    if (AbsPath->count > 1) {
        focal = advanceFocalPoint(AbsPath->route[0],AbsPath->route[1],slowestUnitSpeed);
    }

    WalkMapPoint** targets = FormationTarget(order, focal);
    if (!targets) {free(AbsPath);return;}

    //individual units now
    for (int i = 0; i < order->selectedCount; i++) {
        Unit* unit = order->selectedUnits[i];
        
        if (!unit) {//unit was destroyed
            int last = order->selectedCount - 1;

            order->selectedUnits[i] = order->selectedUnits[last];
            targets[0][i] = targets[0][last];
            targets[1][i] = targets[1][last];
            
            order->selectedCount--;
            i--;    
            continue;
        };

        WalkMapPoint Spoint = {
            .x  = unit->position[0],
            .y  = unit->position[1],
            .tx = unit->tile[0],
            .ty = unit->tile[1]
        };

        // WalkMapPoint Epoint =targets[i];// order->TargetP;
        WalkMapPoint Epoint      = targets[0][i];
        WalkMapPoint FinalTarget = targets[1][i];


        pthread_mutex_lock(&GlobalCache->lock);
        Tile* ST = cache_get_tile(GlobalCache, unit->tile[0], unit->tile[1]);
        Tile* ET = cache_get_tile(GlobalCache, Epoint.tx, Epoint.ty);
        if (!ST || !ET) {pthread_mutex_unlock(&GlobalCache->lock);continue;}

        SubgridPortalRecord* sp=findAbstractPortal(&ST->abstractMap,Spoint);
        SubgridPortalRecord* ep=findAbstractPortal(&ET->abstractMap,Epoint);
        pthread_mutex_unlock(&GlobalCache->lock);

        if (!sp || !ep) {continue;}

        //abstract path from unit to a destination point
        AStarResult* UnitAbsPath=AbstractAStar(sp->localPortal,ep->localPortal);
        if(UnitAbsPath->count==0){free(UnitAbsPath);continue;}

        //replace the start and end of the path since those were transformed into portals
        UnitAbsPath->route[0]=Spoint;
        UnitAbsPath->route[UnitAbsPath->count-1]=Epoint;
        
        //run A* on up to the first 3 nodes of that path, combining segment etc
        ExtractRegion* extractRegion=NULL;
        int nextCount = UnitAbsPath->count < 3 ? UnitAbsPath->count : 3;

        for(int k = 0; k < nextCount; k++){
            Tile* T = cache_get_tile(GlobalCache, UnitAbsPath->route[k].tx, UnitAbsPath->route[k].ty);
            int subx = (int)UnitAbsPath->route[k].x / 32;
            int suby = (int)UnitAbsPath->route[k].y / 32;

            ExtractRegion* currex=extractRegionFunc(T,32*subx,32*suby,32,32);

            if(!extractRegion){extractRegion=currex;}
            else{extractRegion=combineSegments(extractRegion,currex);}
        }

        if (!extractRegion) {free(UnitAbsPath);continue;}
        AStarResult* unitResult=AStarPathCost(extractRegion,Spoint,UnitAbsPath->route[nextCount-1]);

        if(unitResult->count == 1){
            printf(
                "COUNT 1 | Epoint=(tx:%d ty:%d x:%d y:%d) | "
                "FinalTarget=(tx:%d ty:%d x:%d y:%d) | SAME=%d\n",
                Epoint.tx, Epoint.ty, Epoint.x, Epoint.y,
                FinalTarget.tx, FinalTarget.ty, FinalTarget.x, FinalTarget.y,
                SamePosition(Epoint, FinalTarget)
            );
        }
        if (SamePosition(Epoint,FinalTarget) && unitResult->count <= 1) {
            // final destination cannot be reached / unit has arrived
            // remove unit from movement order
            int last = order->selectedCount - 1;

            order->selectedUnits[i] = order->selectedUnits[last];
            targets[0][i] = targets[0][last];
            targets[1][i] = targets[1][last];
            
            order->selectedCount--;
            i--;
            free(unitResult);
            free(extractRegion);
            free(UnitAbsPath);
            continue;    
        }

        int speed=1;
        if (unitResult->count > 0) {
            WalkMapPoint* oldPos = &unitResult->route[0];

            int newIndex = speed;
            if (newIndex >= unitResult->count) {newIndex = unitResult->count - 1;}
            WalkMapPoint* newPos = &unitResult->route[newIndex];

            pthread_mutex_lock(&GlobalCache->lock);
            
            Tile* oldTile = cache_get_tile(GlobalCache,oldPos->tx,oldPos->ty);
            Tile* newTile = cache_get_tile(GlobalCache,newPos->tx,newPos->ty);

            if (oldTile && newTile) {
                oldTile->Buffer[oldPos->y][oldPos->x].object = NULL;
                
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
        
        free(unitResult);
        free(extractRegion);
        free(UnitAbsPath);
    }
    
    free(targets[0]);
    free(targets[1]);
    free(targets);
    free(AbsPath);
}


void MovementLoop(Bucket* b){
    // pthread_mutex_lock(&GlobalCache->lock);
    // pthread_mutex_unlock(&GlobalCache->lock);
    MovementList *ml = &b->Movements;

    for (int i = 0; i < ml->count; i++) {
        MovementOrders *mo = ml->list[i];
        // MovementCommand *order = mo->order;

        if (!mo->order || mo->order->selectedCount == 0) {
            remove_task_from_queue(&scheduler.queues[1],mo->MovementId);
            free(mo->order->selectedUnits);
            free(mo->order);
            free(mo);

            ml->list[i] =
            ml->list[ml->count - 1];

            ml->count--;
            // Re-process whatever we just moved here.
            i--;
            continue;
        }
        
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


