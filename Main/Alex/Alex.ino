#include <serialize.h>
#include <math.h>
#include "packet.h"
#include "constants.h"
#include "NewPing.h"

/*
 * Alex's configuration constants
 */

 typedef enum {
  STOP = 0,
  FORWARD = 1,
  BACKWARD = 2,
  LEFT = 3,
  RIGHT = 4
 } TDirection;

 volatile TDirection dir = STOP;

//Pins for ultrasonic sensor
#define TRIGGER_PIN 13
#define ECHO_PIN 12
#define MAX_DISTANCE 200
unsigned int Distance;
unsigned int scanDistance = 99;
bool brake = false;

// NewPing setup of pins and maximum distance
NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE); 

// Number of ticks per revolution from the 
// wheel encoder. The higher it is the more the wheel turns to meet distance

#define COUNTS_PER_REV      100
#define COUNTS_PER_REV_TURN 35

// Wheel circumference in cm.
// We will use this to calculate forward/backward distance traveled 
// by taking revs * WHEEL_CIRC

#define WHEEL_CIRC          20.420

// Motor control pins. You need to adjust these till
// Alex moves in the correct direction
#define LF                  6   // Left forward pin
#define LR                  5   // Left reverse pin
#define RF                  11  // Right forward pin
#define RR                  10  // Right reverse pin

//Pi, for calculating turn circumference
//#define PI 3.141592654 //got warning when compiling, but its fine

//Alex's length and breadth in cm
//Numbers here are currently placeholders, must measure later

#define ALEX_LENGTH 30 //This includes wires
#define ALEX_BREADTH 13

//Alex's diagonal. We compute and store this once
//since it is expensive to compute and really doesn't change.
float AlexDiagonal = 0.0;

//Alex's turning circumference, calculated once
float AlexCirc = 0.0;
/*
 *    Alex's State Variables
 */

// Store the ticks from Alex's left and
// right encoders.
volatile unsigned long leftForwardTicks; 
volatile unsigned long rightForwardTicks;
volatile unsigned long leftReverseTicks; 
volatile unsigned long rightReverseTicks;

volatile unsigned long leftForwardTicksTurns; 
volatile unsigned long rightForwardTicksTurns;
volatile unsigned long leftReverseTicksTurns; 
volatile unsigned long rightReverseTicksTurns;

// Store the revolutions on Alex's left
// and right wheels
volatile unsigned long leftRevs;
volatile unsigned long rightRevs;

// Forward and backward distance traveled
volatile unsigned long forwardDist;
volatile unsigned long reverseDist;
volatile unsigned long leftForwardDist;
volatile unsigned long leftReverseDist;
volatile unsigned long rightForwardDist;
volatile unsigned long rightReverseDist;

unsigned long deltaDist;
unsigned long newDist;

unsigned long deltaTicks;
unsigned long targetTicks;


/*
 * 
 * Alex Communication Routines.
 * 
 */
 
TResult readPacket(TPacket *packet)
{
    // Reads in data from the serial port and
    // deserializes it.Returns deserialized
    // data in "packet".
    
    char buffer[PACKET_SIZE];
    int len;

    len = readSerial(buffer);

    if(len == 0)
      return PACKET_INCOMPLETE;
    else
      return deserialize(buffer, len, packet);
    
}

void sendStatus()
{
  // Implement code to send back a packet containing key
  // information like leftTicks, rightTicks, leftRevs, rightRevs
  // forwardDist and reverseDist
  // Use the params array to store this information, and set the
  // packetType and command files accordingly, then use sendResponse
  // to send out the packet. See sendMessage on how to use sendResponse.
  //

  TPacket statusPacket;
  statusPacket.packetType = PACKET_TYPE_RESPONSE;
  statusPacket.command = RESP_STATUS;

  statusPacket.params[0] = leftForwardTicks;
  statusPacket.params[1] = rightForwardTicks;
  statusPacket.params[2] = leftReverseTicks;
  statusPacket.params[3] = rightReverseTicks;
  statusPacket.params[4] = leftForwardTicksTurns;
  statusPacket.params[5] = rightForwardTicksTurns;
  statusPacket.params[6] = leftReverseTicksTurns;
  statusPacket.params[7] = rightReverseTicksTurns;
  statusPacket.params[8] = forwardDist;
  statusPacket.params[9] = reverseDist;

  sendResponse(&statusPacket);
}

