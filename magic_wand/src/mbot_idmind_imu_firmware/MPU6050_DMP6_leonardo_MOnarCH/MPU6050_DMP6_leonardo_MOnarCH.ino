#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    #include "Wire.h"
#endif

MPU6050 mpu;

#define OUTPUT_READABLE_YAWPITCHROLL
#define LED_PIN 13

bool blinkState = false;
bool dmpReady = false;  // set true if DMP init was successful
uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;     // count of all bytes currently in FIFO
uint8_t fifoBuffer[64]; // FIFO storage buffer
unsigned long time,time_currect; //timestamp??
uint8_t sent_number;
uint16_t yaw_aux,roll_aux,pitch_aux;
uint16_t yaw_aux_H,yaw_aux_L,roll_aux_H,roll_aux_L,pitch_aux_H,pitch_aux_L;
uint16_t checksum;
uint8_t Auto; //0 ask-send 1 auto-send
int incomingByte=0x00;

// orientation/motion vars
Quaternion q;           // [w, x, y, z]         quaternion container
VectorInt16 aa;         // [x, y, z]            accel sensor measurements
VectorInt16 aaReal;     // [x, y, z]            gravity-free accel sensor measurements
VectorInt16 aaWorld;    // [x, y, z]            world-frame accel sensor measurements
VectorFloat gravity;    // [x, y, z]            gravity vector
float euler[3];         // [psi, theta, phi]    Euler angle container
float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector

// packet structure for InvenSense teapot demo
uint8_t teapotPacket[14] = { '$', 0x02, 0,0, 0,0, 0,0, 0,0, 0x00, 0x00, '\r', '\n' };



// ================================================================
// ===               INTERRUPT DETECTION ROUTINE                ===
// ================================================================

volatile bool mpuInterrupt = false;     // indicates whether MPU interrupt pin has gone high
void dmpDataReady() {
    mpuInterrupt = true;
}



// ================================================================
// ===                      INITIAL SETUP                       ===
// ================================================================

void setup() {
    // join I2C bus (I2Cdev library doesn't do this automatically)
    #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
        Wire.begin();
        TWBR = 24; // 400kHz I2C clock (200kHz if CPU is 8MHz)
    #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
        Fastwire::setup(400, true);
    #endif
    time=0;
    time_currect=0;
    sent_number=0;

    // initialize serial communication
    // (115200 chosen because it is required for Teapot Demo output, but it's
    // really up to you depending on your project)
    Serial.begin(115200);
    //while (!Serial); // wait for Leonardo enumeration, others continue immediately
    Auto=0;

    // NOTE: 8MHz or slower host processors, like the Teensy @ 3.3v or Ardunio
    // Pro Mini running at 3.3v, cannot handle this baud rate reliably due to
    // the baud timing being too misaligned with processor ticks. You must use
    // 38400 or slower in these cases, or use some kind of external separate
    // crystal solution for the UART timer.

    // initialize device
    //Serial.println(F("Initializing I2C devices..."));
    mpu.initialize();

    // verify connection
    Serial.println(F("Testing device connections..."));
    Serial.println(mpu.testConnection() ? F("MPU6050 connection successful") : F("MPU6050 connection failed"));

    // wait for ready
    Serial.println(F("\nSend any character to begin DMP programming and demo: "));
    
    while (Serial.available() && Serial.read()); // empty buffer
    
    
    while (!Serial.available());                 // wait for data
    while (Serial.available() && Serial.read()); // empty buffer again

    // load and configure the DMP
    Serial.println(F("Initializing DMP..."));
    devStatus = mpu.dmpInitialize();

    // supply your own gyro offsets here, scaled for min sensitivity
    mpu.setXGyroOffset(220);
    mpu.setYGyroOffset(76);
    mpu.setZGyroOffset(-85);
    mpu.setZAccelOffset(1788); // 1688 factory default for my test chip

    // make sure it worked (returns 0 if so)
    if (devStatus == 0) {
        // turn on the DMP, now that it's ready
        Serial.println(F("Enabling DMP..."));
        mpu.setDMPEnabled(true);

        // enable Arduino interrupt detection
        Serial.println(F("Enabling interrupt detection (Arduino external interrupt 2)..."));
//        attachInterrupt(0, dmpDataReady, RISING); //interrupt 0 PIN2 do arduino UNO -> PIN3 do Arduino Leonardo
      //  attachInterrupt(1, dmpDataReady, RISING); //interrupt 1 PIN3 do arduino UNO  -> PIN2 do Arduino Leonardo
        attachInterrupt(digitalPinToInterrupt(2), dmpDataReady, RISING); //interrupt 1 PIN0 do arduino Leonardo
  //      attachInterrupt(3, dmpDataReady, RISING); //interrupt 1 PIN1 do arduino Leonardo
    //    attachInterrupt(4, dmpDataReady, RISING); //interrupt 1 PIN7 do arduino Leonardo
        mpuIntStatus = mpu.getIntStatus();

        // set our DMP Ready flag so the main loop() function knows it's okay to use it
        Serial.println(F("DMP ready! Waiting for first interrupt..."));
        dmpReady = true;

        // get expected DMP packet size for later comparison
        packetSize = mpu.dmpGetFIFOPacketSize();
    } else {
        // ERROR!
        // 1 = initial memory load failed
        // 2 = DMP configuration updates failed
        // (if it's going to break, usually the code will be 1)
       Serial.print(F("DMP Initialization failed (code "));
       Serial.print(devStatus);
       Serial.println(F(")"));
    }

    pinMode(2, INPUT);
    
    // configure LED for output
    pinMode(LED_PIN, OUTPUT);
}



