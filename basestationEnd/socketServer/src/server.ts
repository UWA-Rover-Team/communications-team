import WebSocket from 'ws';
import { WebRTCMessage, clients } from './webRtcStreamObject';

const wss = new WebSocket.Server({ host: "0.0.0.0", port: 8080 });
const clientlist: { [key in clients]: WebSocket | null } = {
    BASE_STATION: null,
    ROVER: null,
    SERVER: null
};

console.log("Server Started");

wss.on("connection", (ws: WebSocket) => {
    let registeredClientId: clients | null = null;
    ws.on("message", (message: WebSocket.Data) => {
        const data: WebRTCMessage = JSON.parse(message.toString());

        if (data.type === "REGISTER") {
            clientlist[data.client] = ws;
            registeredClientId = data.client;
            console.log(`Client registered as ${data.client}`);
        }
        else if (clientlist[data.target]) {
            clientlist[data.target]?.send(message.toString());
        }
    });

    ws.on('error', (err: NodeJS.ErrnoException) => {
        if (err.code === 'ECONNRESET') {
            console.log(`Client ${registeredClientId ?? 'unknown'} disconnected abruptly`);
            if (registeredClientId) {
                clientlist[registeredClientId] = null;
            }

            const registerMsg: WebRTCMessage = {
                type: "ERROR",
                client: "BASE_STATION",  
                target: registeredClientId === "ROVER" ? "BASE_STATION" : "ROVER", // if rover, go base station, else rover
                error: "DISCONNECT"
            };
        }
    });
});


