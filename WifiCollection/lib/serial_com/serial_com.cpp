#include "serial_com.h"
#include <ArduinoJson.h>
#include <utility>
// class scom{
//   public:
   
//     // JsonDocument doc;
    
//   private:
//     JsonDocument doc;
//     bool doesloop;
//     void talkloop();
//     void sendloop();
//     void initloop();
// };
// scom::scom(){
//   // tpc=topic;
//   // typ=type;
// }
scom::scom() {
    // Constructor implementation
}
void scom::comLoop(){
      if (doesloop){
        // talkloop(); 
        // sendloop();
        // connectedloop();
      }else{
        initloop();
      }        
    }

void scom::setdoc(JsonDocument document){
      doc=std::move(document); 
    }
void scom::setpair(String key, String value){
      doc[key] = value;
    }
JsonArray scom::setarr(String key ){
      JsonArray data = doc["data"].to<JsonArray>();
      return data;
    }
void scom::clear(){
      doc.clear();
    }
void scom::talkloop() {
  
  // Generate the minified JSON and send it to the Serial port.
  if(doc.isNull()){return;}
  serializeJson(doc, Serial);
  Serial.println();
  // not used in this example
  delay(1000);
  doc.clear();
  // Generate the prettified JSON and send it to the Serial port.
  // serializeJsonPretty(doc, Serial);
}

void scom::send(String topic, String type){
  // if(doc.isNull()){return;}
  if(!doesloop){return;}
  scom::setpair("topic", topic);
  scom::setpair("datatype", type);
  serializeJson(doc, Serial);
  Serial.println();
  // not used in this example
  // delay(1000);
  doc.clear();
}
bool scom::isconnected(){
  return doesloop;
}
void scom::initloop(){
  Serial.println("start");
  delay(1000);
  // send data only when you receive data:
  if (Serial.available() > 0) {
    // read the incoming byte:
    String name = Serial.readStringUntil('\n');
    if(name=="GO"){
      doesloop=true;
      Serial.println("starting");
    }

    // // say what you got:
    // Serial.print("Hello, ");
    // Serial.print(name);
    // Serial.println("!");
  }
  delay(1000);
}

// void scom::sendloop(){
//   if (Serial.available() > 0) {
//     // read the incoming byte:
//     String name = Serial.readStringUntil('\n');

//     // // say what you got:
//     Serial.print("sending ");
//     Serial.print(name);
//     Serial.println();
//   }
// }

