#include <Arduino.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "AzureIoTHub.h"
#include <AzureIoTProtocol_MQTT.h>
#include "AzureIoTHubClient.h"
#include <azure_c_shared_utility/platform.h>

#define MIN_EPOCH 40 * 365 * 24 * 3600

BEGIN_NAMESPACE(LightSensorLogger);

DECLARE_MODEL(LightSensorData,
WITH_DATA(ascii_char_ptr, DeviceId),
WITH_DATA(int, LightIntensity),
WITH_ACTION(ResetDevice)
);

END_NAMESPACE(LightSensorLogger);

LightSensorData* g_lightSensorData;

const char *connectionString="<Your device connection string>";
bool g_bShouldReset = false;
bool g_bInitialized = false;
int g_resetCommandTime;
IOTHUB_CLIENT_LL_HANDLE g_iotHubClientHandle;

EXECUTE_COMMAND_RESULT ResetDevice(LightSensorData* device)
{
    (void)device;
    (void)printf("Resetting the device.\r\n");
    g_bShouldReset = true;
    g_resetCommandTime = millis();
    return EXECUTE_COMMAND_SUCCESS;
}

void sendCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void* userContextCallback)
{
    unsigned int messageTrackingId = (unsigned int)(uintptr_t)userContextCallback;

    (void)printf("Message Id: %u Received.\r\n", messageTrackingId);

    (void)printf("Result Call Back Called! Result is: %s \r\n", ENUM_TO_STRING(IOTHUB_CLIENT_CONFIRMATION_RESULT, result));
}

static void sendMessage(IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle, const unsigned char* buffer, size_t size, LightSensorData *lightSensorData)
{
    static unsigned int messageTrackingId;
    IOTHUB_MESSAGE_HANDLE messageHandle = IoTHubMessage_CreateFromByteArray(buffer, size);
    if (messageHandle == NULL)
    {
        printf("unable to create a new IoTHubMessage\r\n");
        return;
    }
    //else
    
    if (IoTHubClient_LL_SendEventAsync(iotHubClientHandle, messageHandle, sendCallback, (void*)(uintptr_t)messageTrackingId) != IOTHUB_CLIENT_OK)
    {
        printf("failed to hand over the message to IoTHubClient");
    }
    else
    {
        printf("IoTHubClient accepted the message for delivery\r\n");
    }
    IoTHubMessage_Destroy(messageHandle);
    messageTrackingId++;
}

static IOTHUBMESSAGE_DISPOSITION_RESULT IoTHubMessage(IOTHUB_MESSAGE_HANDLE message, void* userContextCallback)
{
    const unsigned char* buffer;
    size_t size;
    if (IoTHubMessage_GetByteArray(message, &buffer, &size) != IOTHUB_MESSAGE_OK)
    {
        (void)printf("unable to IoTHubMessage_GetByteArray\r\n");
    }
    else
    {
        /*buffer is not zero terminated*/
        char* temp = (char *)malloc(size + 1);
        if (temp == NULL)
        {
            (void)printf("failed to malloc\r\n");
        }
        else
        {
            (void)memcpy(temp, buffer, size);
            temp[size] = '\0';
            EXECUTE_COMMAND_RESULT executeCommandResult = EXECUTE_COMMAND(userContextCallback, temp);
            if (executeCommandResult != EXECUTE_COMMAND_SUCCESS)
            {
                (void)printf("Execute command failed\r\n");
            }
            free(temp);
        }
    }
    // MQTT can only accept messages
    return IOTHUBMESSAGE_ACCEPTED;
}


void SendToCloud(int sample)
{
    if (!g_bInitialized)
      return;
    static unsigned int messageTrackingId;

     if (IoTHubClient_LL_SetMessageCallback(g_iotHubClientHandle, IoTHubMessage, g_lightSensorData) != IOTHUB_CLIENT_OK)
     {
        printf("unable to IoTHubClient_SetMessageCallback\r\n");
        return;
     }
     //else
     g_lightSensorData->DeviceId = "AlonsLightSensorDevice";
     g_lightSensorData->LightIntensity = sample;
                       
     unsigned char* destination;
     size_t destinationSize;
     if (SERIALIZE(&destination, &destinationSize, g_lightSensorData->DeviceId, g_lightSensorData->LightIntensity) != CODEFIRST_OK)
     {
        (void)printf("Failed to serialize\r\n");
        return;
     }
     //else
     sendMessage(g_iotHubClientHandle, destination, destinationSize, g_lightSensorData);
     free(destination);
}



static void initTime() 
{  
   time_t epochTime;

   configTime(0, 0, "pool.ntp.org", "time.nist.gov");

   while (true) {
       epochTime = time(NULL);

       if (epochTime < MIN_EPOCH) {
           Serial.println("Fetching NTP epoch time failed! Waiting 2 seconds to retry.");
           delay(2000);
       } else {
           Serial.print("Fetched NTP epoch time is: ");
           Serial.println(epochTime);
           break;
       }
   }
}

void AzureIoTHubInitialize()
{
  if (!g_bInitialized)
  {
    initTime();
    if (platform_init() != 0)
    {
        (void)printf("Failed to initialize platform.\r\n");
        return;
    }
    //else
    if (serializer_init(NULL) != SERIALIZER_OK)
    {
      (void)printf("Failed on serializer_init\r\n");
      platform_deinit();
      return;
    }
    //else
    g_iotHubClientHandle = IoTHubClient_LL_CreateFromConnectionString(connectionString, MQTT_Protocol);
    if (g_iotHubClientHandle == NULL)
    {
      (void)printf("Failed on IoTHubClient_LL_Create\r\n");
      serializer_deinit();
      platform_deinit();
      return;
    }
    g_lightSensorData = CREATE_MODEL_INSTANCE(LightSensorLogger, LightSensorData);
    if (g_lightSensorData == nullptr)
    {
        (void)printf("Failed on CREATE_MODEL_INSTANCE\r\n");
        IoTHubClient_LL_Destroy(g_iotHubClientHandle);
        serializer_deinit();
        platform_deinit();
        return;
     }
    g_bInitialized = true;
  }
}

void AzureIoTHubClientLoop()
{
  AzureIoTHubInitialize();
  
  if (g_bShouldReset && (millis() - g_resetCommandTime) > 500) //0.5 seconds before reset to let the ack to be sent
  {
    Serial.println("Reset..");
    ESP.restart();
  }
  IoTHubClient_LL_DoWork(g_iotHubClientHandle);
}
