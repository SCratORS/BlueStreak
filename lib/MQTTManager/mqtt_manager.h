#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include "entity.h"
#include <map>
#include <list>

class MQTTManager {
    public:
        MQTTManager(std::string server, uint16_t port, std::string login, std::string passwd);
        ~MQTTManager();
        void device_discovery();
        void device_online();
        void handle();
        void setClientID(std::string value) {this->client_id = value;}
        void mqtt_loop();
        Entity * getEntity(std::string entity) {return callback_entity[entity];}
        void addEntity(Entity * entity) {entities.push_back(entity);}
        void begin() {mqtt_started = true;}
        PubSubClient * getMQTTClient() {return mqtt_client;}
        DevInfo * device_info;
        uint8_t last_error = 0;
        uint64_t previousMQTTMillis = 0;
    private:
        bool mqtt_started = false;
        std::string client_id;
        WiFiClient * ns_client;
        PubSubClient * mqtt_client;
        std::string server;
        uint16_t port;
        std::string login;
        std::string passwd;
        std::list<Entity*> entities;
        std::map<std::string, Entity*> callback_entity;
        
};
