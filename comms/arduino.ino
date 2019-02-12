#include <Arduino_FreeRTOS.h>
#include <queue.h>
#include <semphr.h>

/*
 *  JZON 1.1 CONSTANTS (DO NOT CUSTOMIZE)
 */
const int MESSAGE_START = 55;
const int MESSAGE_SIZE_NO_DATA = 3;
const int MESSAGE_PACKET_CODE_INDEX_NO_DATA = 1;
const int PACKET_CODE_NACK = 0;
const int PACKET_CODE_ACK = 1;
const int PACKET_CODE_HELLO = 2;
const int PACKET_CODE_READ = 3;
const int PACKET_CODE_WRITE = 4;
const int PACKET_CODE_DATA_RESPONSE = 5;

/*
 * Program Variables
 */
const int NUM_SENSORS = 3;
const int DELAY_INIT_HANDSHAKE = 100;
const int DELAY_SENSOR_READ = 100;
const int DELAY_SEND2RPI = 1;

QueueHandle_t queue;
SemaphoreHandle_t barrierSemaphore;

struct TSensorData {
  char sensorId;
  short aX;
  short aY;
  short aZ;
  short gX;
  short gY;
  short gZ;
};

struct TJZONPacket {
  char start;
  char packetCode;
  char len;
  struct TSensorData sensorData[NUM_SENSORS];
};

void setup() {
  // initialize both serial ports
  // Serial: Debugging console
  Serial.begin(9600);
  // Serial1: TX/RX to RPi
  Serial1.begin(9600);

  queue = xQueueCreate( NUM_SENSORS, sizeof( struct TSensorData ) );
  if(queue == NULL){
    Serial.write("Error creating the queue!\n");
  }

  barrierSemaphore = xSemaphoreCreateCounting( NUM_SENSORS, NUM_SENSORS );
  if(barrierSemaphore == NULL){
    Serial.write("Error creating the semaphore!\n");
  }

  // Now set up four tasks to run independently.
  xTaskCreate(
    Send2Rpi,
    "Send2Rpi",
    256, // Stack size
    NULL,
    2, // priority
    NULL
  );

  xTaskCreate(
    SensorRead,
    "SensorRead1",
    256, // stack size
    (void *) 1,
    1, // priority
    NULL
  );

  xTaskCreate(
    SensorRead,
    "SensorRead2",
    256, // stack size
    (void *) 2,
    1, // priority
    NULL
  );

  xTaskCreate(
    SensorRead,
    "SensorRead3",
    256, // stack size
    (void *) 3,
    1, // priority
    NULL
  );

  // TODO: Initial handshaking protocol
  // Arduino is ready at this point, and initiates a handshake
  Serial.println("Setup complete!");
}

void Send2Rpi(void *pvParameters)  // This is a task.
{
  (void) pvParameters;
  struct TSensorData sensorData;

  for (;;) {
    // Run only if all sensors are ready with data
    if (uxSemaphoreGetCount(barrierSemaphore) == 0) {
      // TODO: Perform write handshake
      //Serial.write("Preparing to write to RPi...\n");
      // Create data packet
      struct TJZONPacket msg;
      msg.start = MESSAGE_START;
      msg.packetCode = PACKET_CODE_DATA_RESPONSE;
      msg.len = NUM_SENSORS;
      for (int i=0;i<NUM_SENSORS;i++) {
        xQueueReceive(queue, &sensorData, portMAX_DELAY);
        msg.sensorData[i] = sensorData;
      }
      char buffer[sizeof(msg)];
      memcpy(buffer, &msg, sizeof(msg));
      sendSerialData(buffer, sizeof(buffer));
      // Release the semaphores
      for (int i=0;i<NUM_SENSORS;i++) {
        xSemaphoreGive(barrierSemaphore);
      }
    }
    vTaskDelay(DELAY_SEND2RPI);  // one tick delay (15ms) in between reads for stability
  }
}

void SensorRead(void *pvParameters)  // This is a task.
{
  struct TSensorData sensorData;
  int sensorId = (uint32_t) pvParameters;
  for (;;)
  {
    // Reserve the semaphore
    xSemaphoreTake(barrierSemaphore, portMAX_DELAY);
    // Read the inputs
    // TODO: @jiahao sensor sampling code
    // e.g. int sensorValue = analogRead(A0);

    // Assemble sensor data packet
    // TODO: Replace with actual variables
    sensorData.sensorId = sensorId;
    sensorData.aX = (short)1*sensorId;
    sensorData.aY = (short)2*sensorId;
    sensorData.aZ = (short)3*sensorId;
    sensorData.gX = (short)4*sensorId;
    sensorData.gY = (short)5*sensorId;
    sensorData.gZ = (short)6*sensorId;
    // Add to inter-task communication queue
    xQueueSend(queue, &sensorData, portMAX_DELAY);
    vTaskDelay(1);  // one tick delay (15ms) in between reads for stability
  }
}

void sendSerialData(char *buffer, int len) {
//  for(int i=0; i<len; i++) {
//    Serial1.write(buffer[i]);
//  }
  Serial1.write(buffer, len);
  Serial1.flush();
  Serial.println("Data sent!");
}

void loop() {
  delay(1000);
}
