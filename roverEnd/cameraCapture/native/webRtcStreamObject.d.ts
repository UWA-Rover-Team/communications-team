export type clients = "BASE_STATION" | "ROVER" | "SERVER";
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
    type: "REGISTER" | "OFFER" | "ANSWER" | "ICE_CANDIDATE" | "CAMERA_REQUEST";
    target: clients;
    sdp?: string;
    candidate?: RTCIceCandidate;
    camera?: cameras;
    resolution?: resolution;
}