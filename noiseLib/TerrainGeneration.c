
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define FNL_IMPL
#include "TerrainGeneration.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "../MongoDBReadWriteCache/Schema/TileSchema.h"
#include "../MongoDBReadWriteCache/Cache.h"

#include <limits.h> // Required for INT_MAX and INT_MIN

TerrainSetup TSetup;   
fnl_state noise;    
fnl_state warp;

fnl_state MountainNoise;  
fnl_state PlainsNoise;  


TerrainSetup SetupTerrainFields(int xR, int yR, char nT[], float f, int s, char root[]){
    TerrainSetup ts;
    ts.xResolution = xR;
    ts.yResolution = yR;

    strncpy(ts.noiseType, nT, sizeof(ts.noiseType) - 1);
    ts.noiseType[sizeof(ts.noiseType) - 1] = '\0';
    
    ts.frequency = f;
    ts.seed = s;

    strncpy(ts.saveRoot, root, sizeof(ts.saveRoot) - 1);
    ts.saveRoot[sizeof(ts.saveRoot) - 1] = '\0';
    
    return ts;
}

void ApplyTerrainFields(){

    noise = fnlCreateState();
    noise.seed = TSetup.seed;
    noise.frequency = TSetup.frequency;

    // Cellular settings
    noise.cellular_distance_func = FNL_CELLULAR_DISTANCE_HYBRID;
    noise.cellular_return_type = FNL_CELLULAR_RETURN_TYPE_CELLVALUE;
    noise.cellular_jitter_mod = 1.0f;

    noise.fractal_type = FNL_FRACTAL_NONE;
    noise.octaves = 3;
    noise.lacunarity = 2.0f;
    noise.gain = 2.0f;

    if (strcmp(TSetup.noiseType, "Perlin") == 0) {noise.noise_type = FNL_NOISE_PERLIN;}
    else if (strcmp(TSetup.noiseType, "Value") == 0) {noise.noise_type = FNL_NOISE_VALUE;} 
    else if (strcmp(TSetup.noiseType, "ValueCubic") == 0) {noise.noise_type = FNL_NOISE_VALUE_CUBIC;}
    else if (strcmp(TSetup.noiseType, "OpenSimplex2") == 0) {noise.noise_type = FNL_NOISE_OPENSIMPLEX2;} 
    else if (strcmp(TSetup.noiseType, "OpenSimplex2S") == 0) {noise.noise_type = FNL_NOISE_OPENSIMPLEX2S;}
    else {noise.noise_type = FNL_NOISE_CELLULAR;}

    // Domain warp
    warp = fnlCreateState();
    warp.noise_type = FNL_NOISE_OPENSIMPLEX2;
    warp.seed = 1337;
    warp.frequency = 0.010f;
    warp.domain_warp_type = FNL_DOMAIN_WARP_OPENSIMPLEX2;
    warp.domain_warp_amp = 100.0f;

    // Domain warp fractal
    warp.fractal_type = FNL_FRACTAL_DOMAIN_WARP_INDEPENDENT;
    warp.octaves = 3;
    warp.lacunarity = 2.0f;
    warp.gain = 0.5f;


    PlainsNoise = fnlCreateState();
    PlainsNoise.seed = TSetup.seed;
    PlainsNoise.noise_type = FNL_NOISE_OPENSIMPLEX2;
    PlainsNoise.fractal_type = FNL_FRACTAL_FBM;
    PlainsNoise.octaves = 3;
    PlainsNoise.gain = 0.5f;
    PlainsNoise.lacunarity = 1.8f;
    PlainsNoise.frequency = 0.002f;

    MountainNoise = fnlCreateState();
    MountainNoise.seed = TSetup.seed;
    MountainNoise.noise_type = FNL_NOISE_CELLULAR;
    MountainNoise.fractal_type = FNL_FRACTAL_RIDGED;
    MountainNoise.octaves = 3;
    MountainNoise.gain = 0.2f;
    MountainNoise.lacunarity = 2.1f;
    MountainNoise.frequency = 0.005f;
}


void saveFile(pixel_t* buffer,char* extension,int x,int y){
    char path[256];
    _snprintf(path, sizeof(path), "%s/%s/%d%d.png", TSetup.saveRoot,extension, x, y);
    path[sizeof(path) - 1] = '\0';

    stbi_write_png(
        path,
        TSetup.xResolution,
        TSetup.yResolution,
        3,                  // RGB
        buffer,
        TSetup.xResolution * 3
    );
}

