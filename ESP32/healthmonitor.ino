#include <Adafruit_SH1106.h>
#include <Adafruit_GFX.h>
#include <SparkFun_ADXL345.h>


#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"

#include <stddef.h>
#include <math.h>
#include <stdint.h>

#include <WiFi.h>
#include <HTTPClient.h>
#include "time.h"
#include <ArduinoJson.h>


#define OLED_SDA 21
#define OLED_SCL 22

#define ACC_BUF_LEN 600
#define FEATURE_SIZE 12



const char *ssid = "OPPO F17 Pro";
const char *password = "12345678";

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 9900;
const long daylightOffset_sec = 9900;
const char *serverName = "http://192.168.188.164:5000/post";

unsigned long lastTime = 0;
unsigned long timerDelay = 5000;

const int BUFFER_SIZE = JSON_OBJECT_SIZE(10) + JSON_ARRAY_SIZE(ACC_BUF_LEN);

// Accelerometer
ADXL345 adxl = ADXL345(5);

// I2C Ports
TwoWire I2Ctwo = TwoWire(1);

// display
Adafruit_SH1106 display(21, 22);
int button1 = 12;
int button2 = 13;
int buzzer = 2;
const TickType_t xDelay = 5000 / portTICK_PERIOD_MS;

// MAX30105
MAX30105 particleSensor;

const byte RATE_SIZE = 4; 
byte rates[RATE_SIZE];    
byte rateSpot = 0;
long lastBeat = 0; // Time at which the last beat occurred

float beatsPerMinute;
int beatAvg;
int beatAvg_10;
long checkstart;
long irValue;
int beatAvg_new = 0;

// Accelerometer
int int_xyz[3];
int16_t acc_buf[ACC_BUF_LEN] = {0};
int tremor_xyz[3];
volatile int record = 0;
int16_t tremor_acc_buf[ACC_BUF_LEN] = {0};
int16_t tremor_ax[ACC_BUF_LEN / 3] = {0};
int16_t tremor_ay[ACC_BUF_LEN / 3] = {0};
int16_t tremor_az[ACC_BUF_LEN / 3] = {0};

// fall Detection
struct tm last_fall_tm, fall_tm, tremor_tm;
volatile int fall_detected = 0;
hw_timer_t *falltimer = NULL;

// tremor task
TaskHandle_t tremorHandle = NULL;
volatile int tremorFlag;
hw_timer_t *tremortimer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// display fsm
volatile int displayState = 0xF0;
volatile int lastpush = 0;
volatile int thispush;


/*
  Display FSM
  0xF0 : Show Time
  0xF1 : Show Heart Rate
  0xF2 : Show Last Fall
  0xF3 : Record Parkinson's
  0xF4 : Parkinson's Done
  0xF5 : Fall Detected (till fall detection timer is over)
  0xF6 : Menu
  0xF7 : Show Heart Rate select
  0xF8 : Show Last Fall select
  0xF9 : Record Parkinson's select
  0xFA : Go Back
*/

void IRAM_ATTR button1_ISR()
{
  thispush = millis();
  if (thispush - lastpush > 500)
  {
    lastpush = millis();
    if (displayState == 0xF0)
      displayState = 0xF6;
    else if (displayState < 0xFA && displayState >= 0xF6)
      displayState++;
    else if (displayState >= 0xFA)
      displayState = 0xF7;
  }

  if (fall_detected)
  {
    fall_detected = 0;
    displayState = 0xF0;
    digitalWrite(buzzer, LOW);
  }
}

void IRAM_ATTR button2_ISR()
{
  thispush = millis();
  if (thispush - lastpush > 500)
  {
    lastpush = millis();
    if (displayState < 0xFA && displayState >= 0xF6)
      displayState = displayState - 0x06;
    else if (displayState == 0xFA)
      displayState = 0xF0;
  }
}

void IRAM_ATTR ontremortmr()
{
  displayState = 0xF4;
  record = 0;
}

void IRAM_ATTR onfalltmr()
{

  if (fall_detected)
    last_fall_tm = fall_tm;
  displayState = 0xF0;
  digitalWrite(buzzer, LOW);
}


