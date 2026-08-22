#ifndef PathF_H
#define PathF_H   // these form a guard

#include "../MongoDBReadWriteCache/Schema/TileSchema.h"

typedef struct {
    int height;
    int width;
    
    int originX;//global coord
    int originY;

    WalkMapPoint Buffer[];
} ExtractRegion;

typedef struct Node{
    WalkMapPoint val;
    float priority; // 0 = equal, >0 = brighter
    struct Node *next;
} Node;

typedef struct {
    Node *head;
    Node *tail;
    int size;
} PriorityQueue;


typedef struct {
    int cost;//total cost
    int count;
    WalkMapPoint route[];
} AStarResult;

ExtractRegion* extractRegion(Tile* t, int StartX, int StartY, int segW, int segH);


ExtractRegion* combineSegments(const ExtractRegion* bufA,const ExtractRegion* bufB);

AStarResult* AStarPathCost(
    ExtractRegion* AreaBuffer,
    WalkMapPoint startP,WalkMapPoint goalP
);

#endif