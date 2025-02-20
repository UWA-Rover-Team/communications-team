// WebSocket Server. Need to keep active for ICE candidate optimisation and reconnection

const WebSocket = require("ws");

const wss = new WebSocket.Server({ host: "0.0.0.0", port: 8080 });
let clients = {};

console.log("WebSocket server running on ws://0.0.0.0:8080");

wss.on("connection", (ws) => {

    ws.on("message", (message) => {
        const data = JSON.parse(message);
        if (data.type === "offer" || data.type === "answer") {
            if (clients[data.target]) {
                clients[data.target].send(JSON.stringify({
                    type: data.type,
                    sdp: data[data.offer]
                }));
                console.log(`Forwarded ${data.type} to ${data.target}`);
            } 
            else {
                console.log(`Target ${data.target} not found.`);
            }
        } 

        else if (data.type === "nextCamera"){
            const data = JSON.parse(message);
            if (clients[data.target]) {
                clients[data.target].send(JSON.stringify({
                    type: data.type,
                    camera: data.camera
                }));
            }
            console.log("sent next camera to receiver");
        }

        else if (data.type === "candidate") {
            if (clients[data.target]) {
                clients[data.target].send(JSON.stringify({
                    type: "candidate",
                    candidate: data.candidate
                }));
            }
        } 
        else if (data.type === "register") {
            ws.clientName = data.name; 
            clients[data.name] = ws;
            console.log(`Client registered as ${data.name}`);
            
            if ((data.name === "receiver" && clients["sender"]) || (data.name === "sender" && clients["receiver"])) {
            	clients["sender"].send(JSON.stringify({ type: "new_receiver" }));
                console.log("Notified sender about new receiver connection.");
            
            }
        }
    });

    ws.on("close", () => {
        console.log("Client Disconnected:", ws.clientName);

        // Dictionary of where to send it to
        const clientMapping = {
            "receiver": "sender",
            "sender": "receiver"
        };

        const otherClient = clientMapping[ws.clientName];

        // Check if both clients exist
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