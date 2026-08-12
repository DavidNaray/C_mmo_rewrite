import * as THREE from "https://cdn.jsdelivr.net/npm/three@0.176.0/build/three.module.js";

import {scene,requestRenderIfNotRequested} from "../siteJS.js"

import {globalmanager} from "./GlobalInstanceMngr.js"

export class TileInstancePool { 
    constructor(tile) {
        this.tile = tile; // 👈 Full reference to the Tile instance
        this.dummyMatrix = new THREE.Matrix4(); // Globally or per class
        this.instanceGroups = new Map(); // objectType → instanceObject (for that objectType) 
        
        this.ServerId_To_ObjTypeAndInstId_Mapping=new Map();//integer → [objectType,instanceId]

    }

    getTileCoord() {return [this.tile.x,this.tile.y];}

    GeneralAddInstance(objectType, transform,meta={}){

        let mesh=this.instanceGroups.get(objectType);
        
        if(!mesh){
            mesh=this.createInstanceObjectOfCount(objectType,3);
            this.instanceGroups.set(objectType,mesh)
            scene.add(mesh);
        }
        else{
            const trueMax=mesh.instanceMatrix.count
            if(mesh.count >= trueMax){
                const newMesh=this.createInstanceObjectOfCount(objectType,trueMax+16,mesh);
                newMesh.metadata=mesh.metadata;//copy over from smaller instancedmesh
                
                scene.remove(mesh)
                mesh = newMesh;
                scene.add(mesh);
                
                this.instanceGroups.set(objectType,mesh);
        }   }

        const index = mesh.count++;

        this.ServerId_To_ObjTypeAndInstId_Mapping.set(meta.ServerId,[objectType,index]);

        mesh.setMatrixAt(index, transform);
        meta.parentTile=[this.tile.x,this.tile.y]
        mesh.metadata.set(index,meta);
        mesh.instanceMatrix.needsUpdate = true;

        // if (meta.underConstruction) {
        //     mesh.geometry.getAttribute("instanceOpacity").setX(index, 0.5);
        //     mesh.geometry.getAttribute("instanceOpacity").needsUpdate = true;
        // }
        if (index >= mesh.count) {mesh.count = index + 1;}
        mesh.computeBoundingSphere();
        requestRenderIfNotRequested();
    }

    createInstanceObjectOfCount(objectType, count, oldMesh = null) {
        const objectTypeMesh = globalmanager.getAsset(objectType);

        //CLONE GEOMETRY SO WE DON'T MUTATE THE BASE ASSET
        const geometry = objectTypeMesh.geometry.clone();
        const baseMat  = objectTypeMesh.materials;

        const opacityAttr = new THREE.InstancedBufferAttribute(new Float32Array(count), 1);
        for (let i = 0; i < count; i++) opacityAttr.setX(i, 1);
        geometry.setAttribute("instanceOpacity", opacityAttr);
        opacityAttr.needsUpdate = true;

        let material;
        if (Array.isArray(baseMat)) {
            material = baseMat.map(m => {
                const mat = m.clone();

                mat.onBeforeCompile = (shader) => {
                    shader.vertexShader = `
                        attribute float instanceOpacity;
                        varying float vOpacity;
                    ` + shader.vertexShader;

                    shader.vertexShader = shader.vertexShader.replace(
                        '#include <begin_vertex>',
                        `#include <begin_vertex>
                        vOpacity = instanceOpacity;`
                    );

                    shader.fragmentShader = `
                        varying float vOpacity;
                    ` + shader.fragmentShader;

                    shader.fragmentShader = shader.fragmentShader.replace(
                        '#include <dithering_fragment>',
                        `
                        gl_FragColor.a *= vOpacity;
                        #include <dithering_fragment>
                        `
                    );
                };

                mat.transparent = true;
                mat.needsUpdate = true;
                return mat;
            });

        } else if (baseMat.clone) {
            const mat = baseMat.clone();
            mat.transparent = true;
            mat.opacity = 1.0;
            mat.needsUpdate = true;
            material = mat;

        } else {
            console.warn("Material has no clone(), using as-is:", baseMat);
            material = baseMat;
        }

        const mesh = new THREE.InstancedMesh(geometry, material, count);
        mesh.metadata = new Map();

        const freeIndices = new Set();
        for (let j = 0; j < count; j++) freeIndices.add(j);
        mesh.freeIndices = freeIndices;

        if (oldMesh) {
            const oldOpacityAttr = oldMesh.geometry.getAttribute("instanceOpacity");

            for (let i = 0; i < oldMesh.count; i++) {
                freeIndices.delete(i);
                oldMesh.getMatrixAt(i, this.dummyMatrix);
                mesh.setMatrixAt(i, this.dummyMatrix);

                const meta = oldMesh.metadata.get(i);
                if (meta) mesh.metadata.set(i, meta);

                const oldOpacity = oldOpacityAttr.getX(i);
                opacityAttr.setX(i, oldOpacity);
            }

            mesh.count = oldMesh.count;
            mesh.geometry.getAttribute("instanceOpacity").needsUpdate = true;
        } else {
            mesh.count = 0;
        }

        return mesh;
    }