void displaystatus(void *parameter)
{

  while (1)
  {

    if (displayState == 0xF0)
    {
      struct tm timeinfo;
      if (!getLocalTime(&timeinfo))
      {
        Serial.println("Failed to obtain time");
        return;
      }
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(10, 1);
      display.print(timeinfo.tm_mday);
      display.print("-");
      display.print(timeinfo.tm_mon + 1);
      display.print("-");
      display.print(timeinfo.tm_year + 1900);

      display.setCursor(12, 40);
      display.print(timeinfo.tm_hour);
      display.print(":");
      display.print(timeinfo.tm_min);
      display.print(":");
      display.print(timeinfo.tm_sec);
      display.display();

    }

    if (displayState == 0xF1)
    {
      beatAvg_new = beatAvg;
      display.clearDisplay();
      display.setCursor(1, 1);
      display.println("  Average     BPM");
      display.print("    ");
      display.println(beatAvg);
      display.display();
      vTaskDelay(xDelay);
      displayState = 0xF0;
    }

    if (displayState == 0xF2)
    {
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(10, 1);
      display.print("Last Fall Detected:");
      display.setTextSize(1.5);
      display.setCursor(30, 40);
      display.println(&last_fall_tm, "%b %d %Y\n\n      %H:%M:%S");
      display.display();
      vTaskDelay(xDelay);
      displayState = 0xF0;
    }

    if (displayState == 0xF3)
    {
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(10, 1);
      display.println("Recording Tremors\nKeep Your Hand Steady");
      display.setCursor(0, 30);
      display.display();

    if (!record && displayState == 0xF3)
      {
        getLocalTime(&tremor_tm);
        record = 1;
        timerAlarmWrite(tremortimer, 10000000, false);
        timerRestart(tremortimer);
        timerAlarmEnable(tremortimer);
      }
    }

    if (displayState == 0xF4)
    {
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(10, 1);
      display.print("Recording   Done!");
      display.display();
      vTaskDelay(xDelay / 2);
      displayState = 0xF0;
    }

    if (displayState == 0xF5)
    {
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0, 1);
      display.print("   Fall\n Detected!");
      display.setTextSize(1.5);
      display.setCursor(0, 40);
      display.println("   press button 1\n  if false warning");
      display.display();
    }

    if (displayState == 0xF6)
    {
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 1);
      display.println("Heart Rate\n");
      display.println("Last Recorded Fall\n");
      display.println("Record Hand Tremor\n");
      display.println("Go Back");
      display.display();
    }

    if (displayState == 0xF7)
    {
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 1);
      display.println("Heart Rate <-\n");
      display.println("Last Recorded Fall\n");
      display.println("Record Hand Tremor\n");
      display.println("Go Back");
      display.display();
    }

    if (displayState == 0xF8)
    {
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 1);
      display.println("Heart Rate\n");
      display.println("Last Recorded Fall <-\n");
      display.println("Record Hand Tremor\n");
      display.println("Go Back");
      display.display();
    }

    if (displayState == 0xF9)
    {
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 1);
      display.println("Heart Rate\n");
      display.println("Last Recorded Fall\n");
      display.println("Record Hand Tremor <-\n");
      display.println("Go Back");
      display.display();
    }

    if (displayState == 0xFA)
    {
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 1);
      display.println("Heart Rate\n");
      display.println("Last Recorded Fall\n");
      display.println("Record Hand Tremors\n");
      display.println("Go Back <-");
      display.display();
    }
  }
}

