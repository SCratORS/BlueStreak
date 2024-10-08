#include "entity.h"

class Switch: public Entity {
  public:
    bool * value; 
    Switch(std::string name, PubSubClient* client, bool * value, bool * retain):Entity(name, client, retain){this->value = value;}   
    void publishValue();
    void callback(std::string message);
    void mqttDiscovery(DevInfo* dev_info);
};