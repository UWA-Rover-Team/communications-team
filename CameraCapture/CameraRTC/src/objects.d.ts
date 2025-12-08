export type clients = "BASE_STATION" | "ROVER" | "SERVER";

export interface WebRTCMessage {
    client: clients;
    type: "REGISTER" | "OFFER" | "ANSWER" | "ICE_CANDIDATE";
    target: clients;
    sdp?: string;
    camera?: "FRONT" | "LEFT" | "RIGHT" | "BACK" | "MANIPULATOR";
}