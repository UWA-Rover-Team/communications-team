// receiver.js
// This file now creates and manages a separate RTCPeerConnection per camera.

const videoElements = {
  front: document.getElementById("video-front"),
  left: document.getElementById("video-left"),
  right: document.getElementById("video-right"),
  manip: document.getElementById("video-manip")
};

const socket = new WebSocket("ws://192.168.2.173:8080"); // Replace with correct WebSocket server IP
const receiverName = "receiver";
const senderName = "sender";

// Object to store a peer connection per camera
const peerConnections = {
  front: null,
  left: null,
  right: null,
  manip: null,
};

// Global array to store received tracks (if needed)
const receivedTracks = [];

console.log("receiver.js has started");

// Register as receiver
socket.onopen = () => {
  socket.send(JSON.stringify({ 
    type: "register", 
    name: receiverName 
  }));
};

socket.onmessage = async (event) => {
  const data = JSON.parse(event.data);

  if (data.type === "offer") {

    const camera = data.camera;
    console.log(`Received offer for camera: ${camera}`);
    const pc = new RTCPeerConnection();

    // Set up ICE listner
    pc.onicecandidate = (event) => {
      if (event.candidate) {
        socket.send(JSON.stringify({
          type: "candidate",
          candidate: event.candidate,
          target: senderName,
          camera: camera 
        }));
      }
    };

    // Set up track listener
    pc.ontrack = (event) => {
      if (event.track.kind === "video") {
        receivedTracks.push(event.track);
        const videoElem = videoElements[camera];
        if (videoElem) {
          videoElem.srcObject = new MediaStream([event.track]);
          console.log(`${camera} video stream has started`);
        } else {
          console.warn(`No video element found for camera: ${camera}`);
        }
      }
    };

    peerConnections[camera] = pc;
    console.log("SDP data description:", data.sdp);
    const offer = { type: "offer", sdp: data.sdp };
    await pc.setRemoteDescription(new RTCSessionDescription(offer));
    console.log(`Remote description set for camera: ${camera}`);

    // Answer the offer
    const answer = await pc.createAnswer();
    await pc.setLocalDescription(answer);
    console.log(`Local description set for camera: ${camera}`);
    socket.send(JSON.stringify({
      type: "answer",
      sdp: pc.localDescription.sdp,
      target: senderName,
      camera: camera
    }));
    console.log(`Answer sent for camera: ${camera}`);
  } 

  else if (data.type === "candidate") {
    const camera = data.camera;
    if (peerConnections[camera]) {
      await peerConnections[camera].addIceCandidate(new RTCIceCandidate(data.candidate));
      console.log(`ICE candidate added for camera: ${camera}`);
    } else {
      console.warn(`Received candidate for unknown camera: ${camera}`);
    }
  } 

  else if (data.type === "peerdisconnect") {
    // Close all peer connections and stop all received tracks.
    Object.keys(peerConnections).forEach(camera => {
      if (peerConnections[camera]) {
        peerConnections[camera].close();
        peerConnections[camera] = null;
      }
    });
    Object.values(videoElements).forEach(videoElem => {
      if (videoElem.srcObject) {
        videoElem.srcObject.getTracks().forEach(track => track.stop());
      }
    });
    console.log("Sender has disconnected. Closed all peer connections and tracks.");
  } 

  else {
    console.warn("Receiver received unknown message type:", data);
  }
};

// (Optional) Stats gathering logic remains similar; you could extend it to iterate over all connections if needed.
const lastStats = {};
const getStats = false;
setInterval(() => {
  if (getStats) {
    Object.keys(peerConnections).forEach(async (camera) => {
      const pc = peerConnections[camera];
      if (pc) {
        const stats = await pc.getStats();
        stats.forEach(report => {
          // Process inbound video bitrate
          if (report.type === 'inbound-rtp' && report.kind === 'video') {
            if (lastStats[report.id]) {
              const bytesReceivedDiff = report.bytesReceived - lastStats[report.id].bytesReceived;
              const bitrate = (bytesReceivedDiff * 8) / 1000000; // mbps
              console.log(`Camera ${camera} video bitrate: ${bitrate.toFixed(3)} mbps`);
            }
            lastStats[report.id] = {
              bytesReceived: report.bytesReceived,
              timestamp: report.timestamp 
            };
          }
          // Process latency from candidate pair stats
          if (report.type === 'candidate-pair' && (report.selected || report.nominated)) {
            const rtt = report.currentRoundTripTime;
            if (typeof rtt === 'number') {
              console.log(`Camera ${camera} round trip time: ${(rtt * 1000).toFixed(0)} ms`);
            } else {
              console.log(`Camera ${camera} RTT not available yet.`);
            }
          }
        });
      }
    });
  }
}, 1000);
