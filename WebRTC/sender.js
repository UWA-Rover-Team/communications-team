// sender.js

const localVideo = document.getElementById("localVideo");
const peerConnection = new RTCPeerConnection();
const socket = new WebSocket("ws://192.168.0.25:8080");
const senderName = "sender";
const receiverName = "receiver";

// Register sender
socket.onopen = () => {
    socket.send(JSON.stringify({ 
        type: "register", 
        name: senderName 
    }));
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
  
  else {
    console.warn("Sender received unknown message type:", data);
  }
};

// Capture video and create offer
(async () => {

    // List devices and then filter videoinput ones
    const devices = await navigator.mediaDevices.enumerateDevices();
    const videoDevices = devices.filter(device => device.kind === 'videoinput');

    const camera1Id = videoDevices[0].deviceId
    const constraintsCamera1 = { video: { deviceId: camera1Id }, audio: false };
    const stream1 = await navigator.mediaDevices.getUserMedia(constraintsCamera1);
    
    document.getElementById("camera1Video").srcObject = stream1;

    stream1.getTracks().forEach((track) => peerConnection.addTrack(track, stream1));

    // Set max bitrate to 0.1 Mbps (100,000 bits per second)
    const videoSender = peerConnection.getSenders().find(sender => sender.track && sender.track.kind === 'video');
    const parameters = videoSender.getParameters();
    parameters.encodings[0].maxBitrate = 100000;
    await videoSender.setParameters(parameters);

    console.log("creating offer...");
    const offer = await peerConnection.createOffer();
    await peerConnection.setLocalDescription(offer);
    
    socket.send(
      JSON.stringify({
        type: "offer",
        offer: peerConnection.localDescription,
        target: receiverName,
      })
    );
    console.log("offer sent success");
})();


// Handle ICE Candidates
peerConnection.onicecandidate = (event) => {
    if (event.candidate) {
        socket.send(JSON.stringify({
            type: "candidate",
            candidate: event.candidate,
            target: receiverName
        }));
    }
};






    



