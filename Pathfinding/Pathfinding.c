#include "Pathfinding.h"

#include "../MongoDBReadWriteCache/Cache.h"
#include "../MongoDBReadWriteCache/Schema/UserBreakdown.h"
#include "../MongoDBReadWriteCache/ReadUser.h"
#include "../MongoDBReadWriteCache/Cache.h"
#include <mongoc/mongoc.h>

#include <limits.h> // Required for INT_MAX and INT_MIN

#include <cJSON.h>
#include <windows.h>
#include <float.h>



void push_Node(PriorityQueue* q, WalkMapPoint val, float prio) {
    q->size++;

    Node* n = malloc(sizeof(Node));
    n->val = val;
    n->priority = prio;
    n->next = NULL;

    // empty queue
    if (q->head == NULL) {q->head = n;q->tail = n;return;}

    // insert at head if priority is smallest
    if (prio < q->head->priority) {
        n->next = q->head;
        q->head = n;
        return;
    }

    // find insertion point
    Node* cur = q->head;
    while (cur->next != NULL && cur->next->priority <= prio) {
        cur = cur->next;
    }

    // insert after cur
    n->next = cur->next;
    cur->next = n;

    // update tail if inserted at end
    if (n->next == NULL){q->tail = n;}
}

Node* pop_Node(PriorityQueue *q) {
    Node *node = q->head;
    if (node == NULL) {/*theres nothing to get so bail*/return NULL;}

    q->head = node->next;
    if (q->head == NULL) {/*if the node after next is null, tail must be null*/q->tail = NULL;}

    q->size--;
    return node;
}


ExtractRegion* extractRegion(Tile* t, int StartX, int StartY, int segW, int segH){
    //function dedicated to cutting 32x32 cuts from a tiles buffer
    // WalkMapPoint srcbuffer[512][512]
    size_t count = segW * segH;

    ExtractRegion* cut = malloc(sizeof(ExtractRegion) + count * sizeof(WalkMapPoint));
    cut->width=segW;
    cut->height=segH;
    cut->originX = (512 * t->x) + StartX;
    cut->originY = (512 * t->y) + StartY;

    //copies over the segments of rows from the srcbuffer (faster than x,y iteration)
    for (int y = 0; y < segH; y++) {
        memcpy(&cut->Buffer[y * segW],&t->Buffer[StartY + y][StartX],segW * sizeof(WalkMapPoint));
    }

    return cut;
}


void copyRegion(
    ExtractRegion* out, int width,
    const ExtractRegion* src, 
    int destX, int destY
){
    for(int y = 0; y < src->height; y++){
        memcpy(
            &out->Buffer[(destY + y) * width + destX],
            &src->Buffer[y * src->width],
            src->width * sizeof(WalkMapPoint)
        );
    }
}

ExtractRegion* combineSegments(const ExtractRegion* bufA,const ExtractRegion* bufB){
    int posAx=bufA->originX;int posAy=bufA->originY;
    int posBx=bufB->originX;int posBy=bufB->originY;

    // 1. Compute global bounding box
    int minX = posAx < posBx ? posAx : posBx;
    int minY = posAy < posBy ? posAy : posBy;

    //because bufferA and B can have different dimensions the max must account for these

    int maxX = (posAx + bufA->width) > (posBx + bufB->width)
        ? (posAx + bufA->width)
        : (posBx + bufB->width);

    int maxY = (posAy + bufA->height) > (posBy + bufB->height)
        ? (posAy + bufA->height)
        : (posBy + bufB->height);

    int width  = maxX - minX;
    int height = maxY - minY;

    // 2. Allocate combined region
    ExtractRegion* out = malloc(
        sizeof(ExtractRegion) +
        width * height * sizeof(WalkMapPoint)
    );

    out->width   = width;
    out->height  = height;
    out->originX = minX;
    out->originY = minY;

    // make sure to set things as unwalkable
    for (int i = 0; i < width * height; i++) {
        out->Buffer[i].cost = INT_MAX;      // or whatever "infinite" means
        out->Buffer[i].walkability = false;
        out->Buffer[i].object = NULL;
    }

    // 4. Place A and B relative to minX/minY
    int destAx = posAx - minX;
    int destAy = posAy - minY;

    int destBx = posBx - minX;
    int destBy = posBy - minY;

    copyRegion(out,width,bufA, destAx, destAy);
    copyRegion(out,width,bufB, destBx, destBy);

    return out;
}



static inline float heuristic(WalkMapPoint startP,WalkMapPoint goalP){
    int x1=(512*startP.tx) + startP.x;
    int y1=(512*startP.ty) + startP.y;
    int x2=(512*goalP.tx ) + goalP.x;
    int y2=(512*goalP.ty ) + goalP.y;
    
    float dx = x2 - x1;
    float dy = y2 - y1;
    return sqrtf(dx*dx + dy*dy);
}

