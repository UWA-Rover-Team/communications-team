import WebSocket from 'ws';
import { WebRTCMessage, clients } from './objects';

const socket = new WebSocket("ws://192.168.56.1:8080");

socket.onopen = () => {
  const registerMsg: WebRTCMessage = {
    type: "REGISTER",
    client: "ROVER", 
    target: "SERVER"
  };
  
  socket.send(JSON.stringify(registerMsg));
  console.log("Registered to the server");
};

socket.onmessage = () => {
  
};