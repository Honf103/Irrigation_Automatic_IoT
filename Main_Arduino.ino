#include <SoftwareSerial.h>
#include <Wire.h> 
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include "OneButton.h" 
#include "HDC1080JS.h"


#define sensorPin A0
#define RX_PIN 12
#define TX_PIN 13

SoftwareSerial espSerial(RX_PIN, TX_PIN); // Khởi tạo đối tượng SoftwareSerial

LiquidCrystal_I2C lcd(0x27,16,2);
uint8_t HDC1080_ADDRESS = 0x40;
HDC1080JS tempsensor;

// Hai chân của nút bấm
int nutnhanmotor = 11;
int nutnhanauto = 7;

OneButton buttonmotor(nutnhanmotor, true); 
OneButton buttonauto(nutnhanauto, true); 

// Khai báo chân cảm biến DHT11

//////////////////////////////////
// Này cho LCD
byte degree[8] = {
  0B01110,
  0B01010,
  0B01110,
  0B00000,
  0B00000,
  0B00000,
  0B00000,
  0B00000
};
// Các biến cần sài trong code
float val = 0;
int controlMode = 2;
int controlbutton = 1;
int previousState = 1;
int temp = 31;
int humidity = 75;
int hum_land = 100;
int outSignal;
int temp_control = 3;
int humidity_control = 3;
int hum_land_control = 3;
int motorOn; 
int motorData,tempData,humidityData,humLandData;
 String incomingData;
bool waitingForSensorData = false; // Biến cờ cho biết liệu bạn đang trong quá trình chờ hay không
unsigned long sensorDataTimeout = 0; 

/// Khai báo hàm cần sài
int readSensor(float val); 
void displayLCD(float h, float t, float val); 
void SendESP32(float h, float t, float val); 
void nhan_don();
void nhan_don_off();
void AutoMotor(float h, float t, float val);

///////////////// Vô set up 
void setup() {

  /// Hiển thị lên LCD
  lcd.init();  
  lcd.backlight();

  lcd.createChar(1, degree);
  /// Khởi tạo cảm biến DHT11
  ///// Khởi tạo cảm biến đất
   pinMode(sensorPin, INPUT);

  // Chân số 8 là chân động cơ relay
  pinMode(8,OUTPUT);
  // Khởi tạo 2 chân cho 2 nút nhấn
  buttonmotor.attachDoubleClick(nhan_double);  //Kích hoạt lệnh khi nhấn liên tục 2 lần       
  buttonmotor.attachClick(nhan_don);
  buttonauto.attachDoubleClick(nhan_double_1);  //Kích hoạt lệnh khi nhấn liên tục 2 lần       
  buttonauto.attachClick(nhan_don_1);

  /// UART
  espSerial.begin(9600); // Khởi tạo SoftwareSerial
  Serial.begin(9600); // Khởi tạo Serial

  Wire.begin();
  Wire.setClock(100000); //set clock speed for I2C bus to maximum allowed for HDC1080
  tempsensor = HDC1080JS();
  tempsensor.config();

}