int BufferLocalIndex(ExtractRegion* AB,WalkMapPoint p){
    int PGX=(p.tx*512) + p.x;
    int PGY=(p.ty*512) + p.y;

    //if the p is on the buffers origin then BX and BY will be 0
    int BX=PGX - AB->originX;
    int BY=PGY - AB->originY;
    
    if (BX < 0 || BY < 0 || BX >= AB->width || BY >= AB->height){
        printf("omg bufferlocal out");
        return -1; 
    }

    int index=(BY*AB->width) + BX;

    return index;
}

AStarResult* AStarPathCost(
    ExtractRegion* AreaBuffer,
    WalkMapPoint startP,WalkMapPoint goalP
) {
    const int dirs[4][2] = {
        {1,0},{-1,0},
        {0,1},{0,-1}
    };
    int size=AreaBuffer->width * AreaBuffer->height;

    bool* visited = calloc(size, sizeof(bool)); // all false
    float* gScore = malloc(size * sizeof(float));
    float* fScore = malloc(size * sizeof(float));
    int* cameFrom = malloc(size * sizeof(int));

    for (int i = 0; i < size; i++) {
        gScore[i] = FLT_MAX;
        fScore[i] = FLT_MAX;
        cameFrom[i] = -1;
    }

    int startIdx=BufferLocalIndex(AreaBuffer,startP);
    int goalIdx  = BufferLocalIndex(AreaBuffer, goalP);

    gScore[startIdx] = 0;
    fScore[startIdx] = heuristic(startP, goalP);

    PriorityQueue open={ .head=NULL, .tail=NULL, .size=0 };
    push_Node(&open,startP,fScore[startIdx]);

    while (open.size>0) {
        Node* current = pop_Node(&open);
        WalkMapPoint curP = current->val;
        free(current);
        int curIdx = BufferLocalIndex(AreaBuffer,curP);
        

        if(visited[curIdx]){continue;}
        visited[curIdx]=true;

        if (curIdx == goalIdx) {//destination reached
            break;
        }

        //index components of the current 
        int cx = curIdx % AreaBuffer->width;
        int cy = curIdx / AreaBuffer->width;

        for (int d = 0; d < 4; d++) {
            //to shift up and down, side to side by knowing row and col of current
            int nx = cx + dirs[d][0];
            int ny = cy + dirs[d][1];

            //out of bounds check
            if (nx < 0 || ny < 0 || nx >= AreaBuffer->width || ny >= AreaBuffer->height){continue;}

            //index of the pixel in the direction
            int nIdx = ny * AreaBuffer->width + nx;

            WalkMapPoint* wp = &AreaBuffer->Buffer[nIdx];
            if (!wp->walkability){
                visited[nIdx]=true;
                continue;//if not walkable, ignore
            }

            float tentativeG = gScore[curIdx] + wp->cost;

            if (tentativeG < gScore[nIdx]) {
                cameFrom[nIdx] = curIdx;
                gScore[nIdx] = tentativeG;
                fScore[nIdx] = tentativeG + heuristic(*wp, goalP);
                push_Node(&open, *wp, fScore[nIdx]);
            }

        }
    }

    bool reached = (visited[goalIdx] == true);

    if (!reached) {
        AStarResult* r = malloc(sizeof(AStarResult));
        r->cost = INT_MAX;
        r->count = 0;
        return r;
    }

    //camefrom works from the last, the goal essentially
    int count = 0;
    int cur = goalIdx;

    while (cur != -1) {
        count++;
        cur = cameFrom[cur];
    }
    AStarResult* r = malloc(sizeof(AStarResult) + count * sizeof(WalkMapPoint));
    r->cost = gScore[goalIdx];
    r->count = count;

    // printf("%d\n",r->cost);
    cur = goalIdx;
    int writeIndex = count - 1;

    while (cur != -1) {//start still have -1 since it never came from anything
        r->route[writeIndex--] = AreaBuffer->Buffer[cur];
        cur = cameFrom[cur];
    }
    
    return r;
}


static SubgridPortalRecord* findAbstractPortal(
    AbstractMap* map,
    WalkMapPoint p
) {

    int subx = p.x / 32;
    int suby = p.y / 32;

    if (subx < 0 || subx >= 16 ||
        suby < 0 || suby >= 16
    ) {return NULL;}

    SubgridRecord* rec = &map->subgrids[suby][subx];

    for (int i = 0; i < rec->portalCount; i++) {
        SubgridPortalRecord* portal =&rec->portals[i];

        if (portal->localPortal.x  == p.x &&
            portal->localPortal.y  == p.y &&
            portal->localPortal.tx == p.tx &&
            portal->localPortal.ty == p.ty) {

            return portal;
        }
    }

    return NULL;
}

