
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include "myconfig.h"
#include <Adafruit_NeoPixel.h>
#include <UniversalTelegramBot.h>

const int REED_PIN = D5;
const int LED_PIN = D3;

const int MAXOPEN_SEC = 60;

#define NUM_PIXELS 12
#define BOT_TOKEN "1492549551:AAFPhhl6T3F0A_YaZe5mxsGUoMBveLjg6dE"
#define CHAT_ID "129408194"

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(12, LED_PIN, NEO_GRB + NEO_KHZ800);
WiFiClientSecure secure_client;
UniversalTelegramBot bot(BOT_TOKEN, secure_client);

long lastMsg = 0;
char msg[50];
int value = 0;
bool isupdating = false;
int lasti;

void sendTelegram(String c)
{
  bot.sendMessage(CHAT_ID, c);
}

void setup_wifi()
{
  delay(10);

  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  // Do some Animation while connecting to Wifi
  int i = 0;
  int h = 18000;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    pixels.setPixelColor(i, pixels.ColorHSV(h, 255, 100));
    pixels.show();
    i = (i + 1) % NUM_PIXELS;
    h = (h + 200) % 32000;
    Serial.print(".");
  }

  // Flash green when finished
  for (int o = 0; o < 180; o++)
  {
    for (int i = 0; i < NUM_PIXELS; i++)
    {
      pixels.setPixelColor(i, pixels.Color(0, sin(PI / 180.0 * (float)o) * 255, 0));
    }
    pixels.show();
    delay(5);
  }

  pixels.clear();
  pixels.show();

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  if (!MDNS.begin("garage-door"))
  { // Start the mDNS responder for esp8266.local
    Serial.println("Error setting up MDNS responder!");
  }

  // OverTheAirUpdates
  // No fancy animation onStart and onProgress because it makes our updateprocess unstable

  ArduinoOTA.setHostname("garage-door");
  ArduinoOTA.onStart([]() {
    pixels.clear();
    pixels.show();
    isupdating = true;
  });
  ArduinoOTA.onEnd([]() {
    pixels.clear();
    pixels.show();
    isupdating = false;
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    // Show a simple LED Progress
    int po = (progress / (total / 12));
    if (lasti != po)
    {
      lasti = po;
      for (int i = 0; i < lasti; i++)
      {
        pixels.setPixelColor(i, pixels.Color(0, 0, 255));
      }
      pixels.show();
    }
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)
      Serial.println("End Failed");
    isupdating = false;
    ESP.reset();
  });

  ArduinoOTA.begin();
  Serial.println("OTA ready");
}

// Unused by now
void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1')
  {
    digitalWrite(LED_BUILTIN, LOW); // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is acive low on the ESP-01)
  }
  else
  {
    digitalWrite(LED_BUILTIN, HIGH); // Turn the LED off by making the voltage HIGH
  }
}

void reconnect()
{
  int retries = 2;

  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");

    // Attempt to connect
    if (client.connect("garagedoor"))
    {
      Serial.println("connected");
      client.publish("/garage/door/status", "REBOOT");
      client.subscribe("inTopic");
    }
    else
    {
      Serial.println(WiFi.status());
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");

      delay(5000);
      retries--;
      if (retries < 0)
        ESP.reset();
    }
  }
}

void setup()
{

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  pinMode(REED_PIN, INPUT_PULLUP);
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);

  Serial.begin(115200);

  pixels.begin();
  pixels.clear();
  pixels.show();

  secure_client.setInsecure();

  // Do some nice startanimation
  // TODO: make it nice
  for (int o = 0; o < 180; o++)
  {
    uint8_t v = sin(PI / 180.0 * (double)o) * 255;
    for (int i = 0; i < NUM_PIXELS; i++)
    {
      pixels.setPixelColor(i, pixels.ColorHSV(18000, v, v));
    }
    pixels.show();
    delay(10);
  }
  pixels.clear();
  pixels.show();

  setup_wifi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

long last_time_looprate, loop_rate;
long last_time_pub;
char c[30];
bool laststatus;

long start_time_open;
int alertMode = 0;
long start_time_alert;
bool lastalerthigh;
long lastalertstop;
bool lastalertshow;

void loop()
{
  ArduinoOTA.handle();

  if (isupdating)
    return;
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.print("NO WIFI");
  }
  if (!client.connected())
  {
    reconnect();
  }

  loop_rate++;
  long curtime = millis();

  if (curtime - last_time_looprate > 1000)
  {
    Serial.print(F("LoopRate "));
    float rate = (float)loop_rate / ((curtime - last_time_looprate) / 1000.0);
    Serial.print(rate);
    sprintf(c, "%f", rate);
    client.publish("/garage/door/rate", c);
    Serial.println("HZ ");
    loop_rate = 0;
    last_time_looprate = curtime;
  }

  bool open = digitalRead(REED_PIN);

  // Only if status changed ...
  if (open != laststatus)
  {
    // Publish on StatusChange Topic...
    client.publish("/garage/door/status_change", open ? "OPEN" : "CLOSED");
    Serial.println("Publishing to change topic");
    laststatus = open;
    //digitalWrite(LED_BUILTIN, !open);

    // If now open, remember when...
    if (open)
    {
      start_time_open = millis();
      sendTelegram("GARAGE DOOR IS OPEN");
    }
    else
    {
      alertMode = 0;
      for (int i = 0; i < NUM_PIXELS; i++)
      {
        pixels.setPixelColor(i, pixels.Color(0, 255, 0));
      }
      pixels.show();
      lastalertshow = true;
      lastalertstop = millis();
    }
  }

  if (open && curtime - start_time_open > MAXOPEN_SEC * 1000 && alertMode == 0)
  {
    alertMode = 1;

    start_time_alert = millis();
  }
  if (alertMode == 0 && lastalertshow && millis() - lastalertstop > 30)
  {
    lastalertshow = false;
    pixels.clear();
    pixels.show();
    sendTelegram("Ok, GarageDoor is closed again.");
  }

  if (alertMode > 0)
  {

    int delayms = 3000;

    if (alertMode == 1 && millis() - start_time_alert > 60000)
    {
      alertMode = 2;
    }

    if (alertMode > 1)
      delayms = 1000;

    bool thisalert = false;
    if (curtime % delayms < 30)
    {
      thisalert = true;
    }
    else
    {
      thisalert = false;
    }
    if (lastalerthigh != thisalert)
    {
      if (thisalert)
      {
        for (int i = 0; i < NUM_PIXELS; i++)
        {
          pixels.setPixelColor(i, pixels.Color(255, 0, 0));
        }
        pixels.show();
      }
      else
      {
        for (int i = 0; i < NUM_PIXELS; i++)
        {
          pixels.setPixelColor(i, pixels.Color(1, 0, 0));
        }
        pixels.show();
      }
    }
    lastalerthigh = thisalert;
  }

  if (curtime - last_time_pub > 5000)
  {
    client.publish("/garage/door/status", open ? "OPEN" : "CLOSED");
    Serial.println("Publishing to status topic");
    last_time_pub = millis();
  }

  client.loop();
}
