#ifndef BUILDIT_H
#define BUILDIT_H

#include <Arduino.h>
// Sensor labels
#define A 0
#define B 1
#define C 2
#define D 3

// Direction labels
#define Clockwise 1
#define Anticlockwise 0

// IR Sensors
#define IR1_PIN 4
#define IR2_PIN 5
#define IR3_PIN 6
#define IR4_PIN 7

// Ultrasonic Sensors
#define US_TRIG_1 15
#define US_ECHO_1 16
#define US_TRIG_2 17
#define US_ECHO_2 18
#define US_TRIG_3 8
#define US_ECHO_3 3

// Motor Driver
#define MOTOR_A_PWM 42
#define MOTOR_A_IN1 41
#define MOTOR_A_IN2 40
#define MOTOR_B_PWM 39
#define MOTOR_B_IN1 38
#define MOTOR_B_IN2 37

// Servo Control
#define GRIPPER_PIN 36
#define HEIGHT_PIN 35

// Color Sensor
#define TCS_S0 9
#define TCS_S1 10
#define TCS_S2 11
#define TCS_S3 12
#define TCS_OUT 13

// Camera and Object Detection
#define CAMERA_TRIGGER_PIN 19
#define OBJECT_TRIGGER_PIN 20
#define OBJECT_DETECT_PIN 21

void setup_hardware();
float read_ultrasonic_distance(int id);
int read_ir_light(int id);
void run_motor_at_speed(int motor, int direction, int speed);
void gripper_strenght(int value);
void gripper_height(int value);
int color_is(String name);
void stop_motor(int motor);
void take_pic();
bool is_object_detected();

#endif