static int findAbstractSearchPoint(
    WalkMapPoint* points,
    int count,
    WalkMapPoint p
) {
    for (int i = 0; i < count; i++) {
        if (points[i].x  == p.x &&
            points[i].y  == p.y &&
            points[i].tx == p.tx &&
            points[i].ty == p.ty
        ) {return i;}
    }
    return -1;
}

AStarResult* AbstractAStar(
    WalkMapPoint startP,WalkMapPoint goalP
) {
    //rather than using a buffer, traverse abstractmaps
    PriorityQueue open={ .head=NULL, .tail=NULL, .size=0 };
    bool reached=false;

    // dynamic search state
    WalkMapPoint* points = NULL;
    float* gScores = NULL;
    int* cameFrom = NULL;
    bool* closed = NULL;

    int count = 0;
    int capacity = 16;

    points = malloc(sizeof(WalkMapPoint) * capacity);
    gScores = malloc(sizeof(float) * capacity);
    cameFrom = malloc(sizeof(int) * capacity);
    closed = calloc(capacity,sizeof(bool));

    points[0] = startP;
    gScores[0] = 0.0f;
    cameFrom[0] = -1;
    closed[0] = false;

    count = 1;

    push_Node(&open,startP,heuristic(startP, goalP));

    int currentIndex;
    while (open.size>0) {

        //get low fscore node
        Node* node = pop_Node(&open);
        WalkMapPoint currentP = node->val;
        free(node);

        //Find its search-state index
        currentIndex = findAbstractSearchPoint(points,count,currentP);
        if (currentIndex == -1) {continue;}

        //has it been visited / set the visited now
        if (closed[currentIndex]) {continue;}
        closed[currentIndex] = true;

        //goal reached
        if (currentP.x  == goalP.x &&
            currentP.y  == goalP.y &&
            currentP.tx == goalP.tx &&
            currentP.ty == goalP.ty
        ) {reached=true;break;}

        // Find which Tile owns currentP
        pthread_mutex_lock(&GlobalCache->lock);
        Tile* currentTile =cache_get_tile(GlobalCache,currentP.tx,currentP.ty);
        pthread_mutex_unlock(&GlobalCache->lock);
        if (!currentTile) {continue;}

        //Find current portal in that tile
        SubgridPortalRecord* currentPortal= findAbstractPortal(&currentTile->abstractMap,currentP);
        if (!currentPortal) {continue;}

        //go through the connections of the current portal
        for (int a = 0;a < currentPortal->adjCount;a++) {
            PortalAdjacency* adjacency= &currentPortal->adj[a];
            WalkMapPoint neighbour= adjacency->portal;
            int edgeCost= adjacency->cost;

            int neighbourIndex= findAbstractSearchPoint(points,count,neighbour);

            //this neighbour isnt accounted for so add to points etc
            if (neighbourIndex == -1) {
                // Grow search arrays
                if (count == capacity) {
                    capacity *= 2;

                    points = realloc(points,sizeof(WalkMapPoint) *capacity);
                    gScores = realloc(gScores,sizeof(float) *capacity);
                    cameFrom = realloc(cameFrom,sizeof(int) *capacity);

                    closed = realloc(closed,sizeof(bool) *capacity);
                    //set the values after the old closed to false
                    memset(
                        &closed[count],
                        0,
                        sizeof(bool) *(capacity - count)
                    );
                }

                neighbourIndex = count++;

                points[neighbourIndex] =neighbour;
                gScores[neighbourIndex] =FLT_MAX;
                cameFrom[neighbourIndex] =-1;
                closed[neighbourIndex] =false;
            }

            //has this node been visited
            if (closed[neighbourIndex]) {continue;}

            //get the route cost for this node
            float tentativeG =gScores[currentIndex] + edgeCost;

            if (tentativeG < gScores[neighbourIndex]) {
                cameFrom[neighbourIndex] =currentIndex;
                gScores[neighbourIndex] =tentativeG;

                float fScore =tentativeG +heuristic(neighbour,goalP);

                push_Node(&open,neighbour,fScore);
            }
        }

        
    }

    //there is a path
    if(reached){
        //rebuild route
        int routeCount = 0;
        int current = currentIndex;

        while (current != -1) {
            routeCount++;
            current = cameFrom[current];
        }

        AStarResult* result =malloc(sizeof(AStarResult)+routeCount * sizeof(WalkMapPoint));
        result->cost = gScores[currentIndex];
        result->count = routeCount;

        current = currentIndex;

        int writeIndex = routeCount - 1;

        while (current != -1) {
            result->route[writeIndex--] =points[current];
            current =cameFrom[current];
        }

        free(points);
        free(gScores);
        free(cameFrom);
        free(closed);

        return result;
    }

    //no path
    free(points);
    free(gScores);
    free(cameFrom);
    free(closed);

    AStarResult* result = malloc(sizeof(AStarResult));

    result->cost = INT_MAX;
    result->count = 0;

    return result;
}