pixel_t lerp(pixel_t a, pixel_t b, float t){
    pixel_t out;
    out.red   = a.red   + (b.red   - a.red)   * t;
    out.green = a.green + (b.green - a.green) * t;
    out.blue  = a.blue  + (b.blue  - a.blue)  * t;
    return out;
}


pixel_t getOceanColour(int y,int x,BiomeInfluence target,float* oheight){
    
    pixel_t ocean  ={0,  0, 255};
    pixel_t sand = { 210, 190, 120 }; // warm sandy yellow

    *oheight = 0.1f;

    return ocean;//lerp(sand,ocean,  target.ocean);;//(pixel_t){0,  0, 255};
}

pixel_t getPlainsColour(int y,int x,BiomeInfluence target,float* pheight){
    float xw = (float)x;
    float yw = (float)y;

    fnlDomainWarp2D(&warp, &xw, &yw);
    float n = fnlGetNoise2D(&PlainsNoise, xw, yw);
    n = (n + 1.0f) * 0.5f;   // normalize to [0,1] 

    pixel_t low  = {  30, 120,  30 }; // deep grass
    pixel_t high = { 180, 200,  80 }; // light grassy hills

    pixel_t sand = { 210, 190, 120 }; // warm sandy yellow

    pixel_t plainsCol = lerp(low, high, n);

    float pow=powf(target.ocean, 0.3f);
    *pheight = n*0.3;

    return lerp(plainsCol,sand,  pow);
}

pixel_t getMountainColour(int y,int x,BiomeInfluence target,float* mheight){
    float xw = (float)x;
    float yw = (float)y;

    fnlDomainWarp2D(&warp, &xw, &yw);
    float n = fnlGetNoise2D(&MountainNoise, xw, yw);
    n = (n + 1.0f) * 0.5f;   // normalize to [0,1] 

    float bluff=0.0f;//sacraficial to use plainscolour here

    pixel_t foothill = getPlainsColour(y,x,target,&bluff);//{  40, 110,  40 }; // green
    pixel_t rock     = { 120, 100,  80 }; // brownish
    pixel_t highrock = { 160, 160, 160 }; // grey
    // pixel_t snow     = { 255, 255, 255 }; // white
    pixel_t darkRock     = { 94, 89, 86 }; // brownish

    pixel_t mountain;
    if (n < 0.33f) {
        mountain= lerp(foothill, rock, n / 0.33f);
        // *mheight=n / 0.33f;
        // *mheight=n*1.3;
    }
    else if (n < 0.66f) {
        mountain= lerp(rock, highrock, (n - 0.33f) / 0.33f);
        mountain= lerp(mountain, rock, target.plains);
        // *mheight=n;
    }
    else {
        mountain= lerp(highrock, darkRock, (n - 0.66f) / 0.34f);
        mountain= lerp(mountain, rock, target.plains);
        // *mheight=n;
    }
    *mheight=n*1.5;//n+0.2f *powf(1.0f - n, 4.0f);

    return mountain;
}