void checkheartrate(void *parameter)
{

  while (1)
  {

    if (!particleSensor.begin(I2Ctwo, I2C_SPEED_FAST)) // Use default I2C port, 400kHz speed
    {
      Serial.println("MAX30105 was not found. Please check wiring/power. ");
      while (1)
        ;
    }
    Serial.println("Place your index finger on the sensor with steady pressure.");

    particleSensor.setup();                    // Configure sensor with default settings
    particleSensor.setPulseAmplitudeRed(0x0A); // Turn Red LED to low to indicate sensor is running
    particleSensor.setPulseAmplitudeGreen(0);  // Turn off Green LED

    long irValue = particleSensor.getIR();

    if (checkForBeat(irValue) == true)
    {
      // We sensed a beat!
      long delta = millis() - lastBeat;
      lastBeat = millis();

      beatsPerMinute = 60 / (delta / 1000.0);

      if (beatsPerMinute < 255 && beatsPerMinute > 20)
      {
        rates[rateSpot++] = (byte)beatsPerMinute; // Store this reading in the array
        rateSpot %= RATE_SIZE;                    // Wrap variable

        // Take average of readings
        beatAvg = 0;
        for (byte x = 0; x < RATE_SIZE; x++)
          beatAvg += rates[x];
        beatAvg /= RATE_SIZE;
      }
    }

    Serial.print("IR=");
    Serial.print(irValue);
    Serial.print(", BPM=");
    Serial.print(beatsPerMinute);
    Serial.print(", Avg BPM=");
    Serial.print(beatAvg);

    if (irValue < 50000)
    {
      Serial.print(" No finger?");
    }
    Serial.println();
  }
}

void accelerometer_setup()
{
  adxl.powerOn();
  adxl.setActivityThreshold(75);
  adxl.setInactivityThreshold(75);
  adxl.setTimeInactivity(10);
  adxl.setRangeSetting(21);
  adxl.setActivityX(1);
  adxl.setActivityY(1);
  adxl.setActivityZ(1);

  adxl.setInactivityX(1);
  adxl.setInactivityY(1);
  adxl.setInactivityZ(1);

  adxl.setTapDetectionOnX(0);
  adxl.setTapDetectionOnY(0);
  adxl.setTapDetectionOnZ(1);

  adxl.setTapThreshold(50);
  adxl.setTapDuration(15);
  adxl.setDoubleTapLatency(80);
  adxl.setDoubleTapWindow(200);

  adxl.setFreeFallThreshold(7);
  adxl.setFreeFallDuration(45);

  adxl.setInterruptMapping(ADXL345_INT_SINGLE_TAP_BIT, ADXL345_INT1_PIN);
  adxl.setInterruptMapping(ADXL345_INT_DOUBLE_TAP_BIT, ADXL345_INT1_PIN);
  adxl.setInterruptMapping(ADXL345_INT_FREE_FALL_BIT, ADXL345_INT1_PIN);
  adxl.setInterruptMapping(ADXL345_INT_ACTIVITY_BIT, ADXL345_INT1_PIN);
  adxl.setInterruptMapping(ADXL345_INT_INACTIVITY_BIT, ADXL345_INT1_PIN);

  adxl.setInterrupt(ADXL345_INT_SINGLE_TAP_BIT, 1);
  adxl.setInterrupt(ADXL345_INT_DOUBLE_TAP_BIT, 1);
  adxl.setInterrupt(ADXL345_INT_FREE_FALL_BIT, 1);
  adxl.setInterrupt(ADXL345_INT_ACTIVITY_BIT, 1);
  adxl.setInterrupt(ADXL345_INT_INACTIVITY_BIT, 1);
}

float rms(int16_t *buf, size_t len)
{
  float sum = 0.0;
  for (size_t i = 0; i < len; i++)
  {
    float v = (float)(buf[i] / 100.0f);
    sum += v * v;
  }
  return sqrt(sum / (float)len);
}

float skewness(int16_t *buf, size_t len)
{
  float sum = 0.0f;
  float mean;

  // Calculate the mean
  for (size_t i = 0; i < len; i++)
  {
    sum += (float)(buf[i] / 100.0f);
  }
  mean = sum / len;

  // Calculate the m values
  float m_3 = 0.0f;
  float m_2 = 0.0f;

  for (size_t i = 0; i < len; i++)
  {
    float diff;
    diff = (float)(buf[i] / 100.0f) - mean;
    m_3 += diff * diff * diff;
    m_2 += diff * diff;
  }
  m_3 = m_3 / len;
  m_2 = m_2 / len;

  // Calculate (m_2)^(3/2)
  m_2 = sqrt(m_2 * m_2 * m_2);

  // Calculate skew = (m_3) / (m_2)^(3/2)
  return m_3 / m_2;
}

