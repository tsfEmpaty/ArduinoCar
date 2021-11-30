#include <Adafruit_NeoPixel.h>
#include "GyverFilters.h"
#include <Wire.h>
#include <VL53L0X.h>

#define rudderpin A0
#define shutdown1 A2
#define shutdown2 A3
#define WheelPINA 10 //D11
#define WheelPINB 11 //D10
#define LED_COUNT 32
#define LED_PIN 2 //D2
#define PIN_TRIG 12
#define PIN_ECHO 9


VL53L0X left_sensor;
VL53L0X right_sensor;
GFilterRA analog0;
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

const int MAX_SPEED = 35;
const int MIN_RUDDER_ANGLE = 392;
const int MAX_RUDDER_ANGLE = 635;

int lastTime = 0;
int angle = 0;
bool flag;

bool is_show = true;
bool is_line = true;
void setup() 
{
  Serial.begin(9600);
  pinMode(rudder, INPUT);
  pinMode(WheelPINA, OUTPUT);
  pinMode(WheelPINB, OUTPUT);
  pinMode(3, OUTPUT);
  TCCR2B = TCCR2B & B11111000 | B00000100; //делитель 64 для частоты ШИМ 490.20 Гц (по умолчанию)
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(WheelPINA, OUTPUT);
  pinMode(WheelPINB, OUTPUT);
  analog0.setCoef(0.04); // установка коэффициента фильтрации (0.0... 1.0). Чем меньше, тем плавнее фильтр
  analog0.setStep(1); // установка шага фильтрации (мс). Чем меньше, тем резче фильтр
  
  strip.begin();
  strip.show();
  strip.setBrightness(255);

  pinMode(3, OUTPUT);
  pinMode(13, OUTPUT);

  Wire.begin();

  pinMode(shutdown1, OUTPUT);
  pinMode(shutdown2, OUTPUT);
  
  // reset all VL53
  digitalWrite (shutdown1, LOW);
  digitalWrite (shutdown2, LOW);

  // init left sensor
  digitalWrite (shutdown1, HIGH);
  delay (100);  /// some wait
  left_sensor.setAddress(45);
  delay (100);  /// some wait
  left_sensor.setTimeout(500);
  if (!left_sensor.init())
  {
    Serial.println("Failed to detect and initialize left sensor!");
    while (1) {}
  }


  // init right sensor
  digitalWrite (shutdown2, HIGH);
  delay (100);  /// some wait
  right_sensor.setAddress(46);
  delay (100);  /// some wait
  right_sensor.setTimeout(500);
  if (!right_sensor.init())
  {
    Serial.println("Failed to detect and initialize right sensor!");
    while (1) {}
  }

  left_sensor.startContinuous();
  right_sensor.startContinuous();
}

void loop() {
  int forword_distance = get_forword_distance();
  int left_distance = left_sensor.readRangeSingleMillimeters() / 10; 
  int right_distance = right_sensor.readRangeSingleMillimeters() / 10;
  int rudder_angle = get_rudder_angle();
  
  int Stop = map(forword_distance, 0, 1200, 0, MAX_SPEED);
  //Алгоритм газа
  if (forword_distance >= 70) 
  {
    analogWrite(3, MAX_SPEED);
  }
  else if (forword_distance <= 70)
  {
    analogWrite(3, Stop);
  }
  else if (forword_distance <= 50)
  {
    analogWrite(3, 0);
  }
  analogWrite(3, 20);
  //Алгоритм поворотов
  if (left_distance < 80)
    rudder(-35);
  if (left_distance < 60)
    rudder(-40);
  if (left_distance < 40)
    rudder(-45);
  if (left_distance < 20)
    rudder(-55);
  
  if (right_distance < 80)
    rudder(35);
  if (right_distance < 60)
    rudder(40);
  if (right_distance < 40)
    rudder(45);
  if (right_distance < 20)
    rudder(55);
  else
  {
    rudder(0);
  }
  
  if (is_show)
  {
    if (is_line)
    {
      Serial.println(String(forword_distance) + ", " + 
                     String(left_distance) + ", " + 
                     String(right_distance) + ", " + 
                     String(rudder_angle));
    }
    else
    {
      Serial.println("          ");
      Serial.println("Sensors");
      Serial.println("----------");
      Serial.println("Forword distance: " + String(forword_distance) + " cm");
      Serial.println("Left distance: " + String(left_distance) + " cm");
      Serial.println("Right distance: " + String(right_distance) + " cm");
      Serial.println("Rudder angle: " + String(rudder_angle) + " popugays");
    }
  }
}

void rotate(bool flag, int swingSpeed)
{ // flag (true(right side) / false (left side)), swingSpeed(0 - 100);
  int rudder_angle = get_rudder_angle();
  if (rudder_angle > -55 || rudder_angle < 55)
  {
    // Поворачиваем
    if (flag)
      digitalWrite(WheelPINB, HIGH);
    else
      digitalWrite(WheelPINB, LOW); 
    analogWrite(WheelPINA, swingSpeed);
  }
  else
  {
    digitalWrite(WheelPINB, LOW);
    digitalWrite(WheelPINA, LOW);
  }
  return rudder_angle;
}


float get_forword_distance()
{
  digitalWrite(PIN_TRIG, LOW); // Сначала генерируем короткий импульс длительностью 2-5 микросекунд.
  delayMicroseconds(5);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);  // Выставив высокий уровень сигнала, ждем около 10 микросекунд. В этот момент датчик будет посылать сигналы с частотой 40 КГц.
  digitalWrite(PIN_TRIG, LOW);
  float duration = pulseIn(PIN_ECHO, HIGH); //  Время задержки акустического сигнала на эхолокаторе.
  return (duration / 2.0) / 29.1; // Теперь осталось преобразовать время в расстояние
}

int get_rudder_angle()
{
  int rudder_angle = map(analogRead(rudderpin), MIN_RUDDER_ANGLE, MAX_RUDDER_ANGLE, -100, 100) + 26;
//  return analog0.filteredTime(rudder_angle);
  return rudder_angle;
}

void rainbow(int i, int R, int G, int B) 
{
  strip.setPixelColor(i, strip.gamma32(strip.Color(R, G, B)));
  strip.show();
}

void rudder(int angle)
{
  int rudder_angle = get_rudder_angle();
  if (rudder_angle > angle + 2)
  {
    digitalWrite(WheelPINA, HIGH);
    digitalWrite(WheelPINB, LOW);
//    Serial.println(l
  }
  else if (rudder_angle < angle - 2)
  {
    digitalWrite(WheelPINA, LOW);
    digitalWrite(WheelPINB, HIGH);
  }
  delay(3);
  digitalWrite(WheelPINA, LOW);
  digitalWrite(WheelPINB, LOW);
  Serial.print(analogRead(rudderpin));
  Serial.print(" : ");
  Serial.println(rudder_angle);
}