void sendMessage(const char *message)
{
  // Sends text messages back to the Pi. Useful
  // for debugging.
  
  TPacket messagePacket;
  messagePacket.packetType=PACKET_TYPE_MESSAGE;
  strncpy(messagePacket.data, message, MAX_STR_LEN);
  sendResponse(&messagePacket);
}

void sendBadPacket()
{
  // Tell the Pi that it sent us a packet with a bad
  // magic number.
  
  TPacket badPacket;
  badPacket.packetType = PACKET_TYPE_ERROR;
  badPacket.command = RESP_BAD_PACKET;
  sendResponse(&badPacket);
  
}

void sendBadChecksum()
{
  // Tell the Pi that it sent us a packet with a bad
  // checksum.
  
  TPacket badChecksum;
  badChecksum.packetType = PACKET_TYPE_ERROR;
  badChecksum.command = RESP_BAD_CHECKSUM;
  sendResponse(&badChecksum);  
}

void sendBadCommand()
{
  // Tell the Pi that we don't understand its
  // command sent to us.
  
  TPacket badCommand;
  badCommand.packetType=PACKET_TYPE_ERROR;
  badCommand.command=RESP_BAD_COMMAND;
  sendResponse(&badCommand);

}

void sendBadResponse()
{
  TPacket badResponse;
  badResponse.packetType = PACKET_TYPE_ERROR;
  badResponse.command = RESP_BAD_RESPONSE;
  sendResponse(&badResponse);
}

void sendOK()
{
  TPacket okPacket;
  okPacket.packetType = PACKET_TYPE_RESPONSE;
  okPacket.command = RESP_OK;
  sendResponse(&okPacket);  
}

void sendResponse(TPacket *packet)
{
  // Takes a packet, serializes it then sends it out
  // over the serial port.
  char buffer[PACKET_SIZE];
  int len;

  len = serialize(buffer, packet, sizeof(TPacket));
  writeSerial(buffer, len);
}


/*
 * Setup and start codes for external interrupts and 
 * pullup resistors.
 * 
 */
// Enable pull up resistors on pins 2 and 3
void enablePullups()
{
  // Use bare-metal to enable the pull-up resistors on pinsf
  // 2 and 3. These are pins PD2 and PD3 respectively.
  // We set bits 2 and 3 in DDRD to 0 to make them inputs. 

  DDRD  &= 0b11110011;
  PORTD |= 0b00001100;
}

// Functions to be called by INT0 and INT1 ISRs.
void leftISR()
{
  if (dir == LEFT) {
    leftReverseTicksTurns++;
  }
  if (dir == RIGHT) {
    leftForwardTicksTurns++;
  }
  if(dir == FORWARD) {
    leftForwardTicks++;
    leftForwardDist = (unsigned long) ((float) leftForwardTicks / COUNTS_PER_REV * WHEEL_CIRC);
  }
  if(dir == BACKWARD) {
    leftReverseTicks++;
    leftReverseDist = (unsigned long) ((float) leftReverseTicks / COUNTS_PER_REV * WHEEL_CIRC);
  }
  leftRevs = leftForwardTicks / COUNTS_PER_REV;

  // We calculate forwardDist only in leftISR because we
  // assume that the left and right wheels move at the same
  // time.
  leftForwardDist = leftRevs * WHEEL_CIRC;
  
//  Serial.print("LEFT: ");
//  Serial.println(leftForwardTicks);
}

void rightISR()
{
  if (dir == LEFT) { 
    rightForwardTicksTurns++;
  }
  if (dir == RIGHT) {
    rightReverseTicksTurns++;
  }
  if(dir == FORWARD) {
    rightForwardTicks++;
    rightForwardDist = (unsigned long) ((float) rightForwardTicks / COUNTS_PER_REV * WHEEL_CIRC);
  }
  if(dir == BACKWARD) {
    rightReverseTicks++;
    rightReverseDist = (unsigned long) ((float) rightReverseTicks / COUNTS_PER_REV * WHEEL_CIRC);
  }

  rightRevs = rightForwardTicks / COUNTS_PER_REV;
  //Serial.print("RIGHT: ");
  //Serial.println(rightForwardTicks);
  rightForwardDist = rightRevs * WHEEL_CIRC;
}

