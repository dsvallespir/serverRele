
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WiFiMulti.h>   // Include the Wi-Fi-Multi library
#include <DHT.h>
#include <Wire.h>
#include <BH1750.h>
#include <Adafruit_PWMServoDriver.h>



#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>1

#define DHTPIN 2        // antes 4 (D2) ahora 2 (D4)
#define DHTTYPE DHT11
#define RELEPIN 0       // antes 5 (D1) ahora 0 (D3)
#define PWM_PIN 13      // GPIO13 (D7)

#define LEDPIN 14

#define TEMP_MIN 25.8
#define TEMP_MAX 26.20

// Definir constantes
#define ANCHO_PANTALLA 128 // ancho pantalla OLED
#define ALTO_PANTALLA 64 // alto pantalla OLED
 
// Objeto de la clase Adafruit_SSD1306
Adafruit_SSD1306 display(ANCHO_PANTALLA, ALTO_PANTALLA, &Wire, -1);

BH1750 LightSensor;

int PWMDutyCicle = 0;
int count = 0;

void sendHTML(WiFiClient *client);

ESP8266WiFiMulti wifiMulti; 

DHT dht(DHTPIN, DHTTYPE);

// WiFi network
const char* ssid = "Fibertel WiFi365 2.4GHz";
const char* password = "0042984229";

float temp_min = TEMP_MIN;
float temp_max = TEMP_MAX;

uint16_t lux = 0; 

float h_aux = 0;
float t_aux = 0;
float h = 0;
float t = 0;

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;
bool pageDisplayed = false;

WiFiServer server(80);


Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40, Wire);
#define SERVOMIN  150 // This is the 'minimum' pulse length count (out of 4096)
#define SERVOMAX  600 // This is the 'maximum' pulse length count (out of 4096)
#define USMIN  600 // This is the rounded 'minimum' microsecond length based on the minimum pulse of 150
#define USMAX  2400 // This is the rounded 'maximum' microsecond length based on the maximum pulse of 600
#define SERVO_FREQ 50 // Analog servos run at ~50 Hz updates

// our servo # counter
uint8_t servonum = 0;



void setup() {

    wifiMulti.addAP(ssid, password); // prueba
  
    // Start serial
    Serial.begin(115200);
    delay(10);
 
    // Connecting to a WiFi network
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
 
    Serial.println(ssid);
    WiFi.begin(ssid, password);

    int i = 0;
    while (wifiMulti.run() != WL_CONNECTED) { // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
        delay(1000);
        Serial.print(++i); Serial.print(' ');
    }
    //

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    WiFi.printDiag(Serial);
    ESP.getFreeSketchSpace();

    if(!MDNS.begin("prototipo")) {
        Serial.println("Error setting up MDNS responder!");
    }
    else{
        Serial.println("mDNS responder started");
    }
  
    MDNS.addService("http", "tcp", 80); 
  
    server.begin();   
    Serial.println("Server started"); 

    dht.begin();

    pinMode(RELEPIN, OUTPUT);  
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(LEDPIN, OUTPUT);
    pinMode(PWM_PIN, OUTPUT);
    analogWriteFreq(50);  // 50 Hz

    

    ledOff(LED_BUILTIN);
    ledOff(LEDPIN);

    int count = 0;
    Wire.begin();
    for (byte i = 8; i < 120; i++)
    {
        Wire.beginTransmission(i);
        if (Wire.endTransmission() == 0)
        {
            Serial.print("Found I2C Device: ");
            Serial.print(" (0x");
            Serial.print(i, HEX);
            Serial.println(")");
            count++;
            delay(1);
        }
    }
    Serial.print("\r\n");
    Serial.println("Finish I2C scanner");
    Serial.print("Found ");
    Serial.print(count, HEX);
    Serial.println(" Device(s).");

    LightSensor.begin();
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

      if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
            Serial.println("No se encuentra la pantalla OLED");
      }
      else{

          // Limpiar buffer
        display.clearDisplay();
          // Tamaño del texto
        display.setTextSize(1);
        // Color del texto
        display.setTextColor(SSD1306_WHITE);
        // Posición del texto
        display.setCursor(1, 1);
        // Escribir texto
        display.println("Pantalla OLED iniciada..");
                      
        // Enviar a pantalla
        display.display();
      }

        pwm.begin();
        pwm.setPWMFreq(50);  // This is the maximum PWM frequency

