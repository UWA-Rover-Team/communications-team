import WebSocket from 'ws';
import { WebRTCMessage, clients } from './webRtcStreamObject';

const wss = new WebSocket.Server({ host: "0.0.0.0", port: 8080 });
const clientlist: { [key: string]: WebSocket } = {};
console.log("Server Started");

wss.on("connection", (ws: WebSocket) => {
    ws.on("message", (message: WebSocket.Data) => {
        const data: WebRTCMessage = JSON.parse(message.toString());

        if (data.type === "REGISTER") {
            clientlist[data.client] = ws;
            console.log(`Client registered as ${data.client}`);
        }
        else if (clientlist[data.target]) {
            clientlist[data.target]?.send(message.toString());
        }
    });
});


