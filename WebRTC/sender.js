// sender.js

// deviceID of cameras
const frontCameraId = "LQXeVP21cCGt44HPH73pUvFC7Gc8ld1b8Zi136vnzzQ=";

const localVideo = document.getElementById("localVideo");
const socket = new WebSocket("ws://192.168.2.173:8080");
const senderName = "sender";
const receiverName = "receiver";
const streamMapping = new Map();

let peerConnection = new RTCPeerConnection();
connectCameras(peerConnection);

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
    await peerConnection.setRemoteDescription(new RTCSessionDescription(answer));
    console.log("Remote description answer set successfully.");
  } 
  
  else if (data.type === "candidate") {
    console.log("ICE candidate received from receiver:", data.candidate);
    await peerConnection.addIceCandidate(new RTCIceCandidate(data.candidate));
    console.log("ICE candidate added successfully.");
  } 
  
  else if(data.type === "new_receiver") {
    peerConnection = new RTCPeerConnection();
    connectCameras(peerConnection);
  }

  else if(data.type === "peerdisconnect") {
    peerConnection.close()
    document.querySelectorAll('video').forEach(video => video.remove());
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

  // Loop through all found video inputs and send them over their own track
  for (const [index, device] of videoDevices.entries()) {

    if (device.deviceId === "LQXeVP21cCGt44HPH73pUvFC7Gc8ld1b8Zi136vnzzQ=") {
      console.log("Front camera has connected");
      createOffer(pc);
    }
  }
}

async function createOffer(pc) {
  console.log("creating offer...");
  const offer = await pc.createOffer();
  await pc.setLocalDescription(offer);
  console.log(socket.readyState);
  socket.send(
    /*
    JSON.stringify({
      type: "offer",
      offer: pc.localDescription,
      target: receiverName,
    })
    */
   "hello"
  );
  console.log("offer sent success");

  return pc;
}

peerConnection.onicecandidate = (event) => {
  if (event.candidate) {
    socket.send(JSON.stringify({
      type: "candidate",
      candidate: event.candidate,
      target: receiverName
    }));
  }
};






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



    



