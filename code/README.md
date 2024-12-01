There are reference code, local test version, network test version and integrated version.

## some mistakes
Q1: I defined a new topic to control the lights. I could see the messages I published in MQTT Explorer, but the lights didn't change.

A: Subscribers can only receive the messages that are published to a topic they have set.

## some challenges
Q1: In order to obtain more accurate heart rate values, the sensor must be sampled at a high frequency. However, if the sampling frequency is too fast, the reading will be too fast. If delay() is added, it will take a long time to obtain data.

A：Use millis() instead of delay() to control the time interval of data collection without blocking other tasks in the main loop.



Q2: After adding the OLED display, the sensor can no longer calculate the heart rate properly.

A: OLED and sensor share an I2C wire, and the refresh of OLED screen interferes with the heart rate detection of sensor.
1)Introduce time interval to separate the update of OLED and sensor;
2)Reduce the refresh frequency of OLED and increase the sampling frequency of sensor.
