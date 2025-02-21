const WebSocket = require("ws");

const wss = new WebSocket.Server({ host: "0.0.0.0", port: 8080 });
let clients = {};

console.log("WebSocket server running on ws://0.0.0.0:8080");

wss.on("connection", (ws) => {
  ws.on("message", (message) => {
    const data = JSON.parse(message);

    if (data.type === "offer" || data.type === "answer") {
      if (clients[data.target]) {
        // Forward sdp and camera for offer/answer messages
        clients[data.target].send(JSON.stringify({
          type: data.type,
          sdp: data.sdp,       
          camera: data.camera  // include the camera identifier
        }));
        console.log(`Forwarded ${data.type} for camera ${data.camera} to ${data.target}`);
      } else {
        console.log(`Target ${data.target} not found.`);
      }
    } 

    else if (data.type === "nextCamera") {
      // No need to re-parse the message here
      if (clients[data.target]) {
        clients[data.target].send(JSON.stringify({
          type: data.type,
          camera: data.camera
        }));
      }
      console.log("Sent next camera to receiver:", data.camera);
    }

    else if (data.type === "candidate") {
      if (clients[data.target]) {
        // Forward candidate along with camera property
        clients[data.target].send(JSON.stringify({
          type: "candidate",
          candidate: data.candidate,
          camera: data.camera
        }));
      }
    } 

    else if (data.type === "register") {
      ws.clientName = data.name; 
      clients[data.name] = ws;
      console.log(`Client registered as ${data.name}`);
      
      if ((data.name === "receiver" && clients["sender_main"] && clients["sender_periph"]) ||
          (data.name === "sender_main" && clients["sender_periph"] && clients["receiver"])) {
        clients["sender_main"].send(JSON.stringify({ type: "new_receiver" }));
        clients["sender_periph"].send(JSON.stringify({ type: "new_receiver" }));
        console.log("Notified sender about new receiver connection.");
      }
    }
  });

  ws.on("close", () => {
    console.log("Client Disconnected:", ws.clientName);
    const clientMapping = {
      "receiver": "sender",
      "sender": "receiver"
    };
    const otherClient = clientMapping[ws.clientName];
    if (otherClient && clients[otherClient]) {
      clients[otherClient].send(JSON.stringify({
        type: "peerdisconnect"
      }));
      console.log(`Sent disconnect message to ${otherClient}`);
    }
  });

  ws.on("error", (error) => {
    console.error("WebSocket error:", error);
  });
});
