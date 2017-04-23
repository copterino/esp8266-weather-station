#include "Sensor.h"

#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>

// defines WIFI_SSID, WIFI_PASS, SQL_USER, SQL_PASS, SQL_SERVER_ADDR
#include "config.h"

const int led = D0;

ESP8266WebServer httpServer(80);

Sensor sensors[2] = {
    Sensor(SI_Guestroom, 0x77),
    Sensor(SI_Street, 0x76)
};

WiFiClient wifiClient;
MySQL_Connection sqlConn(&wifiClient);
MySQL_Cursor sqlCursor(&sqlConn);
IPAddress sqlAddr;

void handleRoot()
{
    digitalWrite(led, 0);
    httpServer.send(200, "text/plain", "hello from esp8266!");
    digitalWrite(led, 1);
}

void handleWeather()
{
    digitalWrite(led, 0);

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

    httpServer.send(200, "text/html", message);

    digitalWrite(led, 1);
}

void handleNotFound()
{
    digitalWrite(led, 0);
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += httpServer.uri();
    message += "\nMethod: ";
    message += (httpServer.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += httpServer.args();
    message += "\n";
    for (uint8_t i = 0; i < httpServer.args(); ++i)
    {
        message += " " + httpServer.argName(i) + ": " + httpServer.arg(i) + "\n";
    }
    httpServer.send(404, "text/plain", message);
    digitalWrite(led, 1);
}

void sendDataToSql(Sensor& sensor)
{
    if (sensor.isValid())
    {
        sendDataToSql(sensor.getTemperature(),
                      roundf(sensor.getHumidity()),
                      roundf(sensor.getPressureMmHg()),
                      sensor.getId());
    }
}

void sendDataToSql(float temp, int humidity, int pressure, int sensorId)
{
    digitalWrite(led, 0);

    char buf[128];
    char tempStr[7];
    dtostrf(temp, sizeof(tempStr)-1, 2, tempStr);

    int res = snprintf(buf, sizeof(buf),
        "INSERT INTO Weather.sensor_smile (temp, hum, press, sensorId) VALUES (%s, %d, %d, %d)",
        tempStr, humidity, pressure, sensorId);

    if (res < 0 || res >= sizeof(buf))
    {
        DebugPrint("The SQL buffer is too small.\n");
        return;
    }

    if (sqlCursor.execute(buf))
    {
        DebugPrint("SQL sent: %s\n", buf);
    }

    digitalWrite(led, 1);
}

void setupOTA()
{
    // No authentication by default
    // ArduinoOTA.setPassword((const char*)"my_password");

    ArduinoOTA.setHostname("weather-station-1");

    ArduinoOTA.onStart([](){ DebugPrint("OTA start\n"); });
    ArduinoOTA.onEnd([](){ DebugPrint("\nOTA end\n"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
    {
        DebugPrint("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error)
    {
        DebugPrint("Error[%u]: ", error);
        switch (error)
        {
            case OTA_AUTH_ERROR: DebugPrint("Auth Failed\n"); break;
            case OTA_BEGIN_ERROR: DebugPrint("Begin Failed\n"); break;
            case OTA_CONNECT_ERROR: DebugPrint("Connect Failed\n"); break;
            case OTA_RECEIVE_ERROR: DebugPrint("Receive Failed\n"); break;
            case OTA_END_ERROR: DebugPrint("End Failed\n"); break;
        }
    });
}

void setup(void)
{
    pinMode(led, OUTPUT);
    digitalWrite(led, 1);
    Serial.begin(115200);

    sqlAddr.fromString(SQL_SERVER_ADDR);

    httpServer.on("/", handleRoot);
    httpServer.on("/weather", handleWeather);
    httpServer.onNotFound(handleNotFound);
    httpServer.begin();
    DebugPrint("HTTP server started\n");

    setupOTA();

    for (auto& sensor : sensors)
    {
        sensor.init();
    }
}

void loop(void)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        DebugPrint("Connecting to %s...\n", WIFI_SSID);

        WiFi.mode(WIFI_STA);  // Client
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        DebugPrint("\n");

        while (WiFi.status() != WL_CONNECTED)
        {
            delay(1000);
            DebugPrint(".");
        }

        //if (WiFi.waitForConnectResult() != WL_CONNECTED)
        //    return;

        DebugPrint("\nConnected to %s\n", WIFI_SSID);
        DebugPrint("IP address: %s\n", WiFi.localIP().toString().c_str());

        ArduinoOTA.begin();
    }

    if (!sqlConn.connected())
    {
        DebugPrint("Connecting to SQL... ");
        if (sqlConn.connect(sqlAddr, 3306, SQL_USER, SQL_PASS))
        {
            DebugPrint("OK.\n");
        }
        else
        {
            DebugPrint("FAILED.\n");
        }
    }

    httpServer.handleClient();

    unsigned long curTime = millis();
    for (auto& sensor : sensors)
    {
        if (sensor.update(curTime))
        {
            sendDataToSql(sensor);
        }
    }

    ArduinoOTA.handle();
}
