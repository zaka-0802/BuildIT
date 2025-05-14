// BuildIT.cpp
#include "BuildIT.h"
#include <Arduino.h>
#include <ESP32Servo.h>
#define MAX_READINGS 5
Servo servo1;
Servo servo2;

// Ultrasonic pin arrays
const int TRIG_PINS[3] = {US_TRIG_1, US_TRIG_2, US_TRIG_3};
const int ECHO_PINS[3] = {US_ECHO_1, US_ECHO_2, US_ECHO_3};

// IR pin array
const int IR_PINS[4] = {IR1_PIN, IR2_PIN, IR3_PIN, IR4_PIN};

// Sensor buffers
volatile float us_values[3][MAX_READINGS] = {0};
volatile int us_idx[3] = {0};
portMUX_TYPE us_mux = portMUX_INITIALIZER_UNLOCKED;

volatile int ir_values[4][MAX_READINGS] = {0};
volatile int ir_idx[4] = {0};
portMUX_TYPE ir_mux = portMUX_INITIALIZER_UNLOCKED;


// ----------- Helper: Measure Single Ultrasonic -------------
float measure_distance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseInLong(echoPin, HIGH, 50000);
  if (duration == 0) {
    duration = pulseInLong(echoPin, HIGH, 50000);
  }

  return (duration > 0) ? (duration * 0.0343 / 2.0) : 0.0;
}

