// sender.js

// TO WORK ON
// USB disconnect detected, but no reconnect
// Receiver is not disinguishing between deleted track and new track, failing to notice ontrack event. Maybe change to a different listener for ontrack event?


// deviceID of cameras
const frontCameraId =   "LQXeVP21cCGt44HPH73pUvFC7Gc8ld1b8Zi136vnzzQ=";
const leftCameraId =    "2noqUGsn6qKXNB9fjiWc4SoBOttpdywjT+jQfNLESGs=";
const rightCameraId =   "UUeWY3GwAp3rogYYRKjMix0ETJSCdiDG/neZ5xLoPVQ=";
const manipCameraId =   "DhFg29xQSsnPa9y4zu6Rh0uKQsdHDIfuv/HVVe0D12A=";

const socket = new WebSocket("ws://192.168.2.173:8080");
let peerConnection = new RTCPeerConnection();

const senderName = "sender";
const receiverName = "receiver";

const cameraMap = new Map([
                          ['frontCameraTrackId', null],
                          ['leftCameraTrackId', null],
                          ['rightCameraTrackId', null],
                          ['manipCameraTrackId', null]
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
    await navigator.mediaDevices.getUserMedia({ video: true }).then(stream => {
      stream.getTracks().forEach(track => track.stop());
    })

    peerConnection = new RTCPeerConnection();
    await checkForNewDevices();
  }

  else if(data.type === "peerdisconnect") {
    peerConnection.close()
    previousVideoDevices = [];
    cameraMap.forEach((value, key) => {
      cameraMap.set(key, null);
    });
    console.log("Receiver has left the chat. Closing peer connection");
  }

  else {
    console.warn("Sender received unknown message type:", data);
  }
};


let previousVideoDevices = [];
async function checkForNewDevices() {
  console.log("previous devices are:", previousVideoDevices);
  navigator.mediaDevices.enumerateDevices().then (devices => {
    const currentVideoDevices = devices.filter(device => device.kind === "videoinput");
    console.log("current devices:", currentVideoDevices);
    // Compare previous list with current list
    const previousIds = previousVideoDevices.map(device => device.deviceId);
    const newDevices = currentVideoDevices.filter(device => !previousIds.includes(device.deviceId));
    
    // Update the previous devices list for future comparisons
    previousVideoDevices = currentVideoDevices;
    return newDevices
  }).then ((newDevices)=> {
    if (newDevices.length > 0) {
      connectCameras(peerConnection);
      console.log("New device(s) added:", newDevices);
    }
  })
}


// Capture video and create offer
async function connectCameras(pc) {
  
  // List devices and then filter videoinput ones
  navigator.mediaDevices.enumerateDevices().then (devices => {
    const videoDevices = devices.filter(device => device.kind === 'videoinput');
    // Loop through all found video inputs and send them over their own track
    for (const [index, device] of videoDevices.entries()) {

      // Connect Front Camera
      if (device.deviceId === frontCameraId) {
        const camera = cameraMap.get('frontCameraTrackId');
        if (camera !== null) {
          console.log("front camera already connected.");
        } else {
          console.log("Front camera has connected, updating offer");
          addStream('front', device.deviceId, pc)
        }
      }

      // Connect Left Camera
      if (device.deviceId === leftCameraId) {
        const camera = cameraMap.get('leftCameraTrackId');
        if (camera !== null) {
          console.log("Left camera already connected.");
        } else {
          console.log("Left camera has connected, updating offer");
          addStream('left', device.deviceId, pc)
        }
      }

      // Connect Right Camera
      if (device.deviceId === leftCameraId) {
        const camera = cameraMap.get('rightCameraTrackId');
        if (camera !== null) {
          console.log("Right camera already connected.");
        } else {
          console.log("Right camera has connected, updating offer");
          addStream('right', device.deviceId, pc)
        }
      }

      // Connect Manipulator Camera
      if (device.deviceId === leftCameraId) {
        const camera = cameraMap.get('manipCameraTrackId');
        if (camera !== null) {
          console.log("Manip camera already connected.");
        } else {
          console.log("Manip camera has connected, updating offer");
          addStream('manip', device.deviceId, pc);
        }
      }
    }
  });

  console.log("Read all devices connected");
}


async function addStream(camera, cameraId, pc) {

  // Define the constraints we want to use
  const cameraConstraints = { video: {deviceId: cameraId,
    width: { ideal: 640 }, 
    height: { ideal: 480 }}, 
    audio: false 
  };
  
  navigator.mediaDevices.getUserMedia(cameraConstraints).then ((stream) => { // Want to remove the await, but it doesnt work for now
    const tracks = stream.getTracks();
    const videoTrack = tracks[0];
  
    cameraMap.set(`${camera}CameraTrackId`, videoTrack);
    const sender = pc.addTrack(videoTrack, stream);

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
    
    console.log("track added:", camera);
  }).then (() => {
    socket.send(JSON.stringify({ 
      type: "nextCamera",
      camera: camera,
      target: receiverName 
    }));
    renegotiateOffer(peerConnection);
  })
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


// Listen for device changes
// navigator.mediaDevices.ondevicechange = checkForNewDevices;

