import WebSocket from 'ws';
import { RTCPeerConnection, RTCSessionDescription, RTCIceCandidate, nonstandard, MediaStream} from '@roamhq/wrtc';
import { WebRTCMessage, clients, cameras, resolution } from './webRtcStreamObject';


const socket = new WebSocket("ws://192.168.56.1:8080");

// ======================== Create new Stream Functions ===================
// Global storage for cameras. Undefined == Disconnected
const peerConnections: Record<cameras, RTCPeerConnection | undefined> = { // records are like dictionary types
FRONT: undefined,
BACK: undefined,
LEFT: undefined,
RIGHT: undefined,
MANIP: undefined
};

async function createStream(camera: cameras, resolution: resolution): Promise<void> {
  const pcCAM = new RTCPeerConnection();
  peerConnections[camera] = pcCAM; // JavaScript DOES reference automatically, so this object reference is the same
  
  // OUTGOING ICE Listener event (every new ICE candidate this finds, run that)
  pcCAM.onicecandidate = (event) => { 
    if (event.candidate) {
      socket.send(JSON.stringify({
        type: "ICE_CANDIDATE",
        client: "ROVER",
        target: "BASE_STATION",
        candidate: event.candidate,
        camera: camera
      }));
    }
  };
  
  const mediaStream = new MediaStream;
  const mediaTrack = createMockVideoTrack(resolution);
  
  pcCAM.addTrack(mediaTrack, mediaStream); // Media stream is a collection of tracks (audio, visual etc.)
  
  
  // Create and send the offer
  const offerCAM = await pcCAM.createOffer();
  await pcCAM.setLocalDescription(offerCAM);
  
  socket.send(JSON.stringify({
    type: "OFFER",
    client: "ROVER",
    target: "BASE_STATION",
    sdp: pcCAM.localDescription?.sdp,
    camera: camera,
  }));
  
  console.log(`Set camera ${camera}`);
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
socket.onopen = () => {
  const registerMsg: WebRTCMessage = {
    type: "REGISTER",
    client: "ROVER", 
    target: "SERVER"
  };
  
  socket.send(JSON.stringify(registerMsg));
  console.log("Registered to the server");
};


socket.onmessage = async (event: any) => {
  const data: WebRTCMessage = JSON.parse(event.data);
  
  if (data.type === 'CAMERA_REQUEST') {
    if (data.camera && data.resolution) {
      console.log("Camera stream requested...");
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