#include <SoftwareSerial.h>
#include <WiFi.h>
#include <DHTesp.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

#define RX_PIN 22
#define TX_PIN 23

SoftwareSerial espSerial(RX_PIN, TX_PIN);

//----Chỗ này để cài đặt wifi, broker---------------
const char* ssid = "Computer";      //Wifi connect
const char* password = "123789456";   //Password

const char* mqtt_server = "cd80185684ab4cee86ac167bdf8f629a.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_username = "Project1"; //User
const char* mqtt_password = "Project1"; //Password
//--------------------------------------------------

// Khúc này để kết nối với wifi
WiFiClientSecure espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];

// các biến cần sử dụng trong bài
bool updateState=0;
int outSignal;
int hum_land;
int humidity;
int temp;
float h, t, val;
int mode, motor, humLandData;
String modecontrol = "Auto";
String motorcontrol = "Off";
int hValue, tValue, valValue;
unsigned long timeUpdate = 0; 
void receiveData();

void setup_wifi() {
  /// setup wifi
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  randomSeed(micros());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}
//------------Kết nối tới MQTT Broker-----------------------------
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientID =  "ESPClient-";
    clientID += String(random(0xffff),HEX);
    if (client.connect(clientID.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("connected");
      client.subscribe("esp32/client");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}
//-----Call Back để nhận tin nhắn từ MQTT Broker và gửi đến Arduino thông qua UART---------
void callback(char* topic, byte* payload, unsigned int length) {
  String incomingMessage = "";
  for(int i = 0; i < length; i++)
    incomingMessage += (char)payload[i];
  
  Serial.println("Message arrived [" + String(topic) + "]: " + incomingMessage);

  DynamicJsonDocument doc(100);
  DeserializationError error = deserializeJson(doc, incomingMessage);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }

  JsonObject obj = doc.as<JsonObject>();

  if(obj.containsKey("out")){
     outSignal = obj["out"];
     espSerial.print("Autoo_Signal:");
     espSerial.println(outSignal);
  }
  if(obj.containsKey("Motor")){
     outSignal = obj["Motor"];
     espSerial.print("Motor_Signal:");
     espSerial.println(outSignal);
  }
  if(obj.containsKey("hum_land")){
     hum_land = obj["hum_land"];
      espSerial.print("hum:");
     espSerial.println(hum_land);
  }
  if(obj.containsKey("temp")){
     temp = obj["temp"];
     espSerial.print("temp:");
     espSerial.println(temp);
  }
  if(obj.containsKey("hum")){
     humidity = obj["hum"];
     espSerial.print("humidity:");
     espSerial.println(humidity);
  }

  updateState = 1;
}
//----------publish lên MQTT------------
void publishMessage(const char* topic, String payload, boolean retained){
  if(client.publish(topic,payload.c_str(),true))
    Serial.println("Message published ["+String(topic)+"]: "+payload);
}

void setup() {
  Serial.begin(9600);
  while(!Serial) delay(1);

  // set up cái MQTT broker với wifi các kiểu
  setup_wifi();
  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  espSerial.begin(9600);
  
}
unsigned long timeUpdata=millis();

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  receiveData(); // Gọi hàm nhận dữ liệu
  if (mode == 1)
  {
    modecontrol = "No-Auto";
  }
  else if(mode == 2)
  {
    modecontrol = "Auto";
  }
  if(motor == 1)
  {
     motorcontrol = "Off";
  }
  else if(motor == 2)
  {
    motorcontrol = "On";
  }
  // Gửi tín hiệu đến MQTT broker
  if (millis() - timeUpdate > 850) {
    DynamicJsonDocument doc(1024);
    doc["value"] = val;
    doc["mode"] = modecontrol;         
    doc["motor"] = motorcontrol;
    doc["hum_land_threshold"] = hum_land;
    doc["temp"] = t;
    doc["hum"] = h;
    doc["hum_threshold"] = humidity;
    doc["temp_theshold"] = temp;
    char mqtt_message[256];
    serializeJson(doc, mqtt_message);
    publishMessage("esp32/data", mqtt_message, true);
    timeUpdate = millis();
  }
}
void receiveData() {
  if (espSerial.available() > 0) {
    String dataReceived = espSerial.readStringUntil('\n');
    
    // Tìm vị trí của dấu phẩy
    int separatorIndices[7]; // Tăng số phần tử của mảng separatorIndices lên 7 để phù hợp với số lượng dấu phẩy trong chuỗi dữ liệu nhận được
    int separatorCount = 0;
    int lastIndex = -1;
    for (int i = 0; i < dataReceived.length(); i++) {
      if (dataReceived[i] == ',') {
        separatorIndices[separatorCount++] = i;
        lastIndex = i;
      }
    }

    // Kiểm tra nếu có đủ dấu phẩy
    if (separatorCount == 7) { // Bạn có 8 giá trị: val, controlMode, controlbutton, hum_land, h, t, temp, humidity
      // Lấy các giá trị từ chuỗi
      val = dataReceived.substring(0, separatorIndices[0]).toFloat();
      mode = dataReceived.substring(separatorIndices[0] + 1, separatorIndices[1]).toInt();
      motor = dataReceived.substring(separatorIndices[1] + 1, separatorIndices[2]).toInt();
      hum_land = dataReceived.substring(separatorIndices[2] + 1, separatorIndices[3]).toFloat();
      h = dataReceived.substring(separatorIndices[3] + 1, separatorIndices[4]).toFloat();
      t = dataReceived.substring(separatorIndices[4] + 1, separatorIndices[5]).toFloat();
      temp = dataReceived.substring(separatorIndices[5] + 1, separatorIndices[6]).toFloat();
      humidity = dataReceived.substring(separatorIndices[6] + 1).toFloat();
    }
  }
}






