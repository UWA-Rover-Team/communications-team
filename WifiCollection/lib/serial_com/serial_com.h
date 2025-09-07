#include <ArduinoJson.h>
#ifndef SERIAL_COM_H
#define SERIAL_COM_H
class scom{
  public:
    void comLoop();
    // JsonDocument doc;
    void setdoc(JsonDocument document);
    void setpair(String key, String value);
    JsonArray setarr(String key);
    void clear();
    bool isconnected();
    scom();
    void send(String topic, String type);
    // virtual void connectedloop();
    bool doesloop=false;
  private:
    // String tpc;
    // String typ;
    JsonDocument doc;
    
    void talkloop();
    // void sendloop();
    void initloop();
};
#endif