void ApplyBiome(bool* BoundaryMask,
                BiomeInfluence* influences, 
                pixel_t* pixels,
                pixel_t* Heightpixels,
                pixel_t* Walkpixels,
                Tile* tile,
                int offsetX,
                int offsetY){
    int W = TSetup.xResolution;
    int H = TSetup.yResolution;

    BiomeInfluence* out = malloc(W * H * sizeof(BiomeInfluence));
    memset(out, 0, W * H * sizeof(BiomeInfluence));

    float kernel1D[24];
    float sigma = 10.0f; // controls softness
    float sum = 0.0f;
    int x;
    float v;

    for (int i = 0; i < 24; i++) {
        x = i - 24; // center at 12
        v = expf(-(x*x) / (2.0f * sigma * sigma));
        kernel1D[i] = v;
        sum += v;
    }
    for (int i = 0; i < 24; i++) {kernel1D[i] /= sum;}// normalize

    // 24x24 kernel
    float kernel[24][24];
    for (int y = 0; y < 24; y++) {for (int x = 0; x < 24; x++) {
        kernel[y][x] = kernel1D[y] * kernel1D[x];
    }   }


    for (int y = 0; y < H; y++) {for (int x = 0; x < W; x++) {
        int idx = y * W + x;
        
        if (BoundaryMask[idx]) {
            //ocean, plains, mountains
            float bo = influences[idx].ocean;
            float bp = influences[idx].plains;
            float bm = influences[idx].mountains;

            for (int ky = -12; ky <= 11; ky++) {for (int kx = -12; kx <= 11; kx++) {
                int nx = x + kx;
                int ny = y + ky;

                if (nx < 0 || nx >= W || ny < 0 || ny >= H){continue;}

                int iidx = ny * W + nx;
                float w = kernel[ky + 12][kx + 12];

                out[iidx].ocean     += bo * w *60;
                out[iidx].plains    += bp * w *60;
                out[iidx].mountains += bm * w *60;

                influences[iidx].KernelEffect = true;
        }   }   }

        //could still be influenced by a kernel at some point, check
        if(influences[idx].KernelEffect==false){
            out[idx].ocean     = influences[idx].ocean;
            out[idx].plains    = influences[idx].plains;
            out[idx].mountains = influences[idx].mountains;
            continue;
        }

    }   }

    // Write final RGB
    for (int y = 0; y < H; y++) {for (int x = 0; x < W; x++) {
        int i=W*y+x;

        int realX=x + (offsetX*(TSetup.xResolution));
        int realY=y + (offsetY*(TSetup.yResolution));

        BiomeInfluence influence = out[i];

        float sum = influence.ocean + influence.plains + influence.mountains;

        if (sum > 0.0f){
            influence.ocean     /= sum;
            influence.plains    /= sum;
            influence.mountains /= sum;
        }
        float oceanHeight;
        float plainsHeight;
        float mountainHeight;

        pixel_t oceanColor     = getOceanColour(realY,realX,influence,&oceanHeight);//{  0,  0, 255 };
        pixel_t plainsColor    = getPlainsColour(realY,realX,influence,&plainsHeight);//{  0, 255,  0 };
        pixel_t mountainsColor = getMountainColour(realY,realX,influence,&mountainHeight);//{ 150, 150, 150 };

        pixels[i].red =
            oceanColor.red     * influence.ocean +
            plainsColor.red    * influence.plains +
            mountainsColor.red * influence.mountains;

        pixels[i].green =
            oceanColor.green     * influence.ocean +
            plainsColor.green    * influence.plains +
            mountainsColor.green * influence.mountains;

        pixels[i].blue =
            oceanColor.blue     * influence.ocean +
            plainsColor.blue    * influence.plains +
            mountainsColor.blue * influence.mountains;


        //heightmap drawing
        float bline=0.1;
        float mntTot=(bline+mountainHeight)*influence.mountains;
        float height =  
            oceanHeight*influence.ocean + 
            (bline+plainsHeight)*influence.plains +
            mntTot;//oceanHeight + plainsHeight +mountainHeight
        if(height>1){height=1;}

        uint8_t h = (uint8_t)(height * 255.0f);
        Heightpixels[i] = (pixel_t){h,h,h};

        //walkmap drawing
        float mT=0.5f;
        float oceanThreshold=0.2;
        if(mntTot>mT){/*mountain/too high*/
            Walkpixels[i] = (pixel_t) {0,0,0};
            tile->Buffer[y][x].cost=0;
            tile->Buffer[y][x].walkability=false;
        }
        else if(influence.ocean>oceanThreshold){/*blue ocean*/
            Walkpixels[i] = (pixel_t) {0,0,255};
            tile->Buffer[y][x].cost=INT_MAX;//0;
            tile->Buffer[y][x].walkability=false;
        }
        else{/*walkable-white*/
            Walkpixels[i] = (pixel_t) {255,255,255};
            tile->Buffer[y][x].walkability=true;
            tile->Buffer[y][x].cost=1;
        }

    }   }
    
    free(out);
}