//        ServoRoutine();
    
}

void loop() {
 
    delay(100);

    // Control temporal para la ejecución de la rutina del DHT cada 2 segundos
    currentTime = millis();
  
    if(currentTime >= previousTime + timeoutTime){
        previousTime = currentTime;
        dhtRoutine();
        lightSensorRoutine();
        displayOledRoutine();
        PWMRoutine();
        //ServoRoutine();
    }

    // Fundamental para que funcione el .local
    MDNS.update(); 

    // asignar un cliente
    WiFiClient client = server.available();         

    // si existe..
    if (client){ 

        Serial.println("Nuevo cliente..:");
        pageDisplayed = false;

        ledBeep(LED_BUILTIN); 
        
        // si está conectado...
        if(client.connected()){

            // si hay datos disponibles
            if(client.available()){    
              
                // leer hasta el fin de carro
                String request = client.readStringUntil('\n');   

                // imprimir en el puerto serie
                Serial.println(request);

                // esperar hasta que se envien todos los datos.
                client.flush();

                // parseo para el led
                if(request.indexOf("led_on=On") > 0){
                   ledOn(LEDPIN);
                   ledBeep(LED_BUILTIN);
                   // Serial.println("Request led on procesado");
                }
                else{ 
                   if(request.indexOf("led_off=Off") > 0){
                       ledOff(LEDPIN);
                       ledBeep(LED_BUILTIN);
                   //  Serial.println("Request led off procesado");
                    }
                }

                if(request.indexOf("rele_on=On") > 0){
                   releOn();
                   ledBeep(LED_BUILTIN);
                    Serial.println("Request rele on procesado");
                }
                else{ 
                   if(request.indexOf("rele_off=Off") > 0){
                       releOff();
                       ledBeep(LED_BUILTIN);
                       Serial.println("Request rele off procesado");
                    }
                }

                // parseo para las temperaturas

                if(request.indexOf("temp_max=") > 0) {

                    String str = request;
                    int i = str.indexOf("=");
                    int len = str.length();
                    str.remove(0,i+1);
                    str.remove(4,str.length());
                    temp_max = str.toFloat();
                    Serial.println("Nueva temperatura máxima establecida: ");
                    Serial.println(temp_max);
         
                    ledBeep(LED_BUILTIN); ;         
                }

                if(request.indexOf("temp_min=") > 0) {

                    String str = request;
                    int i = str.indexOf("=");
                    int len = str.length();
                    str.remove(0,i+1);
                    str.remove(4,str.length());
                    temp_min = str.toFloat();
                    Serial.println("Nueva temperatura mínima establecida: ");
                    Serial.println(temp_min);

                    ledBeep(LED_BUILTIN); 
                }

                if(request.indexOf("brillo=") > 0) {

                    String str = request;
                    int i = str.indexOf("=");
                    int len = str.length();
                    str.remove(0,i+1);
                    str.remove(4,str.length());
                    count = str.toInt();
                    Serial.println("Nueva valor de brillo establecido: ");
                    Serial.println(count);

                    ledBeep(LED_BUILTIN); 
                }

                if(pageDisplayed == false){
                    sendHTML(&client);
                    pageDisplayed = true;
                }
            }
        } 
    }
}


void dhtRoutine(){
  h = dht.readHumidity();
  t = dht.readTemperature();
  
  if(isnan(h) || isnan(t)) {
    Serial.println("Hubo un error leyendo el sensor");
    return;
  }

  float hic = dht.computeHeatIndex(t, h, false);

  if (h != h_aux || t != t_aux){
    Serial.print("Humedad: ");
    Serial.print(h);
    Serial.print(" %\t");
    Serial.print("Temperatura: ");
    Serial.print(t);
    Serial.print(" *C ");
    Serial.print("\t\t");
    Serial.print("Indice de calor: ");
    Serial.print(hic);
    Serial.println(" *C ");

  }

  if(t > temp_max && digitalRead(RELEPIN) != LOW){
      releOn();
      Serial.println("Temperatura máxima alcanzada.. fan encendido");
  }
  if(t <= temp_min && digitalRead(RELEPIN)!= HIGH){
      releOff();
      Serial.println("Temperatura mínima alcanzada.. fan apagado");
  }

  t_aux = t;
  h_aux = h;
}
  

