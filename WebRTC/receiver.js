// receiver.js

const remoteVideo = document.getElementById("remoteVideo");
const socket = new WebSocket("ws://192.168.2.173:8080"); // Replace with correct WebSocket server IP
const receiverName = "receiver";
const senderName = "sender";
let cameraCount = 0;
const receivedTracks = []; // Global array
let peerConnection = new RTCPeerConnection();

console.log("receiver.js has started");

// Register as receiver
socket.onopen = () => {
	socket.send(JSON.stringify({ 
		type: "register", 
		name: receiverName 
	}));
}


socket.onmessage = async (event) => {
  const data = JSON.parse(event.data);

  if (data.type === "offer") {
    console.log("Received a new offer");
    await acceptPeerConnection();
  
    // Reconstruct the offer object expected by setRemoteDescription
    const offer = { type: "offer", sdp: data.sdp };
    await peerConnection.setRemoteDescription(new RTCSessionDescription(offer));
    console.log("Remote description set successfully.");
  
    // Create an answer to the offer
    const answer = await peerConnection.createAnswer();
    await peerConnection.setLocalDescription(answer);
    console.log("Local description set successfully.");
    
    // Send the answer to the sender after a brief delay (allowing ICE gathering)
    socket.send(JSON.stringify({
       type: "answer",
       answer: peerConnection.localDescription,
       target: senderName
    }));
    console.log("Answer sent successfully");
  } 
  
  else if (data.type === "candidate") {
    await peerConnection.addIceCandidate(new RTCIceCandidate(data.candidate));
  }

  else if(data.type === "peerdisconnect") {
    peerConnection.close()
    document.querySelectorAll('video').forEach(video => video.remove());
    receivedTracks.forEach(track => track.stop());
    console.log("Sender has left the chat. Closing peer connection and closing tracks");
  }
  
   else {
    console.warn("Receiver received unknown message type:", data);
  }
};



async function acceptPeerConnection() {

  // Open new RTCPeerConnection if needed
  if (!peerConnection || 
    peerConnection.connectionState === 'closed' || 
    peerConnection.connectionState === 'failed') {
    peerConnection = new RTCPeerConnection();
    console.log("Opening new RTC Session");
  }

  peerConnection.onicecandidate = (event) => {
    if (event.candidate) {
      socket.send(JSON.stringify({
        type: "candidate",
        candidate: event.candidate,
        target: senderName
      }));
    }
  };

  peerConnection.ontrack = (event) => {
    console.log("New track has been detected, id:", event.stream[0].id);
    if (event.track.kind === 'video') {
      receivedTracks.push(event.track);
      const newVideo = document.createElement('video');
      newVideo.id = `camera${++cameraCount}`;
      newVideo.autoplay = true;
      newVideo.playsInline = true;
      newVideo.srcObject = new MediaStream([event.track]);
      document.body.appendChild(newVideo);
      console.log("New video stream has started");
    }
  };  
}


// Object to store previous stats for calculation
const lastStats = {};
const getStats = false;

// Run every 1 second to measure inbound video bandwidth and latency
setInterval(() => {
  if (getStats) {
    peerConnection.getStats().then(stats => {
      stats.forEach(report => {
        // Process inbound video bitrate
        if (report.type === 'inbound-rtp' && report.kind === 'video') {
          if (lastStats[report.id]) {
            const bytesReceivedDiff = report.bytesReceived - lastStats[report.id].bytesReceived;
            const bitrate = (bytesReceivedDiff * 8) / 1000000; // Converts to mbps
            console.log(`Video bitrate: ${bitrate.toFixed(3)} mbps`);
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
            console.log(`Current round trip time: ${rtt * 1000} ms`);
          } else {
            console.log('RTT not available yet.');
          }
        }
      });
    });
  }
}, 1000);
