#include "Spiral.h"

SpiralState SpiralS;

int* getXYSpiral() {
    static int out[2];

    // output current coordinate
    out[0] = SpiralS.x;
    out[1] = SpiralS.y;

    // advance
    SpiralS.x += SpiralS.dx;
    SpiralS.y += SpiralS.dy;

    SpiralS.segmentPassed++;

    // time to turn?
    if (SpiralS.segmentPassed == SpiralS.segmentLength) {
        SpiralS.segmentPassed = 0;

        // rotate direction clockwise
        int tmp = SpiralS.dx;
        SpiralS.dx = -SpiralS.dy;
        SpiralS.dy = tmp;

        SpiralS.segmentCount++;

        // every two turns, increase segment length
        if (SpiralS.segmentCount % 2 == 0) {
            SpiralS.segmentLength++;
        }
    }

    return out;
}


void initSpiral(){
    SpiralS.x = 0;
    SpiralS.y = 0;

    SpiralS.dx = 1;
    SpiralS.dy = 0;

    SpiralS.segmentLength = 1;
    SpiralS.segmentPassed = 0;
    SpiralS.segmentCount = 0;
}