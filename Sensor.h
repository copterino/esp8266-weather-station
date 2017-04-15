#include <Adafruit_BME280.h>

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