#ifndef Spir_H
#define Spir_H   // these form a guard

typedef struct {
    int x;
    int y;
    
    int dx;
    int dy;

    int segmentLength;
    int segmentPassed;
    int segmentCount;
} SpiralState;

extern SpiralState SpiralS;

int* getXYSpiral();

void initSpiral();
#endif