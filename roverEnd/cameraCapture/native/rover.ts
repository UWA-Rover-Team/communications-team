import WebSocket from 'ws';
import { RTCPeerConnection, RTCSessionDescription, RTCIceCandidate, nonstandard, MediaStream} from '@roamhq/wrtc';
import type { WebRTCMessage, clients, resolution } from './webRtcStreamObject';
import { cameras } from './webRtcStreamObject';
import { RTCVideoFrame } from '@roamhq/wrtc/types/nonstandard';
const vimbax = require('../build/Release/addon');

const { RTCVideoSource } = nonstandard;

let socket: WebSocket | null = null;

export function connectSocket() {
  const ws = new WebSocket("ws://192.168.0.14:8080");

  ws.onopen = () => {
    socket = ws;
    setEventListeners();
    socket?.send(JSON.stringify({
        type: "REGISTER",
        client: "ROVER",
        target: "SERVER",
    }));
  };

  ws.onerror = () => {
    ws.close();
  };

  ws.onclose = () => {
    console.log(new Error("Failed to connect to WebServer"));
    socket = null;
    setTimeout(connectSocket, 3000);
  };
}

connectSocket();

const vimbaSystem = new vimbax.VimbaXSystem();

// ======================== Create new Stream Functions ===================
// Global storage for cameras. Undefined == Disconnected
const peerConnections: Record<cameras, RTCPeerConnection | undefined> = {
  [cameras.FRONT]: undefined,
  [cameras.BACK]: undefined,
  [cameras.LEFT]: undefined,
  [cameras.RIGHT]: undefined,
  [cameras.MANIP]: undefined
};

async function createStream(camera: cameras, resolution: resolution): Promise<void> {
  const pcCAM = new RTCPeerConnection();
  peerConnections[camera] = pcCAM;
  
  pcCAM.onicecandidate = (event) => { 
    if (event.candidate) {
      socket?.send(JSON.stringify({
        type: "ICE_CANDIDATE",
        client: "ROVER",
        target: "BASE_STATION",
        candidate: event.candidate,
        camera: camera
      }));
    }
  };
  
  // Wait for track to be ready
  const mediaTrack = await requestCamera(camera);
  const mediaStream = new MediaStream([mediaTrack]); 
    
  pcCAM.addTransceiver(mediaTrack, {
    streams: [mediaStream],
    sendEncodings: [{
      maxFramerate: 32 
    }]
  });
  
  // Give it a moment to ensure track is active
  await new Promise(resolve => setTimeout(resolve, 100));
  
  const offerCAM = await pcCAM.createOffer({
    offerToReceiveAudio: false,
    offerToReceiveVideo: false 
  });
  await pcCAM.setLocalDescription(offerCAM);
  
  socket?.send(JSON.stringify({
    type: "OFFER",
    client: "ROVER",
    target: "BASE_STATION",
    sdp: pcCAM.localDescription?.sdp,
    camera: camera,
  }));
  
  console.log(`Set camera ${camera}`);
}

// Make this return a Promise
function requestCamera(cameraId: Number): Promise<MediaStreamTrack> {
  return new Promise((resolve, reject) => {
    const source = new RTCVideoSource();
    const track = source.createTrack();
    let err: Number;


    err = vimbaSystem.startCapture(cameraId, (frameData: { buffer: Buffer, width: number, height: number }) => {
      const frame: RTCVideoFrame = {
        width: frameData.width,
        height: frameData.height,
        data: new Uint8Array(frameData.buffer),
      };
      source.onFrame(frame);
    });
    if (err != 0) {
      socket?.send(JSON.stringify({
        type: "ERROR",
        client: "ROVER",
        target: "BASE_STATION",
        error: `Failed to capture camera ${cameraId}`
      }));
    }
    
    // Resolve after first frame or timeout
    setTimeout(() => resolve(track), 500);
  });
}

function createMockVideoTrack(resolution: resolution): MediaStreamTrack {
  const source = new nonstandard.RTCVideoSource();
  const track = source.createTrack();
  
  const width = resolution[0];
  const height = resolution[1];
  
  // Create I420 data once
  const yPlaneSize = width * height;
  const uvPlaneSize = (width / 2) * (height / 2);
  const data = new Uint8Array(yPlaneSize + uvPlaneSize * 2);
  
  // Fill with gray (Y=128, U=128, V=128)
  data.fill(60);
  
  setInterval(() => {
    source.onFrame({ width, height, data });
  }, 33);
  
  return track;
}

// =============== Handling RTC Stuff functions ================
async function handleAnswer(data: WebRTCMessage) {
  if (data.camera && data.sdp) {
    const pcCAM = peerConnections[data.camera];
    if (pcCAM) {
      const description = new RTCSessionDescription({
        type: "answer",
        sdp: data.sdp
      });
      await pcCAM.setRemoteDescription(description);
      console.log(`Answer received for ${data.camera}`);
    }
  }
}

async function handleIceCandidate(data: WebRTCMessage) {
  if (data.camera && data.candidate) {
    const pcCAM = peerConnections[data.camera];
    if (pcCAM) {
      await pcCAM.addIceCandidate(new RTCIceCandidate(data.candidate));
      console.log(`ICE candidate added for ${data.camera}`);
    }
  }
}



// =============== MAIN LOOP ===============
function setEventListeners() {
  if (socket) {

    socket.onopen = () => {
      const registerMsg: WebRTCMessage = {
        type: "REGISTER",
        client: "ROVER", 
        target: "SERVER"
      };
      
      socket?.send(JSON.stringify(registerMsg));
      console.log("Registered to the server");
    };
    
    
    socket.onmessage = async (event: any) => {
      const data: WebRTCMessage = JSON.parse(event.data);
      
      if (data.type === 'CAMERA_REQUEST') {
        if (data.camera && data.resolution) {
          console.log(`Camera stream ${data.camera} requested...`);
          createStream(data.camera, data.resolution);
        }
      }
      
      if (data.type === 'ANSWER') {
        await handleAnswer(data);
      }
      
      if (data.type === 'ICE_CANDIDATE') {
        await handleIceCandidate(data);
      }
    };
  }
}