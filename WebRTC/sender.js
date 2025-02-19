// sender.js

// TO WORK ON
// USB disconnect detected, but no reconnect
// Receiver is not disinguishing between deleted track and new track, failing to notice ontrack event. Maybe change to a different listener for ontrack event?


// deviceID of cameras
const frontCameraId = "LQXeVP21cCGt44HPH73pUvFC7Gc8ld1b8Zi136vnzzQ=";
const leftCameraId = "2noqUGsn6qKXNB9fjiWc4SoBOttpdywjT+jQfNLESGs=";

const socket = new WebSocket("ws://192.168.2.173:8080");
let peerConnection = new RTCPeerConnection();

const senderName = "sender";
const receiverName = "receiver";


const cameraMap = new Map([
                          ['frontCameraTrackId', null],
                          ['leftCameraTrackId', null],
                          ['rightCameraTrackId', null]
                        ]);



// Register sender
socket.onopen = () => {
    socket.send(JSON.stringify({ 
        type: "register", 
        name: senderName 
    }));
    console.log("Registered to the server");
}

// Handle incoming messages
socket.onmessage = async (event) => {
  const data = JSON.parse(event.data);

  if (data.type === "answer") {
    // Reconstruct the full answer object expected by setRemoteDescription
    const answer = { type: "answer", sdp: data.sdp };
    console.log("Answer received");
    await peerConnection.setRemoteDescription(new RTCSessionDescription(answer));
    console.log("Remote description set successfully.");
  } 
  
  else if (data.type === "candidate") {
    console.log("ICE candidate received from receiver:", data.candidate);
    await peerConnection.addIceCandidate(new RTCIceCandidate(data.candidate));
    console.log("ICE candidate added successfully.");
  } 
  
  else if(data.type === "new_receiver") {
    console.log("New receiver found. Sending Offer...");
    peerConnection = new RTCPeerConnection();
    await checkForNewDevices();
    console.log("New offer sent");
  }

  else if(data.type === "peerdisconnect") {
    peerConnection.close()
    console.log("Receiver has left the chat. Closing peer connection");
  }

  else {
    console.warn("Sender received unknown message type:", data);
  }
};


let previousVideoDevices = [];
console.log("previous devices are:". previousVideoDevices);
async function checkForNewDevices() {
  const devices = await navigator.mediaDevices.enumerateDevices();
  const currentVideoDevices = devices.filter(device => device.kind === "videoinput");
  console.log("current devices:", currentVideoDevices);
  // Compare previous list with current list
  const previousIds = previousVideoDevices.map(device => device.deviceId);
  const newDevices = currentVideoDevices.filter(device => !previousIds.includes(device.deviceId));
  
  if (newDevices.length > 0) {
    console.log("New device(s) added:", newDevices);
    await connectCameras(peerConnection);
  }
  
  // Update the previous devices list for future comparisons
  previousVideoDevices = currentVideoDevices;
}


// Capture video and create offer
async function connectCameras(pc) {
  
  // List devices and then filter videoinput ones
  const devices = await navigator.mediaDevices.enumerateDevices();
  const videoDevices = devices.filter(device => device.kind === 'videoinput');
  console.log("List of video devices:", videoDevices);

  // Loop through all found video inputs and send them over their own track
  for (const [index, device] of videoDevices.entries()) {
    if (device.deviceId === frontCameraId) {
      const camera = cameraMap.get('frontCameraTrackId');
      if (camera !== null) {
        console.log("front camera already connected.");
      } else {
        console.log("front camera has connected, updating offer");
        addStream('front', device.deviceId, pc).then(() => {
          renegotiateOffer(peerConnection);
        });
      }
    }

    if (device.deviceId === leftCameraId) {
      const camera = cameraMap.get('leftCameraTrackId');
      if (camera !== null) {
        console.log("Left camera already connected.");
      } else {
        console.log("Left camera has connected, updating offer");
        addStream('left', device.deviceId, pc).then(() => {
          renegotiateOffer(peerConnection);
        });
      }
    }
  }
}


async function addStream(camera, cameraId, pc) {

  const cameraConstraints = { video: {deviceId: cameraId,
    width: { ideal: 640 }, 
    height: { ideal: 480 }}, 
    audio: false 
  };
  
  const stream = await navigator.mediaDevices.getUserMedia(cameraConstraints)
  const tracks = stream.getTracks();
  const videoTrack = tracks[0];



  cameraMap.set(`${camera}CameraTrackId`, videoTrack);
  const sender = pc.addTrack(videoTrack, stream);
  console.log(`Sender's ${camera} track ID:`, videoTrack.id);

  

  // Update sender parameters
  const parameters = sender.getParameters();
  parameters.encodings[0].maxBitrate = 100000; // 0.1 Mbps
  console.log("Offer updated");
  sender.setParameters(parameters);

  videoTrack.onended = () => {
    console.log(`${camera} camera track ended. The device might have been disconnected.`);
    pc.removeTrack(sender);
    renegotiateOffer(pc);
    cameraMap.set(`${camera}CameraTrackId`, null);
    console.log(cameraMap);
  };
}


// Function to resend the offer
async function renegotiateOffer(pc) {
  pc.onicecandidate = (event) => {
    if (event.candidate) {
      socket.send(JSON.stringify({
        type: "candidate",
        candidate: event.candidate,
        target: receiverName
      }));
    }
  };

  const offer = await pc.createOffer();
  await pc.setLocalDescription(offer);
  console.log("Local description set succesfully");

  socket.send(
    JSON.stringify({
      type: "offer",
      offer: pc.localDescription,
      target: receiverName,
    })
    
  );
  console.log("Offer sent success");

  return pc;
}



// Initialize the device list on startup


// Listen for device changes
navigator.mediaDevices.ondevicechange = checkForNewDevices;