// ----------- Helper: Median Calculation -------------
float median_float(float *arr, int size) {
  float temp[size];
  memcpy(temp, arr, sizeof(float) * size);
  for (int i = 0; i < size - 1; i++) {
    for (int j = i + 1; j < size; j++) {
      if (temp[j] < temp[i]) {
        float t = temp[i];
        temp[i] = temp[j];
        temp[j] = t;
      }
    }
  }
  return temp[size / 2];
}
void us_task(void *param) {
  int us_id = 0;

  while (true) {
    float d = measure_distance(TRIG_PINS[us_id], ECHO_PINS[us_id]);
    portENTER_CRITICAL(&us_mux);
    us_values[us_id][us_idx[us_id]] = d;
    us_idx[us_id] = (us_idx[us_id] + 1) % MAX_READINGS;
    portEXIT_CRITICAL(&us_mux);
    us_id = (us_id + 1) % 3;

    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}
// ----------- Public Sensor Access -------------
float read_ultrasonic_distance(int id) {
  if (id < 0 || id > 2) return -1;
  float copy[MAX_READINGS];
  portENTER_CRITICAL(&us_mux);
  for (int i = 0; i < MAX_READINGS; i++) {
    copy[i] = us_values[id][i];
  }
  portEXIT_CRITICAL(&us_mux);
  return median_float(copy, MAX_READINGS);
}

int median_int(int *arr, int size) {
  int temp[size];
  memcpy(temp, arr, sizeof(int) * size);
  for (int i = 0; i < size - 1; i++) {
    for (int j = i + 1; j < size; j++) {
      if (temp[j] < temp[i]) {
        int t = temp[i];
        temp[i] = temp[j];
        temp[j] = t;
      }
    }
  }
  return temp[size / 2];
}
// ----------- Fast IR Task (High-Frequency) -------------
void fast_ir_task(void *param) {
  while (true) {
    for (int i = 0; i < 4; i++) {
      portENTER_CRITICAL(&ir_mux);
      ir_values[i][ir_idx[i]] = digitalRead(IR_PINS[i]);
      ir_idx[i] = (ir_idx[i] + 1) % MAX_READINGS;
      portEXIT_CRITICAL(&ir_mux);
    }
    vTaskDelay(5 / portTICK_PERIOD_MS); // Fast update rate for IR sensors
  }
}
int read_ir_light(int id) {
  if (id < 0 || id > 3) return -1;
  int copy[MAX_READINGS];
  portENTER_CRITICAL(&ir_mux);
  for (int i = 0; i < MAX_READINGS; i++) {
    copy[i] = ir_values[id][i];
  }
  portEXIT_CRITICAL(&ir_mux);
  return !(median_int(copy, MAX_READINGS));
}

// ----------- Initialization Function -------------
void setup_hardware() {
  // Setup ultrasonic pins
  for (int i = 0; i < 3; i++) {
    pinMode(TRIG_PINS[i], OUTPUT);
    digitalWrite(TRIG_PINS[i], LOW);
    pinMode(ECHO_PINS[i], INPUT);
  }

  // Setup IR pins
  for (int i = 0; i < 4; i++) {
    pinMode(IR_PINS[i], INPUT);
  }

  // Launch tasks
  xTaskCreatePinnedToCore(fast_ir_task, "fast_ir_task", 2048, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(us_task, "us_task", 4096, NULL, 1, NULL, 1);

  pinMode(MOTOR_A_PWM, OUTPUT);
  pinMode(MOTOR_A_IN1, OUTPUT);
  pinMode(MOTOR_A_IN2, OUTPUT);

  pinMode(MOTOR_B_PWM, OUTPUT);
  pinMode(MOTOR_B_IN1, OUTPUT);
  pinMode(MOTOR_B_IN2, OUTPUT);

  // Ensure both motors are stopped initially
  stop_motor(A);
  stop_motor(B);


    // Initialize Color Sensor Pins
  pinMode(TCS_S0, OUTPUT);
  pinMode(TCS_S1, OUTPUT);
  pinMode(TCS_S2, OUTPUT);
  pinMode(TCS_S3, OUTPUT);
  pinMode(TCS_OUT, INPUT);

  // Set TCS3200 to 20% frequency scaling
  digitalWrite(TCS_S0, HIGH);
  digitalWrite(TCS_S1, LOW);
}





// Run motor: Specify motor (A/B), direction (Clockwise/Anticlockwise), and speed (0-255)
void run_motor_at_speed(int motor, int direction, int speed) {
  if (speed < 0) speed = 0;
  if (speed > 255) speed = 255;

  int pwm_pin, in1_pin, in2_pin;
  if (motor == A) {
    pwm_pin = MOTOR_A_PWM;
    in1_pin = MOTOR_A_IN1;
    in2_pin = MOTOR_A_IN2;
  } else if (motor == B) {
    pwm_pin = MOTOR_B_PWM;
    in1_pin = MOTOR_B_IN1;
    in2_pin = MOTOR_B_IN2;
  }
  // Set direction
  if (direction == Clockwise) {
    digitalWrite(in1_pin, HIGH);
    digitalWrite(in2_pin, LOW);
  } 
  else if (direction == Anticlockwise) {
    digitalWrite(in1_pin, LOW);
    digitalWrite(in2_pin, HIGH);
  } 

  // Set speed using PWM
  analogWrite(pwm_pin, speed);
}


// Stop motor: Specify motor (A/B)
void stop_motor(int motor) {
  int pwm_pin, in1_pin, in2_pin;
  if (motor == A) {
    pwm_pin = MOTOR_A_PWM;
    in1_pin = MOTOR_A_IN1;
    in2_pin = MOTOR_A_IN2;
  } else if (motor == B) {
    pwm_pin = MOTOR_B_PWM;
    in1_pin = MOTOR_B_IN1;
    in2_pin = MOTOR_B_IN2;
  } else {
    return;  // Invalid motor
  }

  // Set motor to stop (brake mode)
  digitalWrite(in1_pin, LOW);
  digitalWrite(in2_pin, LOW);
  analogWrite(pwm_pin, 0);
}

void gripper_strenght(int angle) {
  servo1.attach(GRIPPER_PIN);
  angle = constrain(angle, 0, 180); // Limit angle between 0 and 180
  servo1.write(angle);
  Serial.print("Servo 1 set to angle: ");
  Serial.println(angle);
}

// Function to set Servo 2 to the specified angle
void gripper_height(int angle) {
  servo2.attach(HEIGHT_PIN);
  angle = constrain(angle, 0, 180); // Limit angle between 0 and 180
  servo2.write(angle);
  Serial.print("Servo 2 set to angle: ");
  Serial.println(angle);
}


// ----------- Color Sensor Function (TCS3200) -------------
int color_is(String name) {
  digitalWrite(TCS_S2, LOW);
  digitalWrite(TCS_S3, LOW);
  int red = pulseIn(TCS_OUT, LOW);
  delay(10);

  digitalWrite(TCS_S2, HIGH);
  digitalWrite(TCS_S3, HIGH);
  int green = pulseIn(TCS_OUT, LOW);
  delay(10);

  digitalWrite(TCS_S2, LOW);
  digitalWrite(TCS_S3, HIGH);
  int blue = pulseIn(TCS_OUT, LOW);
  delay(10);

  if (red == 0) red = 1;
  if (green == 0) green = 1;
  if (blue == 0) blue = 1;

  int r_val = 255000 / red;
  int g_val = 255000 / green;
  int b_val = 255000 / blue;

  r_val = constrain(r_val, 0, 255);
  g_val = constrain(g_val, 0, 255);
  b_val = constrain(b_val, 0, 255);

  int margin = 25;

  if (name == "Red" && r_val > (g_val + margin) && r_val > (b_val + margin)) return 1;
  if (name == "Orange" && r_val > 150 && g_val > 80 && g_val < 200 && b_val < 80) return 1;
  if (name == "Yellow" && r_val > 150 && g_val > 150 && b_val < 100) return 1;
  if (name == "Green" && g_val > (r_val + margin) && g_val > (b_val + margin)) return 1;
  if (name == "Blue" && b_val > (r_val + margin) && b_val > (g_val + margin)) return 1;
  if (name == "Purple" && b_val > 100 && r_val > 80 && r_val < 150 && g_val < 80) return 1;
  if (name == "Pink" && r_val > 150 && b_val > 150 && g_val < 100) return 1;

  return 0;
}

void take_pic() {
  digitalWrite(CAMERA_TRIGGER_PIN, HIGH);  // Trigger camera
  delay(2500);                            // Maintain HIGH for 2.5 seconds
  digitalWrite(CAMERA_TRIGGER_PIN, LOW);   // Stop trigger
}

bool is_object_detected() {
  digitalWrite(OBJECT_TRIGGER_PIN, HIGH);  // Trigger object detection
  delay(2500);                             // Maintain trigger for 2.5 seconds
  digitalWrite(OBJECT_TRIGGER_PIN, LOW);   // Stop trigger

  // Read object detection status (HIGH = detected, LOW = not detected)
  return digitalRead(OBJECT_DETECT_PIN) == HIGH;
}