// Set up the external interrupt pins INT0 and INT1
// for falling edge triggered. Use bare-metal.
void setupEINT()
{
  // Use bare-metal to configure pins 2 and 3 to be
  // falling edge triggered. Remember to enable
  // the INT0 and INT1 interrupts.

  EICRA = 0b00001010;
  EIMSK = 0b00000011;
}

// Implement the external interrupt ISRs below.
// INT0 ISR should call leftISR while INT1 ISR
// should call rightISR.

ISR(INT0_vect)
{
  leftISR();
}

ISR(INT1_vect)
{
  rightISR();
}


// Implement INT0 and INT1 ISRs above.

/*
 * Setup and start codes for serial communications
 * 
 */
// Set up the serial connection. For now we are using 
// Arduino Wiring, you will replace this later
// with bare-metal code.
void setupSerial()
{
  // To replace later with bare-metal.
  Serial.begin(57600);
}

// Start the serial connection. For now we are using
// Arduino wiring and this function is empty. We will
// replace this later with bare-metal code.

void startSerial()
{
  // Empty for now. To be replaced with bare-metal code
  // later on.
  
}

// Read the serial port. Returns the read character in
// ch if available. Also returns TRUE if ch is valid. 
// This will be replaced later with bare-metal code.

int readSerial(char *buffer)
{

  int count=0;

  while(Serial.available())
    buffer[count++] = Serial.read();

  return count;
}

// Write to the serial port. Replaced later with
// bare-metal code

void writeSerial(const char *buffer, int len)
{
  Serial.write(buffer, len);
}

/*
 * Alex's motor drivers.
 * 
 */

// Set up Alex's motors. Right now this is empty, but
// later you will replace it with code to set up the PWMs
// to drive the motors.
void setupMotors()
{
  /* Our motor set up is:  
   *    A1IN - Pin 5, PD5, OC0B
   *    A2IN - Pin 6, PD6, OC0A
   *    B1IN - Pin 10, PB2, OC1B
   *    B2In - pIN 11, PB3, OC2A
   */
}

// Start the PWM for Alex's motors.
// We will implement this later. For now it is
// blank.
void startMotors()
{
  
}

// Convert percentages to PWM values
int pwmVal(float speed)
{
  if(speed < 0.0)
    speed = 0;

  if(speed > 100.0)
    speed = 100.0;

  return (int) ((speed / 100.0) * 255.0);
}

// Move Alex forward "dist" cm at speed "speed".
// "speed" is expressed as a percentage. E.g. 50 is
// move forward at half speed.
// Specifying a distance of 0 means Alex will
// continue moving forward indefinitely.
void forward(float dist, float speed)
{
  if (dist > 0) deltaDist = dist;
  else deltaDist = 999999;

  newDist = leftForwardDist + deltaDist;
  int val = pwmVal(100);
  dir = FORWARD;
  // For now we will ignore dist and move
  // forward indefinitely. We will fix this
  // in Week 9.

  // LF = Left forward pin, LR = Left reverse pin
  // RF = Right forward pin, RR = Right reverse pin
  // This will be replaced later with bare-metal code.
  analogWrite(LF, val);
  analogWrite(RF, val);
  analogWrite(LR,0);
  analogWrite(RR, 0);
  if (leftForwardDist > newDist || dir == STOP) {
      analogWrite(LF, 0);
      analogWrite(RF, 0);
      analogWrite(LR, 255);
      analogWrite(RR, 255);
      int temp = deltaDist/1.5;
      if (temp > 60) temp = 60;
      if (temp <= 10) temp = 0;
      delay(temp); 
  }
}

// Reverse Alex "dist" cm at speed "speed".
// "speed" is expressed as a percentage. E.g. 50 is
// reverse at half speed.
// Specifying a distance of 0 means Alex will
// continue reversing indefinitely.
void reverse(float dist, float speed)
{
  if (dist == 0) deltaDist = 999999;
  else deltaDist = dist;

  newDist = leftReverseDist + deltaDist;
  
  dir = BACKWARD;
  int val = pwmVal(speed);

  // For now we will ignore dist and 
  // reverse indefinitely. We will fix this
  // in Week 9.

  // LF = Left forward pin, LR = Left reverse pin
  // RF = Right forward pin, RR = Right reverse pin
  // This will be replaced later with bare-metal code.
  analogWrite(LR, val);
  analogWrite(RR, val);
  analogWrite(LF, 0);
  analogWrite(RF, 0);
}

