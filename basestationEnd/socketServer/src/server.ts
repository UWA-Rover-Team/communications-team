import WebSocket from 'ws';
import os from 'os';
import { WebRTCMessage, clients, dataPacket } from './webRtcStreamObject';

const wss = new WebSocket.Server({ host: "0.0.0.0", port: 8080 });

wss.on('listening', () => {
  const addr = wss.address();
  if (addr && typeof addr === 'object') {
    const localIP = Object.values(os.networkInterfaces())
      .flat()
      .find(n => n?.family === 'IPv4' && !n.internal)?.address;

    console.log(`WebSocket server listening on ws://${localIP}:${addr.port}`);
  }
});

const clientlist: { [key in clients]: WebSocket | null } = {
    BASE_STATION: null,
    ROVER: null,
    SERVER: null,
    ROVER_LOGS: null
};

console.log("Server Started");

wss.on("connection", (ws: WebSocket) => {
    let registeredClientId: clients | null = null;
    ws.on("message", (message: WebSocket.Data) => {
        const data: WebRTCMessage | dataPacket = JSON.parse(message.toString());

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


