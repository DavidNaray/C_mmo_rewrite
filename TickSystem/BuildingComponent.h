#ifndef Bcomp_H
#define Bcomp_H   // these form a guard

#include "../MongoDBReadWriteCache/Schema/TileSchema.h"

void BuildingLoop(Bucket* b);


void AddMovementOrder(int cx,int cy,int px,int py);
void AddConstructionOrder(int index,int cx,int cy,int px,int py);
void AddUserWithTrainingOrders(char* username);

#endif