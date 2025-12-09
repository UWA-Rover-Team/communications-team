import WebSocket from 'ws';
import { WebRTCMessage, clients, cameras } from './objects';

const socket = new WebSocket("ws://192.168.56.1:8080");

// ======================== Create new Stream Functions ===================
// Global storage for cameras. Undefined == Disconnected
const peerConnections: Record<cameras, RTCPeerConnection | undefined> = {
  FRONT: undefined,
  BACK: undefined,
  LEFT: undefined,
  RIGHT: undefined,
  MANIP: undefined
};
async function createStream(camera: cameras): Promise<void> {
  const pcCAM = new RTCPeerConnection();

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

  peerConnections[camera] = pcCAM; // JavaScript DOES reference automatically, so this object reference is the same

} 
 

// ============= Will think of a name later ============
async function init(): Promise<void> {

  await createStream("FRONT");
  await createStream("BACK");
  await createStream("LEFT");
  await createStream("RIGHT");
  await createStream("MANIP");
  
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

  init();
};




socket.onmessage = (event: any) => {
  const data: WebRTCMessage = JSON.parse(event.data);

  // Handle ANSWER

  // Handle ICE_CANDIDATE (incoming)

};