float std_dev(int16_t *buf, size_t len)
{
  float sum = 0.0f;

  for (size_t i = 0; i < len; i++)
  {
    sum += (float)(buf[i] / 100.0f);
  }

  float mean = sum / len;

  float std = 0.0f;

  for (size_t i = 0; i < len; i++)
  {
    float diff;
    diff = (float)(buf[i] / 100.0f) - mean;
    std += diff * diff;
  }

  return sqrt(std / len);
}

float kurtosis(int16_t *buf, size_t len)
{
  float mean = 0.0f;
  float sum = 0.0f;

  for (size_t i = 0; i < len; i++)
  {
    sum += (float)(buf[i] / 100.0f);
  }
  mean = sum / len;

  // Calculate m_4 & variance
  float m_4 = 0.0f;
  float variance = 0.0f;

  for (size_t i = 0; i < len; i++)
  {
    float diff;
    diff = (float)(buf[i] / 100.0f) - mean;
    float square_diff = diff * diff;
    variance += square_diff;
    m_4 += square_diff * square_diff;
  }
  m_4 = m_4 / len;
  variance = variance / len;

  // Calculate Fisher kurtosis = (m_4 / variance^2) - 3
  return (m_4 / (variance * variance)) - 3;
}

static float dot(float *x, ...)
{
  va_list w;
  va_start(w, 12);
  float dot = 0.0;

  for (uint16_t i = 0; i < 12; i++)
  {
    const float wi = va_arg(w, double);
    dot += x[i] * wi;
  }

  return dot;
}

/**
 * Predict class for features vector
 */
int predict(float *x)
{
  return dot(x, -0.47740808, -0.56071025, 0.018827347, -0.653598, 0.043264825, -0.34762117, 0.0070606587, -0.5321579, -0.19677076, -0.41591045, 0.0068177436, -0.484328) <= -0.6410006758482234 ? 0 : 1;
}

