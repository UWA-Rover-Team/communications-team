import type { WebRTCMessage, clients, resolution, dataPacket } from './webRtcStreamObject';
import {cameras} from './webRtcStreamObject';


// ====================== INIT ======================================================================
// Global storage for cameras. Undefined == Disconnected
const peerConnections: Record<cameras, RTCPeerConnection | undefined> = {
  [cameras.FRONT]: undefined,
  [cameras.BACK]: undefined,
  [cameras.LEFT]: undefined,
  [cameras.RIGHT]: undefined,
  [cameras.MANIP]: undefined
};

const videoElements: Record<cameras, HTMLVideoElement | null> = {
  [cameras.FRONT]: null,
  [cameras.BACK]: null,
  [cameras.LEFT]: null,
  [cameras.RIGHT]: null,
  [cameras.MANIP]: null
};

let socket: WebSocket | null = null;

export function connectSocket() {
  const ws = new WebSocket("ws://192.168.0.14:8080");

  ws.onopen = () => {
    socket = ws;
    setEventListeners();
    socket?.send(JSON.stringify({
      type: "REGISTER",
      client: "BASE_STATION",
      target: "SERVER",
    }));
  };

  ws.onerror = () => {
    ws.close();
  };

  ws.onclose = () => {
    console.log(new Error("Failed to connect to WebServer"));
    socket = null;
    setTimeout(connectSocket, 1000);
  };
}


// ============================ EXPORT functions for GUI ====================================
export async function requestCameraStream(cam : cameras, reso: resolution) {
  if (peerConnections[cam] !== undefined) {
    const pc = peerConnections[cam];
    pc.getSenders().forEach(sender => {
    if (sender.track) {
      sender.track.stop();
    }
    });
    
    pc.getReceivers().forEach(receiver => {
      if (receiver.track) {
        receiver.track.stop();
      }
    });

    pc.close();

    peerConnections[cam] = undefined;

    console.log(`Removed ${cam} Cameras peer connection`)
  }
  
  const pcCAM = new RTCPeerConnection();
  peerConnections[cam] = pcCAM;

  pcCAM.ontrack = (event) => {
    console.log(`Received track ${cam}`);  
    
    const videoElement = videoElements[cam]; 
    if (videoElement) {
      console.log(`Added Camera ${cam}`)
      videoElement.srcObject = event.streams[0];

      setTimeout(() => {
        console.log(`Video dimensions: ${videoElement.videoWidth}x${videoElement.videoHeight}`);
        console.log(`Video readyState: ${videoElement.readyState}`);
        console.log(`Video paused: ${videoElement.paused}`);
      }, 1000);


    } 
  };

  pcCAM.onicecandidate = (event) => {
    socket?.send(JSON.stringify({
      type: "ICE_CANDIDATE",
      client: "BASE_STATION",
      target: "ROVER",
      candidate: event.candidate,
      camera: cam
    }))
  }

  await waitForSocket();

  socket?.send(JSON.stringify({
    type: "CAMERA_REQUEST",
    client: "BASE_STATION",
    target: "ROVER",
    camera: cam,
    resolution: reso
  }));

  console.log(`Requested stream for ${cam}`);
  console.log(`camera is ${pcCAM}`);
}

function waitForSocket(): Promise<void> {
  return new Promise((resolve) => {
    if (socket?.readyState === WebSocket.OPEN) {
      resolve();
    }
    else socket?.addEventListener('open', () => resolve(), { once: true });
  });
}

// Svelte defines the video element, once its mounted to the DOM, it will register it here using this function. we can then add the stream, or sourceObj, as the stream camera track sent over webRTC
export function registerVideoElement(camera: cameras, element: HTMLVideoElement) {
  videoElements[camera] = element;
}


// ============= Handle RTC stuff =============
async function handleIceCandidate(data: WebRTCMessage) {
  if (data.camera && data.candidate) {
    const pcCAM = peerConnections[data.camera];
    if (pcCAM) {
      await pcCAM.addIceCandidate(new RTCIceCandidate(data.candidate));
      console.log(`ICE candidate added for ${data.camera}`);
    }
  }
}

async function handleOffer(data: WebRTCMessage) {
  if (data.camera) {
    const pcCAM = peerConnections[data.camera];
    if (pcCAM) {
      await pcCAM.setRemoteDescription(new RTCSessionDescription({
        type: "offer",
        sdp: data.sdp
      }));

      const answer = await pcCAM.createAnswer();
      await pcCAM.setLocalDescription(answer);

      socket?.send(JSON.stringify({
        type: "ANSWER",
        client: "BASE_STATION",
        target: "ROVER",
        sdp: pcCAM.localDescription?.sdp,
        camera: data.camera
      }));

      console.log(`Answer sent for ${data.camera}`);
    }
  }
}

// =========== WatchDogs ===================
export function frameWatchdog(cam : cameras, timeout = 5000, cameraConnectTime = 15000) {
  let cameraConnectingTimer = Date.now() - cameraConnectTime;
  let lastFrameCount = 0;
  let lastFrameTime = Date.now();

  const interval = setInterval(async () => {
    if (socket !== null) {
      const pcCAM = peerConnections[cam];
      if (Date.now() - cameraConnectingTimer > cameraConnectTime) {
        console.log(pcCAM)
        if (pcCAM !== undefined) {
          const stats = await pcCAM.getStats();
          
          stats.forEach(report => {
            if (report.type === 'inbound-rtp' && report.kind === 'video') {
              const currentFrames = report.framesReceived;
              console.log(`Current frames recieved ${currentFrames}`)
              
              if (currentFrames > lastFrameCount) {
                console.log(`Current frame count is ${currentFrames}`)
                lastFrameCount = currentFrames;
                lastFrameTime = Date.now();
              } else if (Date.now() - lastFrameTime > timeout) {
                console.log('No frames recetnly');
                
              }
            }
          });
        }
        else {
          console.log("pcCAM is undefined ")
          requestCameraStream(cam, [640, 480])
          cameraConnectingTimer = Date.now();
        }
      }
    }
  }, timeout);
}


// ========= Global Listner events ==========
function setEventListeners() {
  if (socket) {

    socket.onopen = () => {
      const registerMsg: WebRTCMessage = {
        type: "REGISTER",
        client: "BASE_STATION",  
        target: "SERVER"
      };
      
      socket?.send(JSON.stringify(registerMsg));
      console.log("Registered to the server");
    };
    
    
    socket.onmessage = async (event: any) => {
      const data: WebRTCMessage | dataPacket = JSON.parse(event.data);
      
      if (data.type === 'OFFER') { 
        await handleOffer(data);
      }
      
      if (data.type === 'ICE_CANDIDATE') {
        await handleIceCandidate(data);
      }
      
      if (data.type === "ERROR") {
        if (data.error === "DISCONNECT") {
          Object.keys(peerConnections).forEach((key : string) => {
            const camera = key as unknown as cameras;
            if (peerConnections[camera]) {
              peerConnections[camera].close();
              peerConnections[camera] = undefined;
            }
          });
          console.log("Removed all Peer Connections");
        }
        console.log(data.error);
      }
      
      if (data.type === "DATAPACKET") {
        console.log("Recieved packet from Agni");
        
      }
    }
  }
}
  
  