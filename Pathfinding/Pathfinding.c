#include "Pathfinding.h"

#include "../MongoDBReadWriteCache/Cache.h"
#include "../MongoDBReadWriteCache/Schema/UserBreakdown.h"
#include "../MongoDBReadWriteCache/ReadUser.h"
#include "../MongoDBReadWriteCache/Cache.h"
#include <mongoc/mongoc.h>

#include <limits.h> // Required for INT_MAX and INT_MIN

#include <cJSON.h>
#include <windows.h>



void push_Node(PriorityQueue* q, int index, float prio) {
    Node* n = malloc(sizeof(Node));
    n->idx = index;
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

    return node;
}


ExtractRegion* extractRegion(WalkMapPoint srcbuffer[512][512], int StartX, int StartY, int segW, int segH){
    //function dedicated to cutting 32x32 cuts from a tiles buffer

    size_t count = segW * segH;

    ExtractRegion* cut = malloc(sizeof(ExtractRegion) + count * sizeof(WalkMapPoint));
    cut->width=segW;
    cut->height=segH;

    //copies over the segments of rows from the srcbuffer (faster than x,y iteration)
    for (int y = 0; y < segH; y++) {
        memcpy(&cut->Buffer[y * segW],&srcbuffer[StartY + y][StartX],segW * sizeof(WalkMapPoint));
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

ExtractRegion* combineSegments(
    const ExtractRegion* bufA, int posAx, int posAy,//pointer and global coords
    const ExtractRegion* bufB, int posBx, int posBy
){
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
    // out->originX = minX;
    // out->originY = minY;

    // make sure to set things as unwalkable
    for (int i = 0; i < width * height; i++) {
        out->Buffer[i].cost = INT_MAX;      // or whatever "infinite" means
        out->Buffer[i].walkability = false;
        out->Buffer[i].object = NULL;
    }


    // // 3. Copy helper (row-by-row memcpy)
    // void copyRegion(const ExtractRegion* src, int destX, int destY){
    //     for(int y = 0; y < src->height; y++){
    //         memcpy(
    //             &out->Buffer[(destY + y) * width + destX],
    //             &src->Buffer[y * src->width],
    //             src->width * sizeof(WalkMapPoint)
    //         );
    //     }
    // }

    // 4. Place A and B relative to minX/minY
    int destAx = posAx - minX;
    int destAy = posAy - minY;

    int destBx = posBx - minX;
    int destBy = posBy - minY;

    copyRegion(out,width,bufA, destAx, destAy);
    copyRegion(out,width,bufB, destBx, destBy);

    return out;
}

void AStarPathCost(
    ExtractRegion* AreaBuffer,
    WalkMapPoint startP,WalkMapPoint goalP,
    bool flag) {

}
