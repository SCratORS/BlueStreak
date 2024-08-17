#include "entity.h"

class Sensor: public Entity {
  public:
    std::string * value; 
    Sensor(std::string name, PubSubClient* client, std::string * value, bool * retain):Entity(name, client, retain){ this->value = value;}   
    void publishValue();
    void mqttDiscovery(DevInfo* dev_info);
};