//New function to esimate number of wheel ticks
//needed to turn an angle
unsigned long computeDeltaTicks(float ang)
{
  //We will assume that angular distance moved = linear distance moved in one wheel
  //revolution. THis is (probably) incorrect but simplifies calculation.
  //# of wheel revs to make one full 360 turn is AlexCirc/ WHEEL_CIRC
  //This is for 260 degrees. For ang degrees it will be (ang * AlexCirc) / (360 * WHEEL_CIRC)
  //To convert to ticks, we multiply by COUNTS_PER_REV.

  unsigned long ticks = (unsigned long) ((ang * AlexCirc * COUNTS_PER_REV_TURN) / (360.0 * WHEEL_CIRC));

  return ticks;
}

// Turn Alex left "ang" degrees at speed "speed".
// "speed" is expressed as a percentage. E.g. 50 is
// turn left at half speed.
// Specifying an angle of 0 degrees will cause Alex to
// turn left indefinitely.
void left(float ang, float speed)
{
  int val = pwmVal(speed);
  dir = LEFT;

  if (ang == 0) deltaTicks = 9999999;
  else deltaTicks = computeDeltaTicks(ang);

  targetTicks = leftReverseTicksTurns + deltaTicks;
  
  // For now we will ignore ang. We will fix this in Week 9.
  // We will also replace this code with bare-metal later.
  // To turn left we reverse the left wheel and move
  // the right wheel forward.
  analogWrite(LR, val);
  analogWrite(RF, val);
  analogWrite(LF, 0);
  analogWrite(RR, 0);
}

// Turn Alex right "ang" degrees at speed "speed".
// "speed" is expressed as a percentage. E.g. 50 is
// turn left at half speed.
// Specifying an angle of 0 degrees will cause Alex to
// turn right indefinitely.
void right(float ang, float speed)
{
  int val = pwmVal(speed);
  dir = RIGHT;

  if (ang == 0) deltaTicks = 9999999;
  else deltaTicks = computeDeltaTicks(ang);

  targetTicks = rightReverseTicksTurns + deltaTicks;
  
  // For now we will ignore ang. We will fix this in Week 9.
  // We will also replace this code with bare-metal later.
  // To turn right we reverse the right wheel and move
  // the left wheel forward.
  analogWrite(RR, val);
  analogWrite(LF, val);
  analogWrite(LR, 0);
  analogWrite(RF, 0);
}

// Stop Alex. To replace with bare-metal code later.
void stop()
{
  dir = STOP;
  analogWrite(LF, 0);
  analogWrite(LR, 0);
  analogWrite(RF, 0);
  analogWrite(RR, 0);
}

/*
 * Alex's setup and run codes
 * 
 */

// Clears all our counters
void clearCounters()
{
  leftForwardTicks=0;
  rightForwardTicks=0;
  leftReverseTicks=0;
  rightReverseTicks=0;
  leftForwardTicksTurns=0; 
  rightForwardTicksTurns=0;
  leftReverseTicksTurns=0; 
  rightReverseTicksTurns=0;
  leftRevs=0;
  rightRevs=0;
  forwardDist=0;
  reverseDist=0;
  leftForwardDist=0;
  rightForwardDist=0;
  leftReverseDist=0;
  rightReverseDist=0; 
}

// Clears one particular counter
void clearOneCounter(int which)
{
  clearCounters();
}
// Intialize Vincet's internal states

void initializeState()
{
  clearCounters();
}

void handleCommand(TPacket *command)
{
  switch(command->command)
  {
    // For movement commands, param[0] = distance, param[1] = speed.
    case 
      COMMAND_FORWARD:
        sendOK();
        //forward((float) command->params[0], (float) command->params[1]);
        forward((float) command->params[0], 100);
        break;
    case
      COMMAND_REVERSE:
        sendOK();
        //reverse((float) command->params[0], (float) command->params[1]);
        reverse((float) command->params[0], 100);
        break;
    case
      COMMAND_TURN_LEFT:
        sendOK();
        //left((float) command->params[0], (float) command->params[1]);
        left((float) command->params[0], 100);
        break;
    case
      COMMAND_TURN_RIGHT:
        sendOK();
        //right((float) command->params[0], (float) command->params[1]);
        right((float) command->params[0], 100);
        break;
    case
      COMMAND_STOP:
        sendOK();
        stop();
      break;
    case
      COMMAND_GET_STATS:
        sendStatus();
      break;
    case 
      COMMAND_CLEAR_STATS:
        sendOK();
        clearOneCounter(command->params[0]);
      break;


    /*
     * Implement code for other commands here.
     * 
     */
        
    default:
      sendBadCommand();
  }
}

