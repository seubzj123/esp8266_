#include <ESP8266WiFi.h>   //安装esp8266arduino开发环境
#include <PubSubClient.h>  //安装PubSubClient库
#include <Wire.h>
#include <ArduinoJson.h>   //json  V5版本
#include <Adafruit_CCS811.h>//这个库文件arduino库管理直接安装
#include "ClosedCube_HDC1080.h"
#include "aliyun_mqtt.h"
//需要安装crypto库

#define LED     D4
#define humidifier D6
#define fan     D7
#define WIFI_SSID        "vivo X23"//替换自己的WIFI
#define WIFI_PASSWD      "b123456789"//替换自己的WIFI

#define PRODUCT_KEY      "a1lAcM3V1ti" //替换自己的PRODUCT_KEY
#define DEVICE_NAME      "home" //替换自己的DEVICE_NAME
#define DEVICE_SECRET    "Zj8EqNmhezeAI0KccfdHVkQhv8rnJUaq"//替换自己的DEVICE_SECRET

#define DEV_VERSION       "S-TH-WIFI-v1.0-20190220"        //固件版本信息

#define ALINK_BODY_FORMAT         "{\"id\":\"123\",\"version\":\"1.0\",\"method\":\"%s\",\"params\":%s}"
#define ALINK_TOPIC_PROP_POST     "/sys/" PRODUCT_KEY "/" DEVICE_NAME "/thing/event/property/post"
#define ALINK_TOPIC_PROP_POSTRSP  "/sys/" PRODUCT_KEY "/" DEVICE_NAME "/thing/event/property/post_reply"
#define ALINK_TOPIC_PROP_SET      "/sys/" PRODUCT_KEY "/" DEVICE_NAME "/thing/service/property/set"
#define ALINK_METHOD_PROP_POST    "thing.event.property.post"
#define ALINK_TOPIC_DEV_INFO      "/ota/device/inform/" PRODUCT_KEY "/" DEVICE_NAME ""    
#define ALINK_VERSION_FROMA      "{\"id\": 123,\"params\": {\"version\": \"%s\"}}"
unsigned long lastMs = 0;
char ab;
int hum_con = 0;//加湿器状态
int fan_con = 0;//风扇状态
double Hum = 30.50;
double Temp = 25.00;
int CO2 = 400;
int TVOC = 0;

WiFiClient   espClient;
PubSubClient mqttClient(espClient);
Adafruit_CCS811 ccs;
ClosedCube_HDC1080 hdc1080;

void init_wifi(const char *ssid, const char *password)      //连接WiFi
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi does not connect, try again ...");
        delay(500);
    }

    Serial.println("Wifi is connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void mqtt_callback(char *topic, byte *payload, unsigned int length) //mqtt回调函数
{
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    payload[length] = '\0';
    Serial.println((char *)payload);
   // https://arduinojson.org/v5/assistant/  json数据解析网站

    
    Serial.println("   ");
    Serial.println((char *)payload);
    Serial.println("   ");
    if (strstr(topic, ALINK_TOPIC_PROP_SET))
    {
        StaticJsonBuffer<100> jsonBuffer;
        JsonObject &root = jsonBuffer.parseObject(payload);
        int params_fan = root["params"]["fan"];
        if(params_fan==0)
        {Serial.println("fan off");
        digitalWrite(fan, LOW); 
        fan_con=0;
        }
        else
        {Serial.println("fan on");
        digitalWrite(fan, HIGH); 
        fan_con=1;}
        int params_hum = root["params"]["humidifier"];
        if(params_hum==0)
        {Serial.println("humidifier off");
        digitalWrite(humidifier, LOW); 
        hum_con=0;
        }
        else
        {Serial.println("humidifier on");
        digitalWrite(humidifier, HIGH); 
        hum_con=1;}
        if (!root.success())
        {
            Serial.println("parseObject() failed");
            return;
        }
    }
}
void mqtt_version_post()
{
    char param[512];
    char jsonBuf[1024];

    //sprintf(param, "{\"MotionAlarmState\":%d}", digitalRead(13));
    sprintf(param, "{\"id\": 123,\"params\": {\"version\": \"%s\"}}", DEV_VERSION);
    //sprintf(jsonBuf, ALINK_BODY_FORMAT, ALINK_METHOD_PROP_POST, param);
    Serial.println(param);
    mqttClient.publish(ALINK_TOPIC_DEV_INFO, param);
}
void mqtt_check_connect()
{
    while (!mqttClient.connected())//
    {
        while (connect_aliyun_mqtt(mqttClient, PRODUCT_KEY, DEVICE_NAME, DEVICE_SECRET))
        {
            Serial.println("MQTT connect succeed!");
            //client.subscribe(ALINK_TOPIC_PROP_POSTRSP);
            mqttClient.subscribe(ALINK_TOPIC_PROP_SET);
            
            Serial.println("subscribe done");
            mqtt_version_post();
        }
    }
    
}

void mqtt_interval_post()
{
    char param[512];
    char jsonBuf[1024];

    //sprintf(param, "{\"MotionAlarmState\":%d}", digitalRead(13));
    sprintf(param, "{\"fancon\":%d,\"fan\":%d,\"humcon\":%d,\"humidifier\":%d,\"CurrentTemperature\":%.2f,\"TVOC\":%d,\"co2\":%d,\"CurrentHumidity\":%.2f,\"vision\":%d}",digitalRead(fan),digitalRead(fan),digitalRead(humidifier),digitalRead(humidifier),Temp,TVOC,CO2,Hum,random(0,5));
    sprintf(jsonBuf, ALINK_BODY_FORMAT, ALINK_METHOD_PROP_POST, param);
    Serial.println(jsonBuf);
    mqttClient.publish(ALINK_TOPIC_PROP_POST, jsonBuf);
}


void setup()
{
    pinMode(LED, OUTPUT);
    pinMode(fan, OUTPUT);
    pinMode(humidifier, OUTPUT);
    digitalWrite(humidifier, LOW); 
    digitalWrite(fan, LOW); 
    /* initialize serial for debugging */
    Serial.begin(115200);

    Serial.println("Demo Start");

    init_wifi(WIFI_SSID, WIFI_PASSWD);

    mqttClient.setCallback(mqtt_callback);

    hdc1080.begin(0x40);

    if(!ccs.begin())
       {
          Serial.println("Couldn't find sensor!");
          while (1);
       }
    while(!ccs.available());
}

// the loop function runs over and over again forever
void loop()
{
    if (millis() - lastMs >= 5000)  //5S
    {
        lastMs = millis();
        mqtt_check_connect();
        /* Post */        
        mqtt_interval_post();
    }
    if(ccs.available())
    {
      if(!ccs.readData())
      {
        Hum = hdc1080.readHumidity();
        Temp = hdc1080.readTemperature();
        CO2 = ccs.geteCO2();
        TVOC = ccs.getTVOC();
      }
    }
    
    mqttClient.loop();

   
}