    removeInstance(serverId) {
        console.log("removing instance with serverId:",serverId)
        // let mesh=this.instanceGroups.get(objectType);
        const relevantInfo=this.ServerId_To_ObjTypeAndInstId_Mapping.get(serverId);
        const objectType=relevantInfo[0]
        const index=relevantInfo[1]
        let mesh=this.instanceGroups.get(objectType)

        if (!mesh) return false;
    
        if (index >= mesh.count) return false; // Invalid index

        const lastIndex = mesh.count - 1;

        //so when count is decremented, it chops off the top instance, so we swap the index to be removed with the lastindex
        if (index !== lastIndex) {
            // Move last matrix into the removed slot
            mesh.getMatrixAt(lastIndex, this.dummyMatrix);
            mesh.setMatrixAt(index, this.dummyMatrix);
    
            // Update metadata
            const lastMeta = mesh.metadata.get(lastIndex);
            mesh.metadata.set(index, lastMeta);
            mesh.metadata.delete(lastIndex);
            console.log("lastMeta.serverId",lastMeta.ServerId)
            this.ServerId_To_ObjTypeAndInstId_Mapping.set(lastMeta.ServerId,[objectType,index])
            
        } else {
            // If you're removing the last one directly since index ==last
            mesh.metadata.delete(index);
            
        }
        //you do not add the last index to free-indices because count is decremented
        this.ServerId_To_ObjTypeAndInstId_Mapping.delete(serverId);
        mesh.count--;
        mesh.instanceMatrix.needsUpdate = true;

        
        // Compact every 10 removals (adjustable)
        if (mesh.freeIndices.size>=10) {
            console.log("trigger compact")
            this.compactInstanceObject(objectType, mesh);
        }
        requestRenderIfNotRequested();
        return true;
    }

    getUnitData(serverId){
        const [objectType,instanceId]=this.ServerId_To_ObjTypeAndInstId_Mapping.get(serverId);

        const mesh = this.instanceGroups.get(objectType);
        if (!mesh) return undefined;

        return [objectType,mesh.metadata.get(instanceId)];
    }

    compactInstanceObject(objectType, oldMesh) {
        const usedIndices = new Set();
        for (let i = 0; i < oldMesh.count; i++) {
            if (!oldMesh.freeIndices.has(i)) {
                usedIndices.add(i);
            }
        }
    
        // Nothing to compact if it's full or only a couple used
        if (usedIndices.size === oldMesh.instanceMatrix.count) return;
    
        //creating newMesh, not updating hence no oldMesh 3rd param into this, have to define freeIndices here, empty cus full
        const newMesh = this.createInstanceObjectOfCount(objectType, usedIndices.size,oldMesh);
    
        scene.remove(oldMesh);
        scene.add(newMesh);
    
        // targetMap.set(objectType, newMesh);
        this.instanceGroups.set(objectType,newMesh);
    }

    moveUnit(serverId,NewPositiontransform){
        const relevantInfo=this.ServerId_To_ObjTypeAndInstId_Mapping.get(serverId);
        const theInstanceObjectType=relevantInfo[0]
        const theUnitsInstanceI=relevantInfo[1]
        let mesh=this.instanceGroups.get(theInstanceObjectType)
        mesh.setMatrixAt(theUnitsInstanceI, NewPositiontransform);
        mesh.instanceMatrix.needsUpdate = true;
        mesh.computeBoundingSphere();
        requestRenderIfNotRequested();
    }

    setInstanceOpacity(serverId, opacity) {
        const relevantInfo = this.ServerId_To_ObjTypeAndInstId_Mapping.get(serverId);
        if (!relevantInfo) return false;

        const [objectType, instanceId] = relevantInfo;

        const mesh = this.instanceGroups.get(objectType);

        if (!mesh) return false;
        if (instanceId >= mesh.count) return false;

        // Clamp to 0..1
        opacity = Math.max(0, Math.min(1, opacity));

        const opacityAttr = mesh.geometry.getAttribute("instanceOpacity");

        if (!opacityAttr) return false;

        opacityAttr.setX(instanceId, opacity);
        opacityAttr.needsUpdate = true;

        requestRenderIfNotRequested();

        return true;
    }

}