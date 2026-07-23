#ifndef TG_H
#define TG_H   // these form a guard

#include <stdint.h>
#include "FastNoiseLite.h"


typedef struct {
    float ocean;
    float plains;
    float mountains;

    bool KernelEffect;
} BiomeInfluence;


typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} pixel_t;

typedef struct {
    int xResolution;
    int yResolution;

    //OpenSimplex2,OpenSimplex2S,Cellular,Perlin,ValueCubic,Value
    char noiseType[16];

    float frequency;
    int seed;

    char saveRoot[64];
} TerrainSetup;

extern TerrainSetup TSetup;
extern fnl_state noise;//biome distribution noise
extern fnl_state warp;

extern fnl_state MountainNoise;
extern fnl_state PlainsNoise;

typedef struct Node{
    int idx;
    float priority; // 0 = equal, >0 = brighter
    struct Node *next;
} Node;

typedef struct {
    Node *head;
    Node *tail;
} NodeQueue;




TerrainSetup SetupTerrainFields(int xR,int yR,char nT[],float f,int s, char root[]);
void ApplyTerrainFields();

void GenerateTerrainTile(int x,int y, char* username);

#endif

