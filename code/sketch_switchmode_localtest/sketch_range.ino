#include <Wire.h>
#include "MAX30105.h" // 使用 SparkFun MAX3010x 库
#include <Adafruit_NeoPixel.h>

#define LED_PIN 6          // 控制灯带的引脚
#define NUM_PIXELS 30      // 灯带上 LED 的数量

MAX30105 particleSensor;
Adafruit_NeoPixel strip(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);

  // 初始化灯带
  strip.begin();
  strip.show(); // 清除所有灯光
  strip.setBrightness(50); // 设置灯带亮度

  // 初始化 MAX30102
  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println("MAX30102 not detected. Check connections!");
    while (1);
  }
  particleSensor.setup(); // 默认配置
  Serial.println("MAX30102 initialized.");
}

void loop() {
  // 读取红光、红外光数据
  long redValue = particleSensor.getRed();
  long irValue = particleSensor.getIR();

  // 计算心率和 SpO2 （需要额外算法支持）
  Serial.print("RED: ");
  Serial.print(redValue);
  Serial.print(" | IR: ");
  Serial.println(irValue);

  // 触摸检测：如果 IR 光信号足够强，则认为有手指接触
  if (irValue > 50000) { // IR 值阈值，表示是否有手指接触
    Serial.println("Finger detected!");

    // 根据 RED 值动态改变灯带颜色
    int redIntensity = map(redValue, 0, 100000, 0, 255); // 将 RED 值映射到 0-255
    int blueIntensity = 255 - redIntensity; // 补充色

    for (int i = 0; i < NUM_PIXELS; i++) {
      strip.setPixelColor(i, strip.Color(redIntensity, 0, blueIntensity));
    }
    strip.show();
  } else {
    // 如果没有手指接触，关闭灯带
    for (int i = 0; i < NUM_PIXELS; i++) {
      strip.setPixelColor(i, strip.Color(0, 0, 0)); // 全黑
    }
    strip.show();
  }

  delay(100); // 每秒检测 10 次
}
