// sender.js


// deviceID of cameras
const frontCameraId =   "UUeWY3GwAp3rogYYRKjMix0ETJSCdiDG/neZ5xLoPVQ=";
const leftCameraId =    "LQXeVP21cCGt44HPH73pUvFC7Gc8ld1b8Zi136vnzzQ=";
const rightCameraId =   "2noqUGsn6qKXNB9fjiWc4SoBOttpdywjT+jQfNLESGs=";
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
function checkForNewDevices() {
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


function connectCameras(pc) {
  
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
          console.log("Connecting Front Camera");
          addStream('front', device.deviceId, pc)
        }
      }

      // Connect Left Camera
      if (device.deviceId === leftCameraId) {
        const camera = cameraMap.get('leftCameraTrackId');
        if (camera !== null) {
          console.log("Left camera already connected.");
        } else {
          console.log("Connecting Left Camera");
          addStream('left', device.deviceId, pc)
        }
      }

      // Connect Right Camera
      if (device.deviceId === rightCameraId) {
        const camera = cameraMap.get('rightCameraTrackId');
        if (camera !== null) {
          console.log("Right camera already connected.");
        } else {
          console.log("Connecting Right Camera");
          addStream('right', device.deviceId, pc)
        }
      }

      // Connect Manipulator Camera
      if (device.deviceId === manipCameraId) {
        const camera = cameraMap.get('manipCameraTrackId');
        if (camera !== null) {
          console.log("Manip camera already connected.");
        } else {
          console.log("Connecting Manip Camera");
          addStream('manip', device.deviceId, pc);
        }
      }
    }
  });

  console.log("Read all devices connected");
}


function addStream(camera, cameraId, pc) {

  // Define the constraints we want to use
  const cameraConstraints = { video: {deviceId: cameraId,
    width: { ideal: 640 }, 
    height: { ideal: 480 }}, 
    audio: false 
  };
  
  navigator.mediaDevices.getUserMedia(cameraConstraints).then ((stream) => {
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
    
    return console.log("track added:", camera);
  }).then (() => {
    socket.send(JSON.stringify({ 
      type: "nextCamera",
      camera: camera,
      target: receiverName 
    }));
    return renegotiateOffer(peerConnection);
  })
}



// Function to resend the offer
function renegotiateOffer(pc) {
  
  pc.onicecandidate = (event) => {
    if (event.candidate) {
      socket.send(JSON.stringify({
        type: "candidate",
        candidate: event.candidate,
        target: receiverName
      }));
    }
  };

  pc.createOffer().then ((offer) => {
    return pc.setLocalDescription(offer);
  }).then (() => {
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
  });
}


// Listen for device changes
// navigator.mediaDevices.ondevicechange = checkForNewDevices;

