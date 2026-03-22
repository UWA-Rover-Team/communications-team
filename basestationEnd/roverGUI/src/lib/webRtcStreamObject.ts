
export type clients = "BASE_STATION" | "ROVER" | "SERVER" | "ROVER_LOGS";

export type clientError = "DISCONNECT"

export enum cameras {
    FRONT = 1,
    BACK = 2,
    LEFT = 3,
    RIGHT = 4,
    MANIP = 5
}
export type resolution = [width: number, height: number];

export interface WebRTCMessage {
    client: clients;
    type: "REGISTER" | "OFFER" | "ANSWER" | "ICE_CANDIDATE" | "CAMERA_REQUEST" | "ERROR" | "DATAPACKET";
    target: clients;
    sdp?: string;
    candidate?: RTCIceCandidate;
    camera?: cameras;
    resolution?: resolution;
    error?: clientError;
}

export interface dataPacket {
    type: "DATAPACKET"
    target: "BASE_STATION"
}