// ================================================================
// ===                    MAIN PROGRAM LOOP                     ===
// ================================================================

void loop() {
    // if programming failed, don't try to do anything
    if (!dmpReady) return;
    
    

    // wait for MPU interrupt or extra packet(s) available
    while (!mpuInterrupt && fifoCount < packetSize) {
        // other program behavior stuff here
        // .
        // .
        // .
        // if you are really paranoid you can frequently test in between other
        // stuff to see if mpuInterrupt is true, and if so, "break;" from the
        // while() loop to immediately process the MPU data
        // .
        // .
        // .
        
        Serial.print("--1--");
        Serial.print((mpuInterrupt));
        Serial.print("\n--2--");
        Serial.print((fifoCount));
        Serial.print("\n--3--");
        Serial.print((packetSize));
    }

    // reset interrupt flag and get INT_STATUS byte
    mpuInterrupt = false;
    mpuIntStatus = mpu.getIntStatus();

    // get current FIFO count
    fifoCount = mpu.getFIFOCount();

    // check for overflow (this should never happen unless our code is too inefficient)
    if ((mpuIntStatus & 0x10) || fifoCount == 1024) {
        // reset so we can continue cleanly
        mpu.resetFIFO();
        Serial.println(F("FIFO overflow!"));

    // otherwise, check for DMP data ready interrupt (this should happen frequently)
    } else if (mpuIntStatus & 0x02) {
        // wait for correct available data length, should be a VERY short wait
        while (fifoCount < packetSize) fifoCount = mpu.getFIFOCount();

        // read a packet from FIFO
        mpu.getFIFOBytes(fifoBuffer, packetSize);
        
        // track FIFO count here in case there is > 1 packet available
        // (this lets us immediately read more without waiting for an interrupt)
        fifoCount -= packetSize;

        #ifdef OUTPUT_READABLE_QUATERNION
            // display quaternion values in easy matrix form: w x y z
            mpu.dmpGetQuaternion(&q, fifoBuffer);
            Serial.print("quat\t");
            Serial.print(q.w);
            Serial.print("\t");
            Serial.print(q.x);
            Serial.print("\t");
            Serial.print(q.y);
            Serial.print("\t");
            Serial.println(q.z);
        #endif

        #ifdef OUTPUT_READABLE_EULER
            // display Euler angles in degrees
            mpu.dmpGetQuaternion(&q, fifoBuffer);
            mpu.dmpGetEuler(euler, &q);
            Serial.print("euler\t");
            Serial.print(euler[0] * 180/M_PI);
            Serial.print("\t");
            Serial.print(euler[1] * 180/M_PI);
            Serial.print("\t");
            Serial.println(euler[2] * 180/M_PI);
        #endif

        #ifdef OUTPUT_READABLE_YAWPITCHROLL
            // display Euler angles in degrees
            mpu.dmpGetQuaternion(&q, fifoBuffer);
            mpu.dmpGetGravity(&gravity, &q);
            mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
            
            yaw_aux=(ypr[0] * 180/M_PI)*100;
            yaw_aux_H=yaw_aux >> 8;
            yaw_aux_L=yaw_aux & 0xFF;
            
            pitch_aux=(ypr[1] * 180/M_PI)*100;
            pitch_aux_H=pitch_aux >> 8;
            pitch_aux_L=pitch_aux & 0xFF;            
            
            roll_aux=(ypr[2] * 180/M_PI)*100;
            roll_aux_H=roll_aux >> 8;
            roll_aux_L=roll_aux & 0xFF;            

            checksum= (uint16_t)0x0040+(uint16_t)yaw_aux_H+(uint16_t)yaw_aux_L+(uint16_t)pitch_aux_H+(uint16_t)pitch_aux_L+(uint16_t)roll_aux_H+(uint16_t)roll_aux_L+(uint16_t)sent_number;
            
            
            if (Serial.available())
            {
              incomingByte=Serial.read();
              {
                if (incomingByte==0x50)
                {
                  while(!Serial.available());
                  incomingByte=Serial.read();
                  {
                    if (incomingByte==0x00)
                    {
                     Auto=0;                       
                    }
                    else if (incomingByte==0x01)
                    {
                      Auto=1;                      
                    }
                    else if (incomingByte==0x02)
                    {
                      Auto=3;                          
                    }
                  }
                  checksum=(uint16_t)0x0050+(uint16_t)sent_number;
                  Serial.write(0x50);
                  Serial.write((unsigned char) sent_number);
                  Serial.write((unsigned char) (checksum >> 8));
                  Serial.write((unsigned char) (checksum & 0xFF));
                  sent_number++;                  
                 }
                 else if (incomingByte==0x40 && Auto==3)
                 {
                   Auto=2;                  
                 }                
              }
            }            
            else if (Auto==1 || Auto==2)
            { 
              Serial.write(0x40);
              Serial.write((unsigned char) yaw_aux_H);
              Serial.write((unsigned char) yaw_aux_L);
              Serial.write((unsigned char) pitch_aux_H);
              Serial.write((unsigned char) pitch_aux_L);
              Serial.write((unsigned char) roll_aux_H);
              Serial.write((unsigned char) roll_aux_L);
              Serial.write((unsigned char) sent_number);
              Serial.write((unsigned char) (checksum >> 8));
              Serial.write((unsigned char) (checksum & 0xFF));
              sent_number++;
              if (Auto==2) Auto=3;
            }
//            Serial.println("\n");

         /*   time=millis();
            Serial.print(time-time_currect); 
            Serial.print("ypr\t");
            Serial.print(ypr[0] * 180/M_PI);
            Serial.print("\t");
            Serial.print(ypr[1] * 180/M_PI);
            Serial.print("\t");
            Serial.print(ypr[2] * 180/M_PI);
            Serial.print("\t");
            Serial.print(sent_number);
            Serial.print("\t");
            Serial.println(ypr[0] * 180/M_PI+ypr[1] * 180/M_PI+ypr[2] * 180/M_PI+(uint16_t)sent_number);*/

            
            
            if (time-time_currect>=1000) time_currect=time;
           
        #endif

        #ifdef OUTPUT_READABLE_REALACCEL
            // display real acceleration, adjusted to remove gravity
            mpu.dmpGetQuaternion(&q, fifoBuffer);
            mpu.dmpGetAccel(&aa, fifoBuffer);
            mpu.dmpGetGravity(&gravity, &q);
            mpu.dmpGetLinearAccel(&aaReal, &aa, &gravity);
            Serial.print("areal\t");
            Serial.print(aaReal.x);
            Serial.print("\t");
            Serial.print(aaReal.y);
            Serial.print("\t");
            Serial.println(aaReal.z);
        #endif

        #ifdef OUTPUT_READABLE_WORLDACCEL
            // display initial world-frame acceleration, adjusted to remove gravity
            // and rotated based on known orientation from quaternion
            mpu.dmpGetQuaternion(&q, fifoBuffer);
            mpu.dmpGetAccel(&aa, fifoBuffer);
            mpu.dmpGetGravity(&gravity, &q);
            mpu.dmpGetLinearAccel(&aaReal, &aa, &gravity);
            mpu.dmpGetLinearAccelInWorld(&aaWorld, &aaReal, &q);
            Serial.print("aworld\t");
            Serial.print(aaWorld.x);
            Serial.print("\t");
            Serial.print(aaWorld.y);
            Serial.print("\t");
            Serial.println(aaWorld.z);
        #endif
    
        #ifdef OUTPUT_TEAPOT
            // display quaternion values in InvenSense Teapot demo format:
            teapotPacket[2] = fifoBuffer[0];
            teapotPacket[3] = fifoBuffer[1];
            teapotPacket[4] = fifoBuffer[4];
            teapotPacket[5] = fifoBuffer[5];
            teapotPacket[6] = fifoBuffer[8];
            teapotPacket[7] = fifoBuffer[9];
            teapotPacket[8] = fifoBuffer[12];
            teapotPacket[9] = fifoBuffer[13];
            Serial.write(teapotPacket, 14);
            teapotPacket[11]++; // packetCount, loops at 0xFF on purpose
        #endif

        // blink LED to indicate activity
        blinkState = !blinkState;
        digitalWrite(LED_PIN, blinkState);
    }
}
