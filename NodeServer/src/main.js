// import net from "net";
const net = require("net");

const {authenticateTokenImport,
    RefreshTokenImport,
    AccessTokenImport,
    verifyImport,
    socketUtilImport
}=require("./modules/Verification/Verification.js")

const http = require('http');
const express=require("express");
const path = require('path')
const { Server } = require('socket.io');
const { Console } = require('console');

const cookieParser = require('cookie-parser');


let usersocketMapAttempts=new Map();

let usersocketMap=new Map();
let UserMessages=new Map();

let UserRefreshTokens=new Map();


const pipe = net.connect("\\\\.\\pipe\\SchedulerPipe");
const pipeMsgs = net.connect("\\\\.\\pipe\\SchedulerPipeMessage");

pipe.on("connect", () => {console.log("Connected to scheduler");});
pipeMsgs.on("connect", () => {console.log("Ready for scheduler messages");});

let pipeBuffer = "";
pipeMsgs.on('data', (data) => {
    // console.log("RAW PIPE DATA:", data.toString());
    pipeBuffer+=data.toString();
    const lines = pipeBuffer.split("\n");
    pipeBuffer = lines.pop();

    for (const line of lines) {
        if (!line.trim()) continue; // Skip empty chunks
        try {
            // console.log("RAW PIPE DATA (FRAMED):", line);
            const msg = JSON.parse(line);
            var accessToken;
            var refreshToken;
            var socketdirect;

            var sockid;
            try{
                accessToken = AccessTokenImport(msg.username)
                refreshToken = RefreshTokenImport(msg.username)
            }catch(e){}
            switch(msg.type){
                case "RegisterResult":
                    var sock=usersocketMapAttempts.get(msg.RId);
                    usersocketMap.set(msg.username,sock)
                    usersocketMapAttempts.delete(msg.RId);

                    UserRefreshTokens.set(msg.username,refreshToken);

                    socketdirect=io.sockets.sockets.get(sock);
                    socketdirect.authenticated=true;
                    socketdirect.username=msg.username;

                    io.to(sock).emit("redirect",{"RequestMetaData":{accessToken}});
                    break;
                case "RegisterFail":
                    usersocketMapAttempts.delete(msg.RId);
                    io.to(sock).emit("failedReg");
                    break;
                case "LoginResult":
                    var sock=usersocketMapAttempts.get(msg.RId);
                    usersocketMap.set(msg.username,sock)
                    usersocketMapAttempts.delete(msg.RId);

                    UserRefreshTokens.set(msg.username,refreshToken);

                    socketdirect=io.sockets.sockets.get(sock);
                    socketdirect.authenticated=true;
                    socketdirect.username=msg.username;

                    io.to(sock).emit("redirect",{"RequestMetaData":{accessToken}});
                    break;
                case "LoginFailed":
                    usersocketMapAttempts.delete(msg.RId);
                    io.to(sock).emit("failedLog");
                    break;
                case "TechDetails":
                    sockid=usersocketMap.get(msg.username)
                    // socketdirect=io.sockets.sockets.get(usersocketMap.get(msg.username));
                    io.to(sockid).emit("TechTreeUpdate",{details:msg.details});
                    break;
                case "ConstructionOptions":
                    sockid=usersocketMap.get(msg.username)
                    // console.log(msg.details)
                    // socketdirect=io.sockets.sockets.get(usersocketMap.get(msg.username));
                    io.to(sockid).emit("availableBuildingTypes",{details:msg.details});
                    break;
                case "TrainingDetails":
                    sockid=usersocketMap.get(msg.username)
                    io.to(sockid).emit("RegLoad",{details: msg.details});
                    break;
                case "NewTrainingSuccess":
                    sockid=usersocketMap.get(msg.username)
                    io.to(sockid).emit("NewRegimen",{slot: msg.slot,regName:msg.name});
                    break;
                case "TileDetails":
                    sockid=usersocketMap.get(msg.username)
                    io.to(sockid).emit('RecTiles',{"RequestMetaData":msg.details});

                    const ogtile=msg.details.tiles[0];
                    for(const username of msg.details.usernames){
                        if(username==msg.username){continue;}
                        sockid=usersocketMap.get(username);
                        io.to(sockid).emit('RecTiles',{"RequestMetaData":{tiles:[ogtile]} });
                    }
                    break;
                case "PlacementMovement":
                    sockid=usersocketMap.get(msg.username)
                    // console.log(msg)
                    io.to(sockid).emit('buildingplacementhover',{"RequestMetaData":msg});

                    break;
                default:;
            }
        }catch(e){console.log("parse error")}
    }
});

const PORT= 5000
const app=express()//creates server
const server = http.createServer(app);
const io = new Server(
    server,{    
        cors: {
            origin: 'http://localhost:'+PORT,
            credentials: true
}   }   );

server.listen(PORT,()=>{console.log("listening to port 5000")})

app.use(express.static("./staticResources"))
app.use(express.static("./staticResources/JS_Externals"))
app.use(cookieParser());
app.use(express.json()); // <-- This must come BEFORE your POST route handlers

app.get("/homepage",(req,res)=>{res.status(200).sendFile(path.join(__dirname,'../sitePages/Homepage.html'))})

app.get("/play",(req,res)=>{res.status(200).sendFile(path.join(__dirname,'../sitePages/index.html'))})




app.get('/Tiles/TextureMaps/{*any}', authenticateTokenImport , async (req, res) => {
    const filePath = req.params; // captures everything after /Tiles/TextureMaps/

    res.status(200).sendFile(path.join(__dirname,'../Tiles/TextureMaps',filePath.any[0]))

});

app.get('/Tiles/HeightMaps/{*any}', authenticateTokenImport, async (req, res) => {
    const filePath = req.params; // captures everything after /Tiles/TextureMaps/
    res.status(200).sendFile(path.join(__dirname,'../Tiles/HeightMaps',filePath.any[0]))
});

app.get('/Tiles/WalkMaps/{*any}', authenticateTokenImport, async(req, res) => {
    const filePath = req.params; // captures everything after /Tiles/TextureMaps/

    res.status(200).sendFile(path.join(__dirname,'../Tiles/WalkMaps',filePath.any[0]))
});

app.get('/Assets/GLB_Exports/{*any}', authenticateTokenImport, async(req, res) => {
    const filePath = req.params; // captures everything after /Tiles/TextureMaps/

    res.sendFile(path.resolve(__dirname,'../Assets/GLB_Exports',filePath.any[0]))

});



app.post('/token', async (req, res) => {
    const refreshToken = req.cookies.refreshToken;

    if (!refreshToken){return res.status(401).json({ message: "No refresh token provided" });} 

    try {
        const payload = verifyImport(refreshToken);

        // Check if refreshToken is still valid (optional)
        const userToken = UserRefreshTokens.get(payload.username);
        
        if (!userToken){return res.status(403).json({ message: "Invalid refresh token or no user" });}

        const accessToken = AccessTokenImport(payload.username);

        res.json({ accessToken });
    } 
    catch (err) {return res.status(403).json({ message: "Invalid or expired refresh token" });}
});

app.post('/RefreshToken', async (req, res) => {
    const username = req.body.username;
    const RefreshToken=UserRefreshTokens.get(username);
    res.cookie('refreshToken', RefreshToken, { httpOnly: true, secure: true, sameSite: 'Strict' });
    res.sendStatus(200);
});


app.get('/{*any}',(req,res)=>{res.status(200).send("pluh")/*handles undefined urls*/})


var requestId=0;

