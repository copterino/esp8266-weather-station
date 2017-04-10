#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <Adafruit_BME280.h>

#undef WITH_SELECT
#include <mysql.h>

// defines
//WIFI_SSID, WIFI_PASS
//SQL_USER, SQL_PASS, SQL_SERVER_ADDR
#include "config.h"

enum SensorIdEnum
{
    SI_Guestroom = 1000,
    SI_Bedroom = 1001,
    SI_Kitchen = 1002,
    SI_Street = 1003
};

struct SensorData
{
    SensorData() {}
    SensorData(float t, float h, float p) : temp(t), hum(h), press(p) {}
    float temp = 0.f;
    float hum = 0.f;
    float press = 0.f;
};

// I2C (SDA - D2, SCL - D1)
class Sensor
{
public:
    Sensor(int sensorId, int chipAddr) : id(sensorId), chipAddress(chipAddr) {}

    bool init()
    {
        valid = bme.begin(chipAddress);
        if (!valid)
        {
            Serial.print("Could not find a valid BME280 sensor for 0x");
            Serial.print(chipAddress, HEX);
            Serial.println(" address.");
        }
        else
        {
            Serial.print("Sensor BME280 (0x");
            Serial.print(chipAddress, HEX);
            Serial.println(") connected.");
        }
        return valid;
    }

    bool isValid() { return valid; }
    int getId() { return id; }

    float getTemperature() { return temperature; }
    float getHumidity() { return humidity; }
    float getPressureMmHg() { return pressure; }

    SensorData getRawData()
    {
        if (valid)
            return SensorData(bme.readTemperature(), bme.readHumidity(), bme.readPressure());
        return SensorData();
    }

    bool update(unsigned long curTime)
    {
        if (!valid)
        {
            /*static int blink = 0;
            if (curTime - lastUpdateTime > 250)
            {
                digitalWrite(led, blink);
            }

            delay(250);
            Serial.print("Sensor not connected ");
            Serial.println(chipAddress);
            digitalWrite(led, blink);*/
            return false;
        }
        if (curTime - lastUpdateTime > updateInterval)
        {
            lastUpdateTime = curTime;
            accumulateData();

            if (++samplesCount == samplesNeeded)
            {
                temperature = accumTemp / samplesCount;
                humidity = accumHumidity / samplesCount;
                pressure = accumPressure * 0.0075006f / samplesCount;

                samplesCount = 0;
                accumTemp = 0.f;
                accumHumidity = 0.f;
                accumPressure = 0.f;

                return isDataChanged();
            }
        }
        return false;
    }

private:
    void accumulateData()
    {
        accumTemp += bme.readTemperature();
        accumHumidity += bme.readHumidity();
        accumPressure += bme.readPressure();

        Serial.print("Sensor 0x");
        Serial.print(chipAddress, HEX);
        Serial.print(" acc: t: ");
        Serial.print(accumTemp);
        Serial.print(" h: ");
        Serial.print(accumHumidity);
        Serial.print(" p: ");
        Serial.println(accumPressure);
    }

    bool isDataChanged()
    {
        bool dataChanged = false;
        if (fabs(lastTriggerTemp - temperature) >= 0.5f)  // diff 0.5C
        {
            lastTriggerTemp = temperature;
            dataChanged = true;
        }
        if (fabs(lastTriggerHumidity - humidity) >= 1.f)  // diff 0.5%
        {
            lastTriggerHumidity = humidity;
            dataChanged = true;
        }
        if (fabs(lastTriggerPressure - pressure) >= 1.f)  // diff 1mmHg
        {
            lastTriggerPressure = pressure;
            dataChanged = true;
        }

        if (!dataChanged)
        {
            Serial.print("Data not changed: t: ");
            Serial.print(temperature);
            Serial.print(" h: ");
            Serial.print(humidity);
            Serial.print(" p: ");
            Serial.println(pressure);
        }
        return dataChanged;
    }

    const unsigned long updateInterval = 12 * 1000;
    const unsigned long samplesNeeded = 5;

    int id = 0;
    int chipAddress = 0;
    bool valid = false;

    unsigned long lastUpdateTime = 0;
    unsigned long samplesCount = 0;
    float temperature = 0.f;
    float humidity = 0.f;
    float pressure = 0.f;
    float lastTriggerTemp = 0.f;
    float lastTriggerHumidity = 0.f;
    float lastTriggerPressure = 0.f;
    float accumTemp = 0.f;
    float accumHumidity = 0.f;
    float accumPressure = 0.f;

    Adafruit_BME280 bme;
};

const int led = D0;

ESP8266WebServer server(80);

