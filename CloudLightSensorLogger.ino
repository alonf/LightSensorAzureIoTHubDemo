#include <AzureIoTHub.h>
#include <AzureIoTProtocol_MQTT.h>
#include "AzureIoTHubClient.h"

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <deque>

using namespace std;

const char *pWebPage = R"(
<html>
<head>
    <title>Light Sensor Demo</title>
    <style>
  canvas{
    -moz-user-select: none;
    -webkit-user-select: none;
    -ms-user-select: none;
  }
  </style>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/Chart.js/2.7.3/Chart.bundle.js"></script>
    
</head>
<body>
    <div style="width:75%;">
        <canvas id="canvas"></canvas>
    </div>
    <script>
        var samples = 0;
        window.chartColors = {
            red: 'rgb(255, 99, 132)',
            orange: 'rgb(255, 159, 64)',
            yellow: 'rgb(255, 205, 86)',
            green: 'rgb(75, 192, 192)',
            blue: 'rgb(54, 162, 235)',
            purple: 'rgb(153, 102, 255)',
            grey: 'rgb(201, 203, 207)'
        };

    var config = {
      type: 'line',
      data: {
        labels: [],
        datasets: [{
          label: 'Light Intensity',
          backgroundColor: window.chartColors.red,
          borderColor: window.chartColors.red,
          data: [
            
          ],
          fill: false,
        }]
      },
      options: {
        responsive: true,
        title: {
          display: true,
          text: 'Light Sensor Demo'
        },
        tooltips: {
          mode: 'index',
          intersect: false,
        },
        hover: {
          mode: 'nearest',
          intersect: true
        },
        scales: {
          xAxes: [{
            display: true,
            scaleLabel: {
              display: true,
              labelString: 'Time'
            }
          }],
          yAxes: [{
            display: true,
            scaleLabel: {
              display: true,
              labelString: 'Value'
            }
          }]
        }
      }
    };
    
   window.onload = function() {
      var ctx = document.getElementById('canvas').getContext('2d');
      window.myLine = new Chart(ctx, config);
      setInterval(async ()=> {
        const promise = await fetch("GetData");
        const result = await promise.json();
        result.data.forEach(element => {
            config.data.labels.push(samples++);
            config.data.datasets[0].data.push(element);
            if (config.data.datasets[0].data.length > 60) {
                config.data.datasets[0].data.shift();
                config.data.labels.shift();
            }});
            window.myLine.update();
        }, 500);
    }
    </script>
</body>
</html>
)";

void HandleRoot();
void HandleReset();
void HandleNotFound();
void HandleGetData();

char resultBuffer[4000];
deque<int> values;
int sampleTime;

const char* ssid = "<your access point SSID>";
const char* password = "<your access point password>";


// Create an instance of the server
// specify the port to listen on as an argument
ESP8266WebServer server(80); 

void setup() 
{
  Serial.begin(115200);
  sampleTime = millis();
  delay(10);

  
  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  server.on("/", HandleRoot);                    // Call the 'handleRoot' function when a client requests URI "/"
  server.on("/GetData", HTTP_GET, HandleGetData);
  server.on("/RESET", HandleReset);               
  server.onNotFound(HandleNotFound);        

  // Start the server
  server.begin();
  Serial.println("Server started");

  // Print the IP address
  Serial.println(WiFi.localIP());

  if (!MDNS.begin("light"))              // Start the mDNS responder for light.local
    Serial.println("Error setting up MDNS responder!");
  else
    Serial.println("mDNS responder started");
}


void loop() 
{
  if (millis() - sampleTime > 1000) //every  second
  {
    sampleTime = millis();
    int sample = analogRead(A0);
    values.push_back(sample);
    SendToCloud(sample);
    if (values.size() > 120) //keep no more the 120 samples
    {
      values.pop_front();
    }
  }
  server.handleClient(); 
  AzureIoTHubClientLoop();
}

void HandleRoot()
{
   server.send(200, "text/html", pWebPage); 
}

void HandleGetData()
{
   String data {R"({"data":[)"};
   for (const int &val : values)
   {
      data += String(val) + ",";
   }
   data = data.substring(0, data.length() - 1) + "]}";
   server.send(200, "text/json", data);
   values.clear();
}

void HandleNotFound()
{
  server.send(404, "text/plain", "404: Not found"); 
}

void HandleReset()
{
  values.clear();
  sampleTime = millis();
  server.send(200, "text/html", "<h1>The data has been reset</h1>");
}
