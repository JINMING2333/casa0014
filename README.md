# casa0014_LumiHealth
A device to monitor the health statues of users and create a comfortable light environment for them.

## Inspiration
I have always been interested in the built environment and human health, so I want to focus on them. I make this device as an extension of the coursework of casa0016, a smart chair that helps users develop healthy sitting habits, and together they will create a comfortable working and living environment for people.

### Current Application Scenarios
1) Medical Assistance
   Used for data collection and remote monitoring, when the patient's heart rate exceeds the normal threshold, the LED lights up red to alarm.
2) Fitness Tracking
   Use LED to indicate exercise intensity, such as blue means need to increase exercise intensity and red means reduce it.
3) Emotion Regulation
   Create a soothing environment through warm and cool tones or breathing light effects to help users relax.

## Components
1) Arduino MKR 1010;
2) MAX30102 sensor - 
   The sensor can monitor IR value(infrared reflection value) and temperature. The IR value reflects the change of blood flow, which needs to be further calculated to get the heart rate. I referred to the heart rate example of its library.
3) OLED display;
4) Jump wires;

## Current modes
1) Health status monitor mode: LED color changes can reflect different heart rate ranges.
   1) Blue indicates a lower heart rate(40-60 BPM).
   2) Yellow indicates a normal heart rate(60-110 BPM).
   3) Red indicates a high heart rate(110-140 BPM).
2) Relaxation mode: Adjust the lighting according to different body temperature ranges to create a relaxing atmosphere.
   1) when temperature is low, LED shows warm color gradients. 
   2) When temperature is high, LED shows cool color gradients;
   
PS: Switch mode by taping the surface of the sensor(when my finger approaches the sensor, the infrared reflection value changes greatly).

## System architecture
The operation of the whole project:
1) Make ensure the operating environment(connect WIFI and MQTT) is set up first;
2) Select the mode;
3) Read the data through the sensor, and then run different functions under the corresponding mode;
4) Publish and subscribe pixel topics through MQTT, update the light status, and realize remote operation.

MAX30102 sensor -> Arduino MKR 1010 -> WIFI -> MQTT broker -> LEDs

## Need to be improved...
1) In the second mode, the gradient of warm and cool tones may not be soothing enough. Later I want to change them to breathing light effect through brightness.
2) Rich lighting effects that adjust in real time based on heart rate can be used as interactive artworks in public spaces or exhibitions to attract audience interaction.