void releToggle(){
  digitalWrite(RELEPIN, !digitalRead(RELEPIN));
}

void releOn(){
  digitalWrite(RELEPIN, LOW);
}

void releOff(){
  digitalWrite(RELEPIN, HIGH);
}


void ledToggle(int led){
  digitalWrite(led, !digitalRead(led));
}

void ledOn(int led){
  digitalWrite(led, LOW);
  Serial.println("Led on..");
}

void ledOff(int led){
  digitalWrite(led, HIGH);
  Serial.println("Led off..");
}

void ledBeep(int led){
  digitalWrite(led, !digitalRead(led));
  delay(50);
  digitalWrite(led, !digitalRead(led));
}

/* Funciòn que despliega la página principal */

void sendHTML(WiFiClient *client){

      // Enviar al cliente la página en html 
      
      client->println("HTTP/1.1 200 OK");   
      client->println("Content-Type: text/html");   
      client->println("");    
      client->println("<!DOCTYPE HTML>");   
      client->println("<html>");
      client->println("<head>");
      client->println("<title>");
      client->println("Prototipo"); 
      client->println("</title>");
      client->println("<meta charset=\"utf-8\" />");
         
      client->println("<meta http-equiv=\"Content-type\" content=\"text/html; charset=utf-8\"/>");
      client->println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"/>");
      client->println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; } </style></head>");
      client->println("<body>");
      client->println("<h1 style=\"color:Tomato;\">Controlador de Temperatura y Humedad</h1>");
      client->println("Temperatura: ");   
      client->println(t);    
      client->println(" *C  ");
      client->println("<br>");   
      client->println("Humedad: ");   
      client->println(h);   
      client->println(" %  "); 
      client->println("<br>"); 
      client->println("Intensidad de luz: "); client->println(String(lux)); client->println(" lux");
      client->println("<hr>");     
      client->println("<p style=\"display:inline;\">Estado del ventilador: </p>");
      {
        if(digitalRead(RELEPIN)){
          client->println("<p style=\"background-color: red; color: white; padding:4px; white-space:pre-wrap; display:inline;\" >apagado</p>"); 
        }
        else {
          client->println("<p style=\"background-color: green; color: white; padding:4px; white-space:pre-wrap; display:inline;\" > encendido</p>");
        }
      }
      client->println("<br><br>");   
      client->println("<p style=\"display:inline;\">Estado del extractor: </p> <p style =\"background-color: red; color: white; padding:4px; white-space:pre-wrap; display:inline;\" >apagado</p>");
      client->println("<br><br>");   
    //  client->println("<p style=\"display:inline;\">Estado de las luces cob: </p><p style =\"background-color: green; color: white; padding:4px; white-space:pre-wrap; display:inline;\" > encendidas</p>");
      client->println("<br><hr>");   
  
      client->println("Temperatura umbral maxima: ");
      client->println(temp_max);
      client->println(" *C");      
      client->println("<br>");   
      client->println("Temperatura umbral minima: ");
      client->println(temp_min);   
      client->println(" *C");    
      client->println("<br>");

      // Formulario Temperaturas umbral

      client->println("<form method=\"GET\">");
      client->println("<label for=\"temp_max\">Establecer nueva temperatura máxima: </label>");
      client->println("<input name=\"temp_max\" type=\"number\" placeholder=\"");client->println(temp_max); client->println("\" min=\"20.0\" max=\"30.0\" lang=\"en\" step=\"0.1\" value =\"");
      client->println(String(temp_max,1)); client->println("\">");
      client->println("<input id=\"temp_max\" type=\"submit\" value=\"OK\">");
      client->println("</form>");
      client->println("<br>");   
      client->println("<form method=\"GET\">");
      client->println("<label for=\"temp_min\">Establecer nueva temperatura mínima: </label>");
      client->println("<input name=\"temp_min\" type=\"number\" placeholder=\"");client->println(temp_min); client->println("\" min=\"20.0\" max=\"30.0\" lang=\"en\" step=\"0.1\" value =\"");
      client->println(String(temp_min,1)); client->println("\">");
      client->println("<input id=\"temp_min\" type=\"submit\" value=\"OK\">");
      client->println("</form>");
      client->println("<br>");   
      client->println("<hr>");  

     // Formulario Led COB

      client->println("<form method=\"GET\">");
      client->println("<label for=\"brillo\">Establecer potencia: </label>");
      client->println("<input name=\"brillo\" type=\"range\" placeholder=\"");client->println(1023 - count); client->println("\" min=\"0\" max=\"1023\" lang=\"en\" step=\"1\" value =\"");
      client->println(String(1023 - count)); client->println("\">");
      client->println("<input id=\"brillo\" type=\"submit\" value=\"OK\">");
      client->println("</form>");
      client->println("<br>");   
      
      // Formulario Led
      
      client->println("<form method=\"GET\">");
      
      if(digitalRead(LEDPIN) == HIGH){
        client->println("<label for=\"led_on\">Relé auxiliar      </label>");
        client->println("<input id=\"ledon\" name=\"led_on\" type=\"submit\" value=\"On\">");  
      }else if(digitalRead(LEDPIN) == LOW){
        client->println("<label for=\"led_off\">Relé auxiliar     </label>");
        client->println("<input id=\"ledoff\" name=\"led_off\" type=\"submit\" value=\"Off\">");  
      }     
      
      client->println("<br>");   

       // Formulario Rele
       
      if(digitalRead(RELEPIN) == HIGH){
        client->println("<label for=\"rele_on\">Relé Fan (manual) </label>");
        client->println("<input id=\"releon\" name=\"rele_on\" type=\"submit\" value=\"On\">");  
      }else if(digitalRead(RELEPIN) == LOW){
        client->println("<label for=\"rele_off\">Relé Fan (manual) </label>");
        client->println("<input id=\"releoff\" name=\"rele_off\" type=\"submit\" value=\"Off\">");  
      }
      client->println("</form>");

      client->println("<br>");  

      client->println("<form method=\"GET\">");
      client->println("<input id=\"refresh\" name=\"refresh\" type=\"submit\" value=\"Actualizar\">");  
      client->println("</form>");

      
      client->println("<hr>");   

      // Footer
      client->println("<footer>");
      client->println("<p style=\" color: grey; font-style: italic;\">V1.0 build: 15-11-2020 - Sebastian Vallespir - 2020</p>");
      client->println("</footer>");
      client->println("</body>");  
      client->println("</html>"); 

      // Fin de la página html
}


