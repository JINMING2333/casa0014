#include <Wire.h>
#include "MAX30105.h"         // SparkFun MAX30102 库
#include <Adafruit_NeoPixel.h> // 灯带控制库

#define LED_PIN 6            // 灯带数据引脚
#define NUM_PIXELS 30        // 灯带上 LED 数量

MAX30105 particleSensor;
Adafruit_NeoPixel strip(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

unsigned long lastBeatTime = 0;   // 上一次心跳时间
float beatThreshold = 0;         // 动态心跳检测阈值
float irSignal = 0;              // 当前红外信号值
float previousSignal = 0;        // 上一次红外信号值
bool isBeat = false;             // 当前是否检测到心跳

void setup() {
  Serial.begin(115200);

  // 初始化灯带
  strip.begin();
  strip.show();  // 清除所有灯光
  strip.setBrightness(50); // 设置灯带亮度

  // 初始化 MAX30102
  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println("MAX30102 not detected. Check connections!");
    while (1);
  }
  particleSensor.setup(); // 默认传感器配置
  Serial.println("MAX30102 initialized.");

  // 初始化动态阈值
  beatThreshold = 50000; // 根据实际情况调整
}

void loop() {
  // 读取红外光信号
  irSignal = particleSensor.getIR();

  // 检测是否有手指接触
  if (irSignal < 50000) { // 红外信号小于阈值时表示未检测到手指
    Serial.println("No finger detected.");
    turnOffLED();
    delay(100);
    return;
  }

  // 检测心跳（简单波峰检测）
  if (irSignal > beatThreshold && irSignal > previousSignal) {
    // 如果当前信号大于动态阈值且是波峰，判断为一次心跳
    if (!isBeat) {
      isBeat = true; // 标记心跳
      unsigned long currentTime = millis();
      unsigned long beatDuration = currentTime - lastBeatTime;
      lastBeatTime = currentTime;

      // 计算心率（bpm）
      float heartRate = 60000.0 / beatDuration; // 每分钟跳动次数
      Serial.print("Heart Rate: ");
      Serial.print(heartRate);
      Serial.println(" bpm");

      // 控制灯带闪烁
      pulseLED();
    }
  } else if (irSignal < beatThreshold) {
    // 如果信号下降，心跳结束
    isBeat = false;
  }

  // 更新动态阈值
  beatThreshold = (beatThreshold * 0.9) + (irSignal * 0.1); // 平滑调整阈值
  previousSignal = irSignal; // 更新上一次信号值

  delay(20); // 稳定信号读取
}

// 灯带点亮
void pulseLED() {
  for (int i = 0; i < NUM_PIXELS; i++) {
    strip.setPixelColor(i, strip.Color(255, 0, 0)); // 红色灯光
  }
  strip.show();
  delay(100); // 保持亮一段时间

  // 关闭灯带
  turnOffLED();
}

// 灯带熄灭
void turnOffLED() {
  for (int i = 0; i < NUM_PIXELS; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0)); // 关闭所有灯光
  }
  strip.show();
}
