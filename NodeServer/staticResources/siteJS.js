import * as THREE from "https://cdn.jsdelivr.net/npm/three@0.176.0/build/three.module.js";

import {OrbitControls} from 'https://cdn.jsdelivr.net/npm/three@0.176.0/examples/jsm/controls/OrbitControls.js';

import { setupSocketConnection,emitRegister,
    emitLogin,getUserTileData,resumesession } from "./JS_Externals/SceneInitiation.js"

import {MakeToolTips} from "./JS_Externals/ResourceTips.js"
import {addEventListenersToButtons} from "./JS_Externals/DropDownUI.js"
import {updateGridColumns} from "./JS_Externals/Utils.js"
import {raycaster,pointer,MouseDownHandling,MouseMovingHandling,MouseUpHandling} from "./JS_Externals/RaycasterHandling.js"

import {globalmanager} from "./JS_Externals/GlobalInstanceMngr.js"
import {InputManager} from "./JS_Externals/UserInputState.js"
import {UImanager} from "./JS_Externals/UIManager.js"

export var renderer,camera,username,UserId,controls;
export const scene = new THREE.Scene();
export const InputState={value:"neutral"};


var renderRequested;

export function sceneSetup(){

    scene.background = new THREE.Color('hsl(194, 100%, 71%)');
    
    renderer = new THREE.WebGLRenderer({ antialias: false, alpha: true,powerPreference: "high-performance" });
    renderer.sortObjects = true;
    renderer.shadowMap.enabled = false;
    renderer.setSize( window.innerWidth, window.innerHeight );
    renderer.setPixelRatio(window.devicePixelRatio * 0.75); // Half the normal pixel ratio

    document.getElementById("ThreeBlock").appendChild(renderer.domElement)

    InputManager.SetupListeners()
    UImanager.AddListeners()

    //add eventlisteners to allow object selection
    renderer.domElement.addEventListener("mousedown",MouseDownHandling)
    renderer.domElement.addEventListener("mousemove",MouseMovingHandling)
    renderer.domElement.addEventListener("mouseup",MouseUpHandling)

    camera = new THREE.PerspectiveCamera( 75, renderer.domElement.width / renderer.domElement.height, 0.1, 10000 );//window.innerWidth / window.innerHeight
    camera.position.z = 0
    camera.position.x = 0
    camera.position.y = 2
    camera.lookAt(new THREE.Vector3(0,0,0))
    
    controls = new OrbitControls( camera, renderer.domElement );
    controls.addEventListener( 'change', requestRenderIfNotRequested );
    
    let ambientLight = new THREE.AmbientLight(new THREE.Color('hsl(0, 100%, 100%)'), 3);
    scene.add(ambientLight);

}

function render(){
    renderRequested = false;
    // raycaster.setFromCamera( pointer, camera );
    controls.update();
    InputManager.Action();
    renderer.render(scene, camera);
}

export function requestRenderIfNotRequested() {
  if (!renderRequested) {
    renderRequested = true;
    requestAnimationFrame(render);
  }
}


function onResize() {
    renderer.setSize( window.innerWidth, window.innerHeight );
    camera.aspect = renderer.domElement.width/renderer.domElement.height;
    camera.updateProjectionMatrix();
    InputManager.UpdateBoxArea(window.innerWidth, window.innerHeight);
    UImanager.onResize();
    updateGridColumns();
    requestRenderIfNotRequested();
}

function decodeJWT(token) {
    const payloadBase64 = token.split('.')[1]; // the middle part
    const payload = atob(payloadBase64); // decode base64 to string
    return JSON.parse(payload); // parse the string to object
}

async function refreshAccessToken() {
    const res = await fetch('/token', {method: 'POST',credentials: 'include'});

    if (res.status === 401) {console.log("No refresh token - user is not logged in.");return false;}

    if (!res.ok) {console.error("Unexpected refresh error:", await res.text());return false;}

    const data = await res.json();
    localStorage.setItem("accessToken", data.accessToken);

    const decoded = decodeJWT(data.accessToken);
    username = decoded.username;

    console.log("Access token refreshed.");
    return true;
}

async function startAutoRefresh() {
    const ok = await refreshAccessToken();

    // Don't keep retrying every 14 minutes if the user isn't logged in.
    if (!ok) return;

    setInterval(refreshAccessToken, 14 * 60 * 1000);
}
window.onload=async function(){

    setupSocketConnection();

    var regform=document.getElementById('RegisterForm');
    if(regform){
        regform.addEventListener('submit', async function (e) {
            e.preventDefault(); // prevent page reload

            const btn = document.getElementById("registerBtn");
            btn.disabled = true;              // prevent spamming
            btn.innerText = "Registering..."; // feedback

            const username = document.getElementById('Username').value;
            const password = document.getElementById('Password').value;

            emitRegister(username,password);
            localStorage.setItem("username", username);
        });
    }
    var logform=document.getElementById('LoginForm');
    if(logform){
        logform.addEventListener('submit', async function (e) {
            e.preventDefault(); // prevent page reload

            const username = document.getElementById('LUsername').value;
            const password = document.getElementById('LPassword').value;

            emitLogin(username,password);
            localStorage.setItem("username", username);
        });
    }

    if(document.getElementById("ThreeBlock")){
        window.addEventListener("resize", onResize);

        await startAutoRefresh();

        resumesession();
    }
    
    //the resource bar needs an overlay with some functionality, calling an emit for whichever resource
    //  and creating a display box for the user to see details about that resource
    // MakeToolTips()
    
    // addEventListenersToButtons();
}