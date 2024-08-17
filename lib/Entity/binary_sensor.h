#include "entity.h"

class BinarySensor: public Entity {
  public:
    bool * value; 
    BinarySensor(std::string name, PubSubClient* client, bool * value, bool * retain):Entity(name, client, retain){this->value = value;}   
    void publishValue();
    void mqttDiscovery(DevInfo* dev_info);
};