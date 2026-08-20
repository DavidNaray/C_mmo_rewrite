#ifndef PathF_H
#define PathF_H   // these form a guard

#include "../MongoDBReadWriteCache/Schema/TileSchema.h"

typedef struct {
    int height;
    int width;
    // int originX;
    // int originY;
    WalkMapPoint Buffer[];
} ExtractRegion;

typedef struct Node{
    int idx;
    float priority; // 0 = equal, >0 = brighter
    struct Node *next;
} Node;

typedef struct {
    Node *head;
    Node *tail;
} PriorityQueue;


ExtractRegion* extractRegion(WalkMapPoint srcbuffer[512][512], int StartX, int StartY, int segW, int segH);

ExtractRegion* combineSegments(
    const ExtractRegion* bufA, int posAx, int posAy,
    const ExtractRegion* bufB, int posBx, int posBy
);

void AStarPathCost(
    ExtractRegion* AreaBuffer,
    WalkMapPoint startP,WalkMapPoint goalP,
    bool flag
);

#endif