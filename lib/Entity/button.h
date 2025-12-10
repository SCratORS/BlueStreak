#include "entity.h"

class Button: public Entity {
  public:
    Button(std::string name, PubSubClient* client, bool * value, bool * retain):Entity(name, client, retain){}   
    void mqttDiscovery(DevInfo* dev_info);
};