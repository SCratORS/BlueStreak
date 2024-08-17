#include "entity.h"
#include <map>

class Select: public Entity {
  public:
    uint8_t * value; 
    std::map<uint8_t, std::string> items_list;
    Select(std::string name, PubSubClient* client, uint8_t * value, bool * retain):Entity(name, client, retain){this->value = value;}   
    ~Select();
    void publishValue();
    std::string getItemsString();
    void setItem();
    void callback(std::string message);
    void mqttDiscovery(DevInfo* dev_info);
};