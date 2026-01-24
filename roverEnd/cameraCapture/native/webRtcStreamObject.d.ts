export type clients = "BASE_STATION" | "ROVER" | "SERVER";
export type cameras = "FRONT" | "BACK" | "LEFT" | "RIGHT" | "MANIP"
export type resolution = [width: number, height: number];

export interface WebRTCMessage {
    client: clients;
    type: "REGISTER" | "OFFER" | "ANSWER" | "ICE_CANDIDATE" | "CAMERA_REQUEST";
    target: clients;
    sdp?: string;
    candidate?: RTCIceCandidate;
    camera?: cameras;
    resolution?: resolution;
}