Sensor sensors[2] = {
    Sensor(SI_Guestroom, 0x76),
    Sensor(SI_Street, 0x77)
};

Connector sqlConn;
IPAddress sqlAddr;


void handleRoot()
{
    digitalWrite(led, 1);
    server.send(200, "text/plain", "hello from esp8266!");
    digitalWrite(led, 0);
}

void handleWeather()
{
    digitalWrite(led, 1);

    String message =
        "<!DOCTYPE HTML>"
        "<html>"
        "<head><META HTTP-EQUIV=\"refresh\" CONTENT=\"15\"></head><body>"
        "<h1>ESP8266 Weather Web Server</h1>";

    for (auto& sensor : sensors)
    {
        SensorData data = sensor.getRawData();

        char temperatureString[7];
        char humidityString[7];
        char pressureMmString[6];
        char dpString[7];
        float pmm = data.press * 0.0075006f;
        float dp = data.temp - 0.2f*(100.f - data.hum);
        dtostrf(data.temp, 6, 1, temperatureString);
        dtostrf(data.hum, 6, 1, humidityString);
        dtostrf(pmm, 5, 2, pressureMmString);
        dtostrf(dp, 6, 1, dpString);

        message += "<h1>BME280 sensor ";
        message += sensor.getId();
        message += "</h1><table border=\"2\" width=\"456\" cellpadding=\"10\"><tbody><tr><td><h3>Temperature = ";
        message += temperatureString;
        message += "&deg;C</h3><h3>Humidity = ";
        message += humidityString;
        message += "%</h3><h3>Approx. Dew Point = ";
        message += dpString;
        message += "&deg;C</h3><h3>Pressure = ";
        message += pressureMmString;
        message += " mmHg</h3></td></tr></tbody></table><br>";
    }

    message += "</body></html>";

    server.send(200, "text/html", message);

    digitalWrite(led, 0);
}

void handleNotFound()
{
    digitalWrite(led, 1);
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    for (uint8_t i = 0; i < server.args(); ++i)
    {
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    server.send(404, "text/plain", message);
    digitalWrite(led, 0);
}

void sendDataToSql(Sensor& sensor)
{
    if (sensor.isValid())
    {
        sendDataToSql(sensor.getTemperature() * 10,
                      roundf(sensor.getHumidity()),
                      roundf(sensor.getPressureMmHg()),
                      sensor.getId());
    }
}

void sendDataToSql(int temp10, int humidity, int pressure, int sensorId)
{
    digitalWrite(led, 1);
    delay(150);
    digitalWrite(led, 0);
    delay(150);
    digitalWrite(led, 1);

    char buf[128];
    const int temp1 = temp10 / 10;
    const int temp2 = temp10 - temp1 * 10;

    int res = snprintf(buf, sizeof(buf),
        "INSERT INTO Weather.sensor_smile (temp, hum, press, sensorId) VALUES (%d.%d, %d, %d, %d)",
        temp1, temp2, humidity, pressure, sensorId);

    if (res < 0 || res >= sizeof(buf))
    {
        Serial.println("The SQL buffer is too small.");
    }

    if (sqlConn.cmd_query(buf))
    {
        Serial.print("SQL sent: ");
        Serial.println(buf);
    }

    digitalWrite(led, 0);
}

void setup(void)
{
    pinMode(led, OUTPUT);
    digitalWrite(led, 0);
    Serial.begin(115200);
    
    sqlAddr.fromString(SQL_SERVER_ADDR);

    server.on("/", handleRoot);
    server.on("/weather", handleWeather);
    server.onNotFound(handleNotFound);
    server.begin();
    Serial.println("HTTP server started");

    for (auto& sensor : sensors)
    {
        sensor.init();
    }
}

void loop(void)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.print("Connecting to ");
        Serial.print(WIFI_SSID);
        Serial.println("...");
        
        WiFi.mode(WIFI_STA);  // Client
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        Serial.println("");

        while (WiFi.status() != WL_CONNECTED)
        {
            delay(500);
            Serial.print(".");
        }

        //if (WiFi.waitForConnectResult() != WL_CONNECTED)
        //    return;
        
        Serial.println("");
        Serial.print("Connected to ");
        Serial.println(WIFI_SSID);
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        
        if (sqlConn.mysql_connect(sqlAddr, 3306, SQL_USER, SQL_PASS))
        {
            delay(500);
            Serial.println("SQL Connected.");
        }
        else
        {
            Serial.println("Connection failed.");
        }
    }
    
    server.handleClient();

    unsigned long curTime = millis();
    for (auto& sensor : sensors)
    {
        if (sensor.update(curTime))
        {
            sendDataToSql(sensor);
        }
    }
}