void monitor_fall(void *parameter)
{

  while (1)
  {

    memmove(acc_buf, acc_buf + 3, (ACC_BUF_LEN - 3) * sizeof(int16_t));

    adxl.readAccel(int_xyz);
    acc_buf[ACC_BUF_LEN - 3] = (int16_t)(int_xyz[0]);
    acc_buf[ACC_BUF_LEN - 2] = (int16_t)(int_xyz[1]);
    acc_buf[ACC_BUF_LEN - 1] = (int16_t)(int_xyz[2]);

    int16_t ax[ACC_BUF_LEN / 3], ay[ACC_BUF_LEN / 3], az[ACC_BUF_LEN / 3];
    uint8_t j = 0;

    for (uint16_t i = 0; i < ACC_BUF_LEN; i = i + 3)
    {
      if (record)
      {
        tremor_ax[j] = acc_buf[i];
        tremor_ay[j] = acc_buf[i + 1];
        tremor_az[j] = acc_buf[i + 2];
      }
      j++;
      ax[j] = (int16_t)(int_xyz[0] * 0.003);
      ay[j] = (int16_t)(int_xyz[1] * 0.003);
      az[j] = (int16_t)(int_xyz[2] * 0.003);
    }

    float features[FEATURE_SIZE] = {
        rms(ax, ACC_BUF_LEN / 3),
        std_dev(ax, ACC_BUF_LEN / 3),
        skewness(ax, ACC_BUF_LEN / 3),
        kurtosis(ax, ACC_BUF_LEN / 3),
        rms(ay, ACC_BUF_LEN / 3),
        std_dev(ay, ACC_BUF_LEN / 3),
        skewness(ay, ACC_BUF_LEN / 3),
        kurtosis(ay, ACC_BUF_LEN / 3),
        rms(az, ACC_BUF_LEN / 3),
        std_dev(az, ACC_BUF_LEN / 3),
        skewness(az, ACC_BUF_LEN / 3),
        kurtosis(az, ACC_BUF_LEN / 3),
    };

    //    /* Normalize Features */
    float max[FEATURE_SIZE] = {
        20.563904, 20.22484, 10.65187, 115.28641,
        25.33499, 23.21588, 8.645585, 92.44442,
        19.243101, 17.982597, 7.243396, 83.07158};

    float min[FEATURE_SIZE] = {
        0.06523956, 0.04126525, -10.173604, -1.7842116,
        0.06462117, 0.05163805, -8.996278, -1.838343,
        0.07216922, 0.06428464, -8.466334, -1.8937062};

    for (uint8_t i = 0; i < FEATURE_SIZE; i++)
    {
      features[i] = (features[i] - min[i]) / (max[i] - min[i]);
    }

    const int32_t predicted_class = predict(features);
    if (predicted_class == 0)
    {
      fall_detected = 1;
      digitalWrite(buzzer, HIGH);
      displayState = 0xF5;
      getLocalTime(&fall_tm);

      timerAlarmWrite(falltimer, 10000000, false);
      timerRestart(falltimer);
      timerAlarmEnable(falltimer);
    }

    if ((millis() - lastTime) > timerDelay)
    {
      Serial.println("Sending data");
      if (WiFi.status() == WL_CONNECTED)
      {

        WiFiClient client;
        HTTPClient http;

        http.begin(serverName);

        http.addHeader("Content-Type", "application/json");

        StaticJsonDocument<BUFFER_SIZE> doc;

        doc["HR"] = beatAvg;
        doc["fallDay"] = last_fall_tm.tm_mday;
        doc["fallMon"] = last_fall_tm.tm_mon + 1;
        doc["fallYear"] = last_fall_tm.tm_year + 1900;
        doc["fallHr"] = last_fall_tm.tm_hour;
        doc["fallMin"] = last_fall_tm.tm_min;
        doc["fallSec"] = last_fall_tm.tm_sec;

        doc["tremorDay"] = tremor_tm.tm_mday;
        doc["tremorMon"] = tremor_tm.tm_mon + 1;
        doc["tremorYear"] = tremor_tm.tm_year + 1900;
        doc["tremorHr"] = tremor_tm.tm_hour;
        doc["tremorMin"] = tremor_tm.tm_min;
        doc["tremorSec"] = tremor_tm.tm_sec;
        doc["istremor"] = record;
        JsonArray acc_x = doc.createNestedArray("acc_x");
        for (int i = 0; i < ACC_BUF_LEN / 3; i++)
          acc_x.add(tremor_ax[i]);

        JsonArray acc_y = doc.createNestedArray("acc_y");
        for (int i = 0; i < ACC_BUF_LEN / 3; i++)
          acc_y.add(tremor_ay[i]);

        JsonArray acc_z = doc.createNestedArray("acc_z");
        for (int i = 0; i < ACC_BUF_LEN / 3; i++)
          acc_z.add(tremor_az[i]);

        String requestBody;
        serializeJson(doc, requestBody);
        int httpResponseCode = http.POST(requestBody);

        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
      }
      else
      {
        Serial.println("WiFi Disconnected");
      }
      lastTime = millis();
    }
  }
}

void setup()
{

  Serial.begin(115200);
  Serial.println("Initializing...");

  accelerometer_setup();

  I2Ctwo.begin(32, 33);

  display.begin(SH1106_SWITCHCAPVCC, 0x3C);
  display.setTextSize(2);
  display.setTextColor(WHITE);

  // Connectivity
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" CONNECTED");

  // init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  pinMode(button1, INPUT_PULLUP);
  pinMode(button2, INPUT_PULLUP);
  pinMode(buzzer, OUTPUT);
  attachInterrupt(button1, button1_ISR, FALLING);
  attachInterrupt(button2, button2_ISR, FALLING);

  // tremor timer setup for 10 seconds
  tremortimer = timerBegin(3, 80, true);
  timerAttachInterrupt(tremortimer, &ontremortmr, true);

  falltimer = timerBegin(1, 80, true);
  timerAttachInterrupt(falltimer, &onfalltmr, true);

  xTaskCreate(
      checkheartrate,
      "Check Heart Rate",
      2000,
      NULL,
      2,
      NULL);

  xTaskCreate(
      displaystatus,
      "OLED Display",
      2000,
      NULL,
      1,
      NULL);

  xTaskCreate(
      monitor_fall,
      "Monitor Fall",
      15000,
      NULL,
      3,
      NULL);
}

void loop()
{
}