void loop() {
  tempsensor.readTempHumid();
  int t = tempsensor.getTemp();
  int h = tempsensor.getRelativeHumidity();
  // Thu thập thông tin cảm biến

  val = analogRead(val);
  val = int(val * 100 / 1024);

  // Kiểm tra tín hiệu nhận từ ESP32 
  if (espSerial.available() > 0) {
    incomingData = espSerial.readStringUntil('\n'); // Đọc dữ liệu từ ESP32 
    Serial.println(incomingData);
    if (incomingData.startsWith("Autoo_Signal:")) {
      // Xử lý dữ liệu tín hiệu bơm chìm
      motorData = incomingData.substring(13).toInt(); 
        if(motorData == 2)
        {
          controlMode = 1;
        }
        if (motorData == 1)
        {
          controlMode = 2;
        }
    } else if (incomingData.startsWith("Motor_Signal:")) {
    // Xử lý dữ liệu nhiệt độ
    motorOn = incomingData.substring(13).toInt();
    Serial.print("motoron: "); 
    Serial.println(motorOn);
    if (controlMode == 1) {
        if (motorOn == 2) {
            controlbutton = 2;
            Serial.println("Vo 2");
        }
        else if (motorOn == 1) {
            controlbutton = 1;
            Serial.println("Vo 1");
        }
    }
  }
   else if (incomingData.startsWith("hum:")) {
      // Xử lý dữ liệu độ ẩm đất
      humLandData = incomingData.substring(4).toInt(); 
      if (humLandData >= 1 && humLandData <= 2) {
        hum_land_control = humLandData;
      }
    }
    else if (incomingData.startsWith("temp:")) {
      // Xử lý dữ liệu độ ẩm đất
      tempData = incomingData.substring(5).toInt(); 
      if (tempData >= 1 && tempData <= 2) {
        temp_control = tempData;
      }
    }
    else if (incomingData.startsWith("humidity:")) {
      // Xử lý dữ liệu độ ẩm đất
      humidityData = incomingData.substring(9).toInt(); 
      if (humidityData >= 1 && humidityData <= 2) {
        humidity_control = humidityData;
      }
    }
  }
  // Kiểm tra điều kiện để điều chỉnh các giá trị ngưỡng
   if(hum_land_control == 1)
  {
    hum_land += 1;
    hum_land_control =3 ;
  }
  else if(hum_land_control == 2)
  {
    hum_land = hum_land - 1;
    hum_land_control = 3;
  }
  if(humidity_control == 1)
  {
    humidity += 1;
    humidity_control = 3;
  }
  else if(humidity_control == 2)
  {
    humidity =humidity- 1;
    humidity_control = 3;
  }
  if(temp_control == 1)
  {
    temp += 1;
    temp_control = 3;
  }
  else if(temp_control == 2)
  {
    temp = temp - 1;
    temp_control = 3;
  }
  //Đọc trạng thái button
  int buttonStatusauto = digitalRead(nutnhanauto);
  int buttonStatusmotor = digitalRead(nutnhanmotor);  
  buttonmotor.tick();
  buttonauto.tick();
  /// Kiểm tra điều kiện và điều khiển 
  if(controlMode == 2) //Mode : On
  {
    AutoMotor(h, t, val);
    //Serial.println("Auto");
  }
  else if(controlMode == 1)
  {
   if (controlbutton == 1)
    {
      digitalWrite(8, LOW);
      //Serial.println("No-Auto motor off");
    }
    else if (controlbutton == 2)
   {
      digitalWrite(8, HIGH);
      //Serial.println("No-Auto motor on");
    }
  }
  // In ra màn hình LCD
      displayLCD(h,t,val);
  // Gửi giá trị đến esp32 qua uart
  SendESP32( h, t, val,controlbutton);
  delay(15);
}
void AutoMotor(float h, float t, float val)
{
  if(h < humidity and t > temp)
  {
     digitalWrite(8, HIGH);
     controlbutton = 2;
  }
  else if(val < hum_land)
  {
     digitalWrite(8, HIGH);
     controlbutton = 2;
  }
  else 
  {
    controlbutton = 1;
    digitalWrite(8, LOW);
  }
}
// Hàm gửi tín hiệu cho ESP32
void SendESP32(float h, float t, float val, int controlbutton)
{
  espSerial.print(val);
  espSerial.print(",");
  espSerial.print(controlMode); 
  espSerial.print(",");
  espSerial.print(controlbutton); 
  espSerial.print(",");
  espSerial.print(hum_land); 
  espSerial.print(",");
  espSerial.print(h);
  espSerial.print(",");
  espSerial.print(t);
  espSerial.print(",");
  espSerial.print(temp); 
  espSerial.print(",");
  espSerial.println(humidity); 

}
// Định nghĩa hàm readSensor
void nhan_don() 
{  
 controlbutton = 1; 
  Serial.println("NhanDon");
      lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("DONG CO TAT");  
    delay(700);
 } 
void nhan_double()
{                                 
  controlbutton = 2;
   Serial.println("NhanDouble"); 
       lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("DONG CO BAT");  
      delay(700);

}
void nhan_don_1() 
{  
 controlMode = 1; 
  Serial.println("NhanDon_1");  
      lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("CHE DO NO AUTO");
        delay(700);

 } 
void nhan_double_1()
{                                    
  controlMode = 2;
   Serial.println("NhanDouble_1");
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("CHE DO AUTO");
  delay(700);
}
void displayLCD(float h, float t, float val) {
  // Hiển thị hàng trên
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(round(t));
  lcd.print(" ");
  lcd.write(1);
  lcd.print("C");
  lcd.print(" ");
  lcd.print(round(h));
  lcd.print(" %RH");
  // Hiển thị hàng dưới
    lcd.setCursor(0,1);
    lcd.print("Do am dat");
  lcd.setCursor(10,1);
  lcd.print(round(val)); 
}