void waitForHello()
{
  int exit=0;

  while(!exit)
  {
    TPacket hello;
    TResult result;
    
    do
    {
      result = readPacket(&hello);
    } while (result == PACKET_INCOMPLETE);

    if(result == PACKET_OK)
    {
      if(hello.packetType == PACKET_TYPE_HELLO)
      {
     

        sendOK();
        exit=1;
      }
      else
        sendBadResponse();
    }
    else
      if(result == PACKET_BAD)
      {
        sendBadPacket();
      }
      else
        if(result == PACKET_CHECKSUM_BAD)
          sendBadChecksum();
  } // !exit
}

void setup() {
  // put your setup code here, to run once:
  AlexDiagonal = sqrt((ALEX_LENGTH * ALEX_LENGTH) + (ALEX_BREADTH * ALEX_BREADTH));
  AlexCirc = PI * AlexDiagonal;
  
  cli();
  setupEINT();
  setupSerial();
  startSerial();
  setupMotors();
  startMotors();
  enablePullups();
  initializeState();
  sei();
  //Serial.begin(9600);
}

void handlePacket(TPacket *packet)
{
  switch(packet->packetType)
  {
    case PACKET_TYPE_COMMAND:
      handleCommand(packet);
      break;

    case PACKET_TYPE_RESPONSE:
      break;

    case PACKET_TYPE_ERROR:
      break;

    case PACKET_TYPE_MESSAGE:
      break;

    case PACKET_TYPE_HELLO:
      break;
  }
}

void DistanceSensor()
{
   Distance = 0;
   for (int i = 0; i < 5; i++) {
    delay(5);
    Distance += sonar.ping_cm();
   }
   scanDistance = Distance/5;
   //Serial.print(Distance);
   //Serial.println("cm");
}

void loop() {

 // put your main code here, to run repeatedly:
  TPacket recvPacket; // This holds commands from the Pi

  TResult result = readPacket(&recvPacket);
  
  if(result == PACKET_OK)
    handlePacket(&recvPacket);
  else
    if(result == PACKET_BAD)
    {
      sendBadPacket();
    }
    else
      if(result == PACKET_CHECKSUM_BAD)
      {
        sendBadChecksum();
      } 

  
  if(deltaDist > 0) 
  { 
    if(dir==FORWARD) 
    { 
      
      while((leftForwardDist <= newDist) && (rightForwardDist <= newDist)) {
        scanDistance = 99;
        DistanceSensor();
        if (scanDistance <= 5 && scanDistance > 0|| dir == STOP) {
          analogWrite(LF, 0);
          analogWrite(RF, 0);
          analogWrite(LR, 255-30);
          analogWrite(RR, 255);
          int temp = deltaDist/1.8;
          if (temp > 60) temp = 60;
          if (temp <= 10) temp = 0;
          delay(temp);
          break;
        }
      }
      
      //if((leftForwardDist > newDist) && (rightForwardDist > newDist))
      { 
        delay(60); 
        deltaDist=0; 
        newDist=0; 
        stop();
      } 
    } 
    else if(dir == BACKWARD) 
    { 
      if((leftReverseDist > newDist) && (rightReverseDist > newDist))
      { 
        deltaDist=0; 
        newDist=0; 
        stop(); 
      } 
    } 
    else if(dir == STOP) 
    { 
      deltaDist=0; 
      newDist=0; 
      stop(); 
    } 
  }

  if (deltaTicks > 0)
  {
    if (dir == LEFT)
    {
      if (leftReverseTicksTurns >= targetTicks)
      {
        deltaTicks = 0;
        targetTicks = 0;
        stop();
      }
    }
    else if (dir == RIGHT)
    {
      if (rightReverseTicksTurns >= targetTicks)
      {
        deltaTicks = 0;
        targetTicks = 0;
        stop();
      }
    }
    else if (dir == STOP)
    {
      deltaTicks = 0;
      targetTicks = 0;
      stop();
    }
  }
}

