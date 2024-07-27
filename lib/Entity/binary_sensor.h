#include "entity.h"

class BinarySensor: public Entity {
  public:
    bool * value; 
    BinarySensor(std::string name, PubSubClient* client, bool * value):Entity(name, client){this->value = value;}   
    void publishValue();
    void mqttDiscovery(DevInfo* dev_info);
};