io.on('connection', async (socket) => {
    socket.authenticated = false;
    console.log("Connected:", socket.id);
    
    socket.on("ResumeSession", ({ RequestMetaData }) => {
        const username = RequestMetaData.username;
        const AT = RequestMetaData.token;
        
        const storedRefresh = UserRefreshTokens.get(username);

        if( !username || !storedRefresh ){return;}

        const newAccessToken = AccessTokenImport(username);
        
        if (newAccessToken !== AT) {return;}

        socket.authenticated = true;
        socket.username = username;
        usersocketMap.set(username, socket.id);


        console.log("Session resumed for:", username, "socket:", socket.id);
        socket.emit("bootup")

    });

    socket.on('Register',async({RequestMetaData}) => {
        const username=RequestMetaData.username;
        const password=RequestMetaData.password;
        console.log("user wants to register",username,password)
        pipe.write(JSON.stringify({
            type: "Register",
            RId:requestId.toString(),
            username: username,
            password: password,
        }) + "\n");
        usersocketMapAttempts.set(requestId,socket.id);
        requestId++;
    });

    socket.on('Login',async({RequestMetaData}) => {
        const username=RequestMetaData.username;
        const password=RequestMetaData.password;
        console.log("user wants to login",username,password)
        pipe.write(JSON.stringify({
            type: "Login",
            RId:requestId.toString(),
            username: username,
            password: password,
        }) + "\n");
        usersocketMapAttempts.set(requestId,socket.id);
        requestId++;
    });


    socket.on('GetTiles',async() => {
        if(!socket.authenticated){console.log("unauthorised tile request");return;}
        const username=socket.username//RequestMetaData.username;
        console.log("user wants to gettiles",username)
        //push to scheduler that user needs their relevant tiles

        pipe.write(JSON.stringify({
            type: "TilesRequest",
            username: username,
        }) + "\n");

    })


    socket.on('DailyReward',async() => {
        if(!socket.authenticated){console.log("unauthorised tile request");return;}
    })

    socket.on('TechTreeInfo',async() => {
        if(!socket.authenticated){console.log("unauthorised tile request");return;}
        console.log("TechTreeUpdate",socket.username, socket.id)
        pipe.write(JSON.stringify({
            type: "TechTreeUpdate",
            username: socket.username.toString(),
        }) + "\n");
    })

    socket.on('constructable',async() => {
        if(!socket.authenticated){console.log("unauthorised tile request");return;}
        // console.log("yeah i do want my constructables?")
        pipe.write(JSON.stringify({
            type: "ConstructableUpdate",
            username: socket.username,
        }) + "\n");
    })

    socket.on('trainable',async() => {
        if(!socket.authenticated){console.log("unauthorised tile request");return;}

        //updates what regimens the user has in training
        pipe.write(JSON.stringify({
            type: "TrainableUpdate",
            username: socket.username,
        }) + "\n");
    })

    socket.on('unitTypes',async() => {
        if(!socket.authenticated){console.log("unauthorised tile request");return;}

        const types={
            "ARCHER":{Description:"Uses a bow to eliminate foes from afar",unlocked:true},
            "SPEARMAN":{Description:"A basic infantry unit",unlocked:true},
            "SWORDSMAN":{Description:"a durable infantry unit",unlocked:true}
        }
        socket.emit("availableUnitTypes",{types})
    })

    //see if they need a Daily Login reward
    // await LoginRewardCheckup(socket.userId)
    // await TickManager.ConstructableMessage(socket.userId);

    //sockets pertaining to production

    socket.on('ProductionSetupRequest',async() => {});

    socket.on('requestProductionLine',async ({RequestMetaData}) =>{});

    socket.on('ChangeFactoryCountForProd',async ({RequestMetaData}) =>{});

    socket.on('CloseProductionLine',async ({RequestMetaData}) =>{});

    socket.on('requestingProductionInventory',async ()=>{})

    socket.on('ChangeFactoryScaleForProd',async ({RequestMetaData}) =>{});

    
    //---------------------------------------------------
    socket.on('BuildingPlacementMovement',async ({RequestMetaData}) =>{
        if(!socket.authenticated){console.log("unauthorised tile request");return;}
        // console.log("BuildingPlacementMovement",RequestMetaData)
        pipe.write(JSON.stringify({
            type: "PlacementMovement",
            username: socket.username,
            building:RequestMetaData.Building,
            position:RequestMetaData.pos,
            taskId:RequestMetaData.tId
        }) + "\n");
    });

    socket.on('BuildingPlacement',async ({RequestMetaData}) =>{})


    //sockets pertaining to unit movement and creation
    socket.on('MovementCommand',async ({RequestMetaData}) => {});

    socket.on('NewRegimen',async ({RequestMetaData}) => {
        if(!socket.authenticated){console.log("unauthorised tile request");return;}
        // console.log(RequestMetaData)
        pipe.write(JSON.stringify({
            type: "NewRegimen",
            username: socket.username,
            regName:RequestMetaData,
        }));

    });

    socket.on('AdjustRegimen',async ({RequestMetaData}) => {});

    socket.on('unitdeploymentposition',async ({RequestMetaData}) => {});

    socket.on('RegimenDeploy',async ({RequestMetaData}) => {});

    socket.on('DestroyRegimen',async ({RequestMetaData}) => {});
    

    // Handle disconnect
    socket.on("disconnect", () => {});
});


async function gameTick() {
    for (const [userId, Message] of UserMessages) {
        const TheirSocket=usersocketMap.get(userId)

        try{
            for (const value of TheirSocket) {
                io.to(value).emit('TickUpdate', Message);
            }
        }catch(nosoc){console.log("no socket?",nosoc)}
    
    }
}

setInterval(gameTick, 200);//5 calls a second
// setInterval(TickManager.ResourceMessage.bind(TickManager), TickManager.GetResourceTickRate());