void GenerateTerrainTile(int x,int y,char* username){

    pixel_t* pixels = malloc(TSetup.xResolution * TSetup.yResolution * sizeof(pixel_t));
    bool* BoundaryMask = malloc(TSetup.xResolution * TSetup.yResolution * sizeof(bool));
    BiomeInfluence* influences = malloc(TSetup.xResolution * TSetup.yResolution * sizeof(BiomeInfluence));

    pixel_t* Heightpixels = malloc(TSetup.xResolution * TSetup.yResolution * sizeof(pixel_t));
    pixel_t* Walkpixels = malloc(TSetup.xResolution * TSetup.yResolution * sizeof(pixel_t));

    memset(influences, 0, TSetup.xResolution * TSetup.yResolution * sizeof(BiomeInfluence));
    int BiomeCount = 3;// number of shades/biomes represented you want
    int index = 0;
    float nPrev;
    int cooldown=0;

    for (int yy = 0; yy < TSetup.yResolution; yy++){
        for (int xx = 0; xx < TSetup.xResolution; xx++) {
            float xw = (float)xx + (x*TSetup.xResolution);
            float yw = (float)yy + (y*TSetup.yResolution);
            fnlDomainWarp2D(&warp, &xw, &yw);

            float n = fnlGetNoise2D(&noise, (float)xw, (float)yw);
            n = (n + 1.0f) * 0.5f;   // normalize to [0,1]         
            // n = floorf(n * BiomeCount);

            if(n!=nPrev){
                // if(cooldown>0){cooldown--;}
                for(int rows=-5;rows<11;rows++){for(int cols=-5;cols<11;cols++){
                    int targetIdx=index + (rows*TSetup.xResolution) + cols;
                    if(targetIdx<0 || targetIdx>=TSetup.xResolution*TSetup.yResolution){continue;} 
                    BoundaryMask[targetIdx]=true;
                // BoundaryMask[index]=true;
                }   }
                // cooldown=15;
                nPrev=n;
            }

            if (n < 0.02f) {influences[index].ocean = 1.0f;}//2%
            else if (n < 0.32f) {influences[index].mountains = 1.0f;}//30%+2%
            else {influences[index].plains = 1.0f;}//the rest %

            index++;
        }
    }

    //make a new tile object for cache
    Tile* tile = malloc(sizeof(Tile));
    tile->x=x;
    tile->y=y;
    // printf("init tile");
    tile->buildings.count = 0;
    tile->buildings.capacity = 4;   // small initial size
    tile->buildings.list = malloc(sizeof(Building*) * tile->buildings.capacity);

    memset(tile->usernames, '\0', sizeof(tile->usernames));
    snprintf(tile->usernames[4],256,username);/*middle of a 3x3*/

    //find any other neighbours by looping around
    // pthread_mutex_lock(&GlobalCache->lock);
    for(int rows=-1;rows<2;rows++){for(int cols=-1;cols<2;cols++){
        if(rows==0 && cols==0){continue;}

        Tile* targettile=cache_get_tile(GlobalCache,x+cols,y+rows);
        if(targettile==NULL){continue;}
        
        //index 4 is the owner of the tile, other indices are the owners of other tiles
        int index=(rows+1)*3+(cols+1);

        //mirror update
        snprintf(tile->usernames[index],256,targettile->usernames[4]);
        snprintf(targettile->usernames[8-index],256,username);
    }   }
    // pthread_mutex_unlock(&GlobalCache->lock);

    

    snprintf(tile->textures.texturemapUrl,
        sizeof(tile->textures.texturemapUrl),
        "../Tiles/TextureMaps/%d%d.png",
        x, y
    );
    snprintf(tile->textures.heightmapUrl,
        sizeof(tile->textures.heightmapUrl),
        "../Tiles/HeightMaps/%d%d.png",
        x, y
    );
    snprintf(tile->textures.WalkMapURL,
        sizeof(tile->textures.WalkMapURL),
        "../Tiles/WalkMaps/%d%d.png",
        x, y
    );

    // printf("reached applybiome");
    ApplyBiome(BoundaryMask,influences,pixels,Heightpixels,Walkpixels,tile,x,y);

    AbstractMapCreate(tile,true);
    
    //add to the cache the tile
    // pthread_mutex_lock(&GlobalCache->lock);
    // printf("adding tile to cache\n");
    cache_insert_tile(GlobalCache,tile);
    // pthread_mutex_unlock(&GlobalCache->lock);
    // printf("after tile insert\n");

    saveFile(pixels,"TextureMaps",x,y);
    saveFile(Heightpixels,"Heightmaps",x,y);
    saveFile(Walkpixels,"WalkMaps",x,y);
    // printf("should be saving fileeee\n");

    free(pixels);
    free(Heightpixels);
    free(Walkpixels);

    free(influences);
}