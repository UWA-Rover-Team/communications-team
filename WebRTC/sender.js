// sender.js

// deviceID of cameras
const frontCameraId = "LQXeVP21cCGt44HPH73pUvFC7Gc8ld1b8Zi136vnzzQ=";
const leftCameraId = "2noqUGsn6qKXNB9fjiWc4SoBOttpdywjT+jQfNLESGs=";

const localVideo = document.getElementById("localVideo");
const socket = new WebSocket("ws://192.168.2.173:8080");
const senderName = "sender";
const receiverName = "receiver";
let peerConnection = new RTCPeerConnection();
cameraMap = new Map([
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
    await connectCameras(peerConnection);
    renegotiateOffer(peerConnection);
  }

  else if(data.type === "peerdisconnect") {
    peerConnection.close()
    console.log("Receiver has left the chat. Closing peer connection");
  }

  else {
    console.warn("Sender received unknown message type:", data);
  }
};




// Capture video and create offer
async function connectCameras(pc) {
  
  // List devices and then filter videoinput ones
  const devices = await navigator.mediaDevices.enumerateDevices();
  const videoDevices = devices.filter(device => device.kind === 'videoinput');
  console.log("List of video devices:", videoDevices);

  const trackPromises = [];
  // Loop through all found video inputs and send them over their own track
  for (const [index, device] of videoDevices.entries()) {
    if (device.deviceId === frontCameraId) {
      console.log("Front camera has connected, updating offer");
      await addTrack('middle', device.deviceId)
    }

    if (device.deviceId === leftCameraId) {
      console.log("Left camera has connected, updating offer");
      await addTrack('left', device.deviceId);
    }

  }
  console.log("All camera tracks have been added.");
}


function addTrack(camera, cameraId) {
  const cameraConstraints = { video: {deviceId: cameraId,
    width: { ideal: 640 }, 
    height: { ideal: 480 }}, 
    audio: false };
    navigator.mediaDevices.getUserMedia(cameraConstraints).then((stream) => {
      const tracks = stream.getTracks();
      const videoTrack = tracks[0];
      const sender = pc.addTrack(videoTrack, stream);
      console.log(`Sender's ${camera} track ID:`, videoTrack.id);
      // Update sender parameters
      const parameters = sender.getParameters();
      if (!parameters.encodings) parameters.encodings = [{}];
      parameters.encodings[0].maxBitrate = 100000; // 0.1 Mbps
      console.log("Offer updated");
      sender.setParameters(parameters);
    })

    .catch((error) => {
      console.error("Error accessing user media:", error);
    });
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
  console.log("...Offer sent success");

  return pc;
}









/*
    else {
      const cameraConstraints = { video: {deviceId: device.deviceId,
                                          width: { ideal: 640 }, 
                                          height: { ideal: 480 }}, 
                                          audio: false };
      const stream = await navigator.mediaDevices.getUserMedia(cameraConstraints);

      // Add each track to the peerConnection
      for (const track of stream.getTracks()) {
        const sender = pc.addTrack(track, stream);

        // Double checks that each track is a video track
        if (track.kind === 'video') {
        const parameters = sender.getParameters();
        parameters.encodings[0].maxBitrate = 100000; // 0.1 Mbps
        await sender.setParameters(parameters);
        }
      }

      // Add each track to the html stream
      const videoElement = document.getElementById('camera${index + 1}Video');

      // Optionally create a video element if it doesn't already exist
      const newVideo = document.createElement('video');
      newVideo.id = `camera${index + 1}Video`;
      newVideo.autoplay = true;
      newVideo.playsInline = true;
      newVideo.srcObject = stream;
      document.body.appendChild(newVideo);
    }
    */



    