void lightSensorRoutine(){
  lux = LightSensor.readLightLevel();// Get Lux value
}

void displayOledRoutine(){
            // Limpiar buffer
    display.clearDisplay();
          // Tamaño del texto
    display.setTextSize(1);
        // Color del texto
    display.setTextColor(SSD1306_WHITE);
        // Posición del texto
    display.setCursor(0, 0);
        // Escribir texto
    display.println("Fibertel WiFi365 2.4GHz");
    display.println("IP: 192.168.0.211");
    display.print("T: ");
    display.print(String(t,1)); 
    display.print("-H: ");
    display.print(String(h,1));
    display.print(" - L: ");
    display.println(lux);

    if(digitalRead(RELEPIN) == HIGH){
      display.println("Fan Off");
    }
    else{
      display.println("Fan On");
    }
    
    if(digitalRead(LEDPIN) == HIGH){
      display.println("Rele Aux Off");
    }
    else{
      display.println("Rele Aux On");
    }
                            
        // Enviar a pantalla
    display.display();

}


void PWMRoutine(){
  
  analogWrite(PWM_PIN, 1023 - count);
  Serial.print("Brillo: ");
  Serial.println(String(count));
}

//void PWMIntroduction(){
//   // increase the LED brightness
//  for(int dutyCycle = 0; dutyCycle < 1023; dutyCycle++){   
//    // changing the LED brightness with PWM
//    analogWrite(PWM_PIN, dutyCycle);
//    delay(40);
//  }
//
//  // decrease the LED brightness
//  for(int dutyCycle = 1023; dutyCycle > 0; dutyCycle--){
//    // changing the LED brightness with PWM
//    analogWrite(PWM_PIN, dutyCycle);
//    delay(40);
//  }
//}
//
//void ServoRoutine(){
//
//    for (uint16_t i=0; i<4096; i += 8) {
//       for (uint8_t pwmnum=0; pwmnum < 16; pwmnum++) {
//           pwm.setPWM(pwmnum, 0, (i + (4096/16)*pwmnum) % 4096 );
//    }
//#ifdef ESP8266
//    yield();  // take a breather, required for ESP8266
//#endif
//}
//}
