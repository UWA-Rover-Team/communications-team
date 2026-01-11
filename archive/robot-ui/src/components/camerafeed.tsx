import { useEffect, useRef, useState } from 'react';

function RequestCameraFeed() {
  const videoRef = useRef<HTMLVideoElement>(null);
  const peerConnectionRef = useRef<RTCPeerConnection | null>(null);
  const wsRef = useRef<WebSocket | null>(null);
  const [connected, setConnected] = useState(false);

  useEffect(() => {
    // Connect to your WebSocket server
    const ws = new WebSocket('ws://192.168.56.1:8080'); // Your WebSocket URL
    wsRef.current = ws;

    ws.onopen = () => {
      console.log('WebSocket connected');
      ws.send(JSON.stringify({ type: 'request-stream', camera: 'front' }));
    };

    ws.onmessage = async (event) => {
      const message = JSON.parse(event.data);
      
      if (!peerConnectionRef.current) {
        // Initialize peer connection on first message
        const pc = new RTCPeerConnection({
          iceServers: [{ urls: 'stun:stun.l.google.com:19302' }]
        });

        pc.ontrack = (event) => {
          if (videoRef.current && event.streams[0]) {
            videoRef.current.srcObject = event.streams[0];
            setConnected(true);
          }
        };

        pc.onicecandidate = (event) => {
          if (event.candidate) {
            ws.send(JSON.stringify({ 
              type: 'ice-candidate', 
              candidate: event.candidate 
            }));
          }
        };

        peerConnectionRef.current = pc;
      }

      const pc = peerConnectionRef.current;

      // Handle signaling messages
      if (message.type === 'offer') {
        await pc.setRemoteDescription(new RTCSessionDescription(message.offer));
        const answer = await pc.createAnswer();
        await pc.setLocalDescription(answer);
        ws.send(JSON.stringify({ type: 'answer', answer }));
      } 
      else if (message.type === 'ice-candidate') {
        await pc.addIceCandidate(new RTCIceCandidate(message.candidate));
      }
    };

    return () => {
      ws.close();
      peerConnectionRef.current?.close();
    };
  }, []);

  return (
    <div>
      <h2>Rover Camera {connected && '🟢'}</h2>
      <video 
        ref={videoRef} 
        autoPlay 
        playsInline 
        style={{ 
          width: '100%', 
          maxWidth: '800px',
          border: connected ? '2px solid green' : '2px solid gray'
        }}
      />
    </div>
  );
}

export default RequestCameraFeed;