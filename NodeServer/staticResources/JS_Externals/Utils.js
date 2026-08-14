export function updateGridColumns() {
    try{
        // console.log("RAHHHHHHHHHHHHHHHHHHHHHHHHHH")
        const IndiOrTemplateButtons=document.getElementById("IndiOrTemplateButtons");
        if (window.innerWidth < 800) {
            IndiOrTemplateButtons.style.gridTemplateColumns = "auto auto 0";
        } else {
            IndiOrTemplateButtons.style.gridTemplateColumns = "auto auto 30%";
        }
    }catch(m){}
}


export function styleOuterDiv(elem){
    elem.style.width="calc(100% - max(8px, 0.6vw))"
    elem.style.minHeight="10px"
    elem.style.outline="lightgray dashed 0.1vw"; 
    elem.style.backgroundColor="rgba(216,216,216,0.2)"
    elem.style.padding="max(4px, 0.3vw)"
    elem.style.display="grid"
    elem.style.gridTemplateColumns="10% 90%"
    elem.style.columnGap="max(4px, 0.3vw)"
}

export function AddImageToElem(elem, URL){
    let Imgsec=document.createElement("img");
    Imgsec.style.width="100%"
    Imgsec.src=URL;
    Imgsec.style.backgroundColor="rgb(188, 187, 187)";
    Imgsec.style.objectFit="contain"
    Imgsec.style.display="block"
    Imgsec.style.aspectRatio="1/1"

    elem.appendChild(Imgsec)
}

export function styleInnerContainer(elem){
    elem.style.width="calc(100% - max(4px, 0.3vw))"
    elem.style.display="grid"
    elem.style.gridTemplateRows="40% calc(60% - max(4px, 0.3vw))"
    elem.style.rowGap="max(4px, 0.3vw)"
}

export function TitleAndCancelSection(elem, Title){
    let TCContainer=document.createElement("div");
    TCContainer.style.width="100%"
    TCContainer.style.height="100%"
    TCContainer.style.display = "flex";
    TCContainer.style.alignItems = "center";

    let TopTitle=document.createElement("div");
    TopTitle.innerHTML=Title;
    TopTitle.className="resourceText"
    TopTitle.style.fontSize="max(20px,1vw)";
    TopTitle.style.backgroundColor="rgb(188, 187, 187)";
    TopTitle.style.flex = "1";
    TopTitle.style.height = "100%";

    let Destroy=document.createElement("div");
    Destroy.style.height="100%"
    Destroy.style.aspectRatio="1/1"
    Destroy.style.backgroundColor="rgb(188, 187, 187)";
    Destroy.style.marginLeft = "auto";
    Destroy.style.backgroundImage="url('Icons/Cross.png')"
    Destroy.className="IconGeneral"
    Destroy.style.marginLeft="max(4px, 0.3vw)"

    TCContainer.appendChild(TopTitle);
    TCContainer.appendChild(Destroy);
    elem.appendChild(TCContainer);
}


export function StyleBotContainer(elem){
    elem.style.width="100%"
    elem.style.height="100%"
    elem.style.display = "flex";
    elem.style.alignItems = "center";
}

export function ProgressBar(elem){
    let ProgressBar=document.createElement("div");
    ProgressBar.style.backgroundColor="rgb(188, 187, 187)";
    ProgressBar.style.flex = "1";
    ProgressBar.style.height = "calc(100% - 2 * max(4px, 0.3vw))";
    ProgressBar.style.display = "flex";
    ProgressBar.style.alignItems = "center";
    ProgressBar.style.padding = "max(4px, 0.3vw)";

    let ProgressText = document.createElement("div");
    ProgressText.innerText = "Progress: ";
    ProgressText.style.fontSize="max(20px,1vw)";
    ProgressText.style.lineHeight="0.75";
    ProgressText.style.marginRight = "max(8px, 0.6vw)";

    ProgressBar.appendChild(ProgressText);

    let ProgressTrack = document.createElement("div");
    ProgressTrack.style.flex = "1";
    ProgressTrack.style.height = "75%";
    ProgressTrack.style.backgroundColor = "rgb(100, 100, 100)";
    ProgressTrack.style.position = "relative";
    ProgressTrack.style.overflow = "hidden";
    ProgressTrack.style.marginRight = "max(3px, 0.2vw)";

    ProgressBar.appendChild(ProgressTrack);

    let Progress = document.createElement("div");
    Progress.style.height = "100%";
    Progress.style.width = `${0}%`;
    Progress.style.backgroundColor = "rgb(50, 180, 70)";
    
    ProgressTrack.appendChild(Progress);

    elem.appendChild(ProgressBar);
}

export function DeployButton(elem){
    let Deploy=document.createElement("div");
    Deploy.style.height="100%"
    Deploy.style.aspectRatio="1/1"
    Deploy.style.backgroundColor="rgb(188, 187, 187)";
    Deploy.style.marginLeft = "auto";
    Deploy.style.backgroundImage="url('Icons/Deploy.png')"
    Deploy.className="IconGeneral"
    Deploy.style.marginLeft="max(4px, 0.3vw)"

    elem.appendChild(Deploy);
}

export function MoveToButton(elem){
    let MoveTo=document.createElement("div");
    MoveTo.style.height="100%"
    MoveTo.style.aspectRatio="1/1"
    MoveTo.style.backgroundColor="rgb(188, 187, 187)";
    MoveTo.style.marginLeft = "auto";
    MoveTo.style.backgroundImage="url('Icons/Deploy.png')"
    MoveTo.className="IconGeneral"
    MoveTo.style.marginLeft="max(4px, 0.3vw)"

    elem.appendChild(MoveTo);
}