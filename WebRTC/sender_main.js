// sender.js
// This file now creates a separate RTCPeerConnection per camera,
// and includes the camera identifier in signaling messages.

const frontCameraId =   "CPCzDkgEpvOqkvAfv9UFxA4mxG8xikCtXtWXIgjlHD0=";
const manipCameraId =   "+02XaXkOoSeK54GA3nlGoz1wSaq7HL3MnWAO6ar16Uc=";

const socket = new WebSocket("ws://192.168.2.173:8080");

const senderName = "sender_main";
const receiverName = "receiver";

// Map to track video tracks for each camera.
const cameraMap = new Map([
  ['frontCameraTrackId', null],
  ['manipCameraTrackId', null]
]);

// Object to store separate peer connections for each camera.
const peerConnections = {
  front: null,
  left: null,
  right: null,
  manip: null,
};

// Register sender with the signaling server.
socket.onopen = () => {
  socket.send(JSON.stringify({ 
      type: "register", 
      name: senderName 
  }));
  console.log("Registered to the server");
};

// Handle incoming signaling messages.
socket.onmessage = async (event) => {
  const data = JSON.parse(event.data);

  if (data.type === "answer") {
    // Use the camera property to determine which connection to update.
    const camera = data.camera;
    if (peerConnections[camera]) {
      const answer = { type: "answer", sdp: data.sdp };
      console.log(`Answer received for camera: ${camera}`);
      await peerConnections[camera].setRemoteDescription(new RTCSessionDescription(answer));
      console.log(`Remote description set for camera: ${camera}`);
    } else {
      console.warn("No peer connection found for camera", camera);
    }
  } 
  else if (data.type === "candidate") {
    const camera = data.camera;
    console.log(`ICE candidate received for camera ${camera}:`, data.candidate);
    if (peerConnections[camera]) {
      await peerConnections[camera].addIceCandidate(new RTCIceCandidate(data.candidate));
      console.log(`ICE candidate added for camera ${camera}.`);
    }
  } 
  else if(data.type === "new_receiver") {
    console.log("New receiver found. Initiating connections for cameras...");

    // Trigger get use media permissions
    await navigator.mediaDevices.getUserMedia({ video: true }).then(stream => {
      stream.getTracks().forEach(track => track.stop());
    });

    checkForNewDevices();
  }
  else if(data.type === "peerdisconnect") {
    // Close all connections and reset state.
    for (const camera in peerConnections) {
      if (peerConnections[camera]) {
        peerConnections[camera].close();
        peerConnections[camera] = null;
      }
    }
    previousVideoDevices = [];
    cameraMap.forEach((value, key) => {
      cameraMap.set(key, null);
    });
    console.log("Receiver has left the chat. Closed all peer connections.");
  }
  else {
    console.warn("Sender received unknown message type:", data);
  }
};

let previousVideoDevices = [];

// Connect the new connections
function checkForNewDevices() {
  console.log("Previous devices:", previousVideoDevices);
  navigator.mediaDevices.enumerateDevices().then(devices => {
    const currentVideoDevices = devices.filter(device => device.kind === "videoinput");
    console.log("Current video devices:", currentVideoDevices);
    const previousIds = previousVideoDevices.map(device => device.deviceId);
    const newDevices = currentVideoDevices.filter(device => !previousIds.includes(device.deviceId));
    previousVideoDevices = currentVideoDevices;
    return newDevices;
  }).then(newDevices => {
    if (newDevices.length > 0) {
      connectCameras();
      console.log("New device(s) added:", newDevices);
    }
  });
}

// Create a new RTCPeerConnection for each camera as needed and add its stream.
function connectCameras() {
  navigator.mediaDevices.enumerateDevices().then(devices => {
    const videoDevices = devices.filter(device => device.kind === 'videoinput');
    for (const device of videoDevices) {

      // Front Camera
      if (device.deviceId === frontCameraId) {
        if (cameraMap.get('frontCameraTrackId') !== null) {
          console.log("Front camera already connected.");
        } else {
          console.log("Connecting Front Camera");
          peerConnections.front = new RTCPeerConnection();
          setupPeerConnectionEventHandlers(peerConnections.front, 'front');
          addStream('front', device.deviceId, peerConnections.front);
        }
      }

      // Manipulator Camera
      else if (device.deviceId === manipCameraId) {
        if (cameraMap.get('manipCameraTrackId') !== null) {
          console.log("Manip camera already connected.");
        } else {
          console.log("Connecting Manip Camera");
          peerConnections.manip = new RTCPeerConnection();
          setupPeerConnectionEventHandlers(peerConnections.manip, 'manip');
          addStream('manip', device.deviceId, peerConnections.manip);
        }
      }
    }
  });
  console.log("Read all devices connected.");
}

// Setup listener of ICE candidate. Need to set this up for each camera
function setupPeerConnectionEventHandlers(pc, camera) {
  pc.onicecandidate = (event) => {
    if (event.candidate) {
      socket.send(JSON.stringify({
        type: "candidate",
        candidate: event.candidate,
        target: receiverName,
        camera: camera // Include camera identifier in signaling
      }));
    }
  };
}

// Add the camera stream to its peerConnection
function addStream(camera, cameraId, pc) {
  const cameraConstraints = { 
    video: { 
      deviceId: cameraId,
      width: { ideal: 640 }, 
      height: { ideal: 480 }
    }, 
    audio: false 
  };
  
  navigator.mediaDevices.getUserMedia(cameraConstraints).then(stream => {
    const tracks = stream.getTracks();
    const videoTrack = tracks[0];
  
    cameraMap.set(`${camera}CameraTrackId`, videoTrack);
    const sender = pc.addTrack(videoTrack, stream);

    // Update sender parameters if available.
    const parameters = sender.getParameters();
    if (parameters.encodings && parameters.encodings.length > 0) {
      parameters.encodings[0].maxBitrate = 100000; // 0.1 Mbps
      sender.setParameters(parameters);
    }
    console.log("Track added for camera:", camera);
  
    // Handle track end.
    videoTrack.onended = () => {
      console.log(`${camera} camera track ended. The device might have been disconnected.`);
      pc.removeTrack(sender);
      renegotiateOffer(pc, camera);
      cameraMap.set(`${camera}CameraTrackId`, null);
      console.log(cameraMap);
    };

    pc.createOffer().then(offer => {
      return pc.setLocalDescription(offer);
    }).then(() => {
      console.log(`Local description set successfully for camera: ${camera}`);
      socket.send(JSON.stringify({
        type: "offer",
        sdp: pc.localDescription.sdp,
        target: receiverName,
        camera: camera // Include camera identifier for proper association on the receiver side.
      }));
      console.log(`Offer sent successfully for camera: ${camera}`);
    });
  });
}

// Optionally, you can listen for device changes to trigger checkForNewDevices.
// navigator.mediaDevices.ondevicechange = checkForNewDevices;
