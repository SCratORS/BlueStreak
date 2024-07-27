#include "entity.h"

class Sensor: public Entity {
  public:
    std::string * value; 
    Sensor(std::string name, PubSubClient* client, std::string * value):Entity(name, client){ this->value = value;}   
    void publishValue();
    void mqttDiscovery(DevInfo* dev_info);
};