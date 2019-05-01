/*  
 *  AUTH0 ARDUINO MKR1000 IOT DEMO
 *  
 *  Features:
 *  - Connect to WiFi (display status on LCD + LED).
 *  - Get OAuth2 device code (diaplay on LCD). 
 *  
 *  CHRIS ADRIAENSEN @ Auth0, 2019
 */

#include <SPI.h>
#include <WiFi101.h>
#include <LiquidCrystal.h>
#include <ArduinoJson.h>
#include <base64.hpp>

/*
 * Set global constant attributes.
 */
static const char WIFI_SSID[] = "Auth0 Public"; // "2C7F Hyperoptic 1Gb Fibre 2.4Ghz"; // ADJUST TO SETUP
static const char WIFI_PASSWORD[] = "NeverCompromise"; // "gTekrua4auaf"; // ADJUST TO SETUP
static const char AUTH0_HOST[] = "chrisadriaensen.eu.auth0.com"; // ADJUST TO SETUP
//static const String APP_HOST = "app.example.com:8080"; // ADJUST TO SETUP
//static const IPAddress OPENAM_SERVER(172,20,10,3); //OPENAM_SERVER(192,168,1,5); //  ADJUST TO SETUP
//static const IPAddress APP_SERVER(172,20,10,3); //APP_SERVER(192,168,1,5); //  ADJUST TO SETUP
static const int AUTH0_PORT = 443; // ADJUST TO SETUP
//static const int APP_PORT = 8080; // ADJUST TO SETUP
static const char OAUTH_CLIENT_ID[] = "PRMMboxs8XgIn5HVWmwA128MEcZFrLEx"; // ADJUST TO SETUP
static const char OAUTH_CLIENT_SECRET[] = "-W6tnY7EzGiiQai4swkqnPHN0d6ab_Ho9qUsHi_DkBuMo0MIzuypusefLzMw8f_U"; // ADJUST TO SETUP
static const int SERIAL_PORT = 9600;
static const int NO_LED = -1;
static const int RED_LED = 0;
static const int GREEN_LED = 1;
static const int LCD_WIDTH = 16;
static const int LCD_HEIGTH = 2;
static const int ERROR_STATE = -1;
static const int INITIAL_STATE = 0;
static const int WAIT_STATE = 1;
static const int END_STATE = 2;
static const int TMP_SENSOR = A0;

/*
 * Set global variable attributes.
 */
static int CURRENT_STATE = INITIAL_STATE;
static String OAUTH_USER_CODE;
static String OAUTH_DEVICE_CODE;
static int OAUTH_INTERVAL;
static int OAUTH_EXPIRES_IN;
static String OAUTH_VERIFICATION_URL;
unsigned long OAUTH_RECEIVED;
static String OAUTH_ACCESS_TOKEN;
static String OIDC_ID_TOKEN;

/*
 * Initialize WiFi client and LCD display.
 */
WiFiClient WIFI_CLIENT;
LiquidCrystal LCD(12, 11, 5, 4, 3, 2);

/*
 * Setup demo (called once initially).
 */
void setup() {
  
  /*
   * Initialize serial port and wait to open.
   */
  Serial.begin(SERIAL_PORT);
  while (!Serial) delay(1000);
  Serial.println("Serial intialized.");

  /*
   * Initialize board.
   */
  LCD.begin(LCD_WIDTH, LCD_HEIGTH);
  LCD.setCursor(0,0);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  /*
   * Connect to Wifi (if fails wait 1 minute).
   */
  while (!connect()) {

    Serial.println("Can't connect to WiFi network! >> sleeping 60 seconds");
    
    delay(60000);
    
  }
  
}

/*
 * Connect to WiFi network. 
 */
boolean connect() {
  
  /*
   * Check for WiFi hardware.
   */
  if (WiFi.status() == WL_NO_SHIELD) {

    output("No WiFi hardware found!", RED_LED);
    
    return false;
    
  }

  /*
   * Connect to WiFi network.
   */
  int status = WL_IDLE_STATUS;
  while (status != WL_CONNECTED) {

    output("Connecting...   " + String(WIFI_SSID), RED_LED);

    status = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.println("Waiting for connection to WiFi network! >> sleeping 1 second");

    /*
     * Wait 1 second.
     */
    delay(1000);
  
  }

  output("Connected       " + String(WIFI_SSID), NO_LED);

  return true;

}

/*
 * Loop demo (called continously after setup).
 */
void loop() {

/*
 * Continue based on current state.
 */
  switch (CURRENT_STATE) {

    case INITIAL_STATE:
      
      executeInitialState();
      Serial.println("Waiting for next loop! >> sleeping 1 second");
      delay(1000);
      
      break;
    case WAIT_STATE:
      
      executeWaitState();
      Serial.println("Waiting for next loop! >> sleeping 1 second");
      delay(1000);
      
      break;
    case END_STATE:
    
      executeEndState();
      Serial.println("Waiting for next loop! >> sleeping 5 seconds");
      delay(5000);
      
      break;
    default:
      //executeErrorState();
      Serial.println("ERROR");
      break;
    
  }

}

/*
 * Execute initial state.
 */
void executeInitialState() {

  Serial.println("Intial state started.");

  /*
   * Stop all connections.
   */
  WIFI_CLIENT.stop();

  /*
   * Connect to Auth0 service.
   */
  while (!WIFI_CLIENT.connectSSL(AUTH0_HOST, AUTH0_PORT)) {

    Serial.println("Waiting for connection to Auth0! >> sleeping 1 second");
    
    delay(1000);
  
  }

  /*
   * Send OAuth device code request.
   */
  String data = "client_id=" + String(OAUTH_CLIENT_ID) + "&scope=openid profile";
  
  WIFI_CLIENT.println("POST /oauth/device/code HTTP/1.1");
  WIFI_CLIENT.println("Host: " + String(AUTH0_HOST));
  WIFI_CLIENT.println("User-Agent: ArduinoWiFi/1.1");
  WIFI_CLIENT.println("Content-Type: application/x-www-form-urlencoded");
  WIFI_CLIENT.println("Content-Length: " + String(data.length()));
  WIFI_CLIENT.println("");
  WIFI_CLIENT.println(data);
  WIFI_CLIENT.println("Connection: close");
  WIFI_CLIENT.println();

 /*
  * Wait for response.
  */
  while (!WIFI_CLIENT.available()) {

    Serial.println("Waiting for response from Auth0! >> sleeping 1 second");
    
    delay(1000);
  
  }

  /*
   * Parse response.
   */
  char responseBuffer[500];
  int i = 0;
  
  while (WIFI_CLIENT.available() && i < 499) {
    char c = WIFI_CLIENT.read();
    if (i > 0 || c == '{') {
      responseBuffer[i] = c;
      Serial.write(c);
      i++;
    }
  }
  WIFI_CLIENT.flush();

  StaticJsonDocument<500> jsonDocument;
  deserializeJson(jsonDocument, responseBuffer);

  /*
   * Get OAuth device code.
   */
  String user_code = jsonDocument["user_code"];
  String device_code = jsonDocument["device_code"];
  int interval = jsonDocument["interval"];
  int expires_in = jsonDocument["expires_in"];
  String verification_url = jsonDocument["verification_uri"];

  OAUTH_USER_CODE = user_code;
  OAUTH_DEVICE_CODE = device_code;
  OAUTH_INTERVAL = interval;
  OAUTH_EXPIRES_IN = expires_in;
  OAUTH_VERIFICATION_URL = verification_url;
  OAUTH_RECEIVED = millis();

  /*
   * Set state.
   */
   CURRENT_STATE = WAIT_STATE;
  
}

/*
 * Execute wait state.
 */
void executeWaitState() {

  Serial.println("Wait state started.");

  /*
   * Check wether code has expired.
   */
  if ((millis() - OAUTH_RECEIVED) > (OAUTH_EXPIRES_IN * 1000L)) {

    /*
     * Set state.
     */
     CURRENT_STATE = INITIAL_STATE;

     return;
    
  }

  /*
   * Display code and verification URL.
   */
  for (int i = 0; i <= (OAUTH_VERIFICATION_URL.length() - 16); i++) {

    output("Code: " + OAUTH_USER_CODE + "  " + OAUTH_VERIFICATION_URL.substring(i,i+16), NO_LED);

    /*
     * Sleep 500 milliseconds.
     */
    delay(500);
    
  }

  /*
   * Stop all connections.
   */
  WIFI_CLIENT.stop();

  /*
   * Connect to Auth0 server.
   */
  while (!WIFI_CLIENT.connectSSL(AUTH0_HOST, AUTH0_PORT)) {

    Serial.println("Waiting for connection to Auth0! >> sleeping 1 second");
    
    delay(1000);
  
  }

  /*
   * Check OAuth device code validation.
   */
  String data =
    "client_id=" + String(OAUTH_CLIENT_ID) +
 //   "&client_secret=" + String(OAUTH_CLIENT_SECRET) +
    "&device_code=" + OAUTH_DEVICE_CODE +
    "&grant_type=urn:ietf:params:oauth:grant-type:device_code";
   
  WIFI_CLIENT.println("POST /oauth/token HTTP/1.1");
  WIFI_CLIENT.println("Host: " + String(AUTH0_HOST));
  WIFI_CLIENT.println("User-Agent: ArduinoWiFi/1.1");
  WIFI_CLIENT.println("Content-Type: application/x-www-form-urlencoded");
  WIFI_CLIENT.println("Content-Length: " + String(data.length()));
  WIFI_CLIENT.println("");
  WIFI_CLIENT.println(data);
  WIFI_CLIENT.println("Connection: close");
  WIFI_CLIENT.println();

 /*
  * Wait for response.
  */
  while (!WIFI_CLIENT.available()) {

    Serial.println("Waiting for response from Auth0! >> sleeping 1 second");
    
    delay(1000);
  
  }

  /*
   * Parse response.
   */
  char responseBuffer[2000];
  int i = 0;
  
  while (WIFI_CLIENT.available() && i < 1999) {
    char c = WIFI_CLIENT.read();
    if (i > 0 || c == '{') {
      responseBuffer[i] = c;
      Serial.write(c);
      i++;
      delay(1);

      if (c == '}') break;
    }
  }
  WIFI_CLIENT.flush();

  StaticJsonDocument<2000> jsonDocument;
  deserializeJson(jsonDocument, responseBuffer);

  String access_token = jsonDocument["access_token"];
  String id_token = jsonDocument["id_token"];
 
  if ((access_token != "null") && (id_token != "null")) {

    OAUTH_ACCESS_TOKEN = access_token;
    OIDC_ID_TOKEN = id_token;
    
    /*
     * States state.
     */
    CURRENT_STATE = END_STATE;
    
  }
  
}

/*
 * Execute end state.
 */
void executeEndState() {

  Serial.println("End state started.");

  /*
   * Stop all connections.
   */
  WIFI_CLIENT.stop();

//  /*
//   * Connect to Auth0 server.
//   */
//  while (!WIFI_CLIENT.connectSSL(AUTH0_HOST, AUTH0_PORT)) {
//
//    Serial.println("Waiting for connection to Auth0! >> sleeping 1 second");
//    
//    delay(1000);
//  
//  }
//
//  /*
//   * Get OAuth token information.
//   */   
//  WIFI_CLIENT.println("GET /openam/oauth2/tokeninfo?access_token=" + OAUTH_ACCESS_TOKEN + " HTTP/1.1");
//  WIFI_CLIENT.println("Host: " + String(AUTH0_HOST));
//  WIFI_CLIENT.println("User-Agent: ArduinoWiFi/1.1");
//  WIFI_CLIENT.println("");
//  WIFI_CLIENT.println("Connection: close");
//  WIFI_CLIENT.println();
//
// /*
//  * Wait for response.
//  */
//  while (!WIFI_CLIENT.available()) {
//
//    Serial.println("Waiting for response from OpenAM! >> sleeping 1 second");
//    
//    delay(1000);
//  
//  }
//
//  /*
//   * Parse response.
//   */
//  char responseBuffer[500];
//  int i = 0;
//  
//  while (WIFI_CLIENT.available() && i < 500) {
//    char c = WIFI_CLIENT.read();
//    if (i > 0 || c == '{') {
//      responseBuffer[i] = c;
//      Serial.write(c);
//      i++;
//    }
//  }
//  WIFI_CLIENT.flush();
//

    int startData = OIDC_ID_TOKEN.indexOf('.', 0) + 1;

    String OIDC_ID_TOKEN_DATA = OIDC_ID_TOKEN.substring(startData, OIDC_ID_TOKEN.indexOf('.', startData));

    unsigned char id_token_encoded[OIDC_ID_TOKEN_DATA.length()];

    OIDC_ID_TOKEN_DATA.toCharArray((char *)id_token_encoded, OIDC_ID_TOKEN_DATA.length());

    unsigned char id_token_decoded[decode_base64_length(id_token_encoded)];
    
    decode_base64(id_token_encoded, id_token_decoded);
    
    StaticJsonDocument<2000> jsonDocument;
    deserializeJson(jsonDocument, id_token_decoded);
//
//  if (response["error"]) {
//
//    CURRENT_STATE = INITIAL_STATE;
//    
//  } else {

    float voltage = analogRead(TMP_SENSOR) * (3300/1024);
    float temperature = (voltage - 500) / 10;

    String userName = jsonDocument["name"];
  
    String userInfo = "User: " + userName;
  
    while (userInfo.length() < LCD_WIDTH) userInfo += " ";
  
    output(userInfo.substring(0,LCD_WIDTH) + "Temp: " + String(temperature) + " C", GREEN_LED);

//    /*
//     * Stop all connections.
//     */
//    WIFI_CLIENT.stop();
//  
//    /*
//     * Connect to app server.
//     */
//    while (!WIFI_CLIENT.connect(APP_SERVER, APP_PORT)) {
//  
//      Serial.println("Waiting for connection to App! >> sleeping 1 second");
//      
//      delay(1000);
//    
//    }
//  
//    /*
//     * Send temperature information.
//     */   
//    WIFI_CLIENT.println("POST /UCB/App?temp=" + temp + " HTTP/1.1");
//    WIFI_CLIENT.println("Host: " + APP_HOST);
//    WIFI_CLIENT.println("User-Agent: ArduinoWiFi/1.1");
//    WIFI_CLIENT.println("");
//    WIFI_CLIENT.println("Connection: close");
//    WIFI_CLIENT.println();
//
//  }
  
}

/*
 * Output message on LCD and activate led.
 */
void output(String message, int led) {

  LCD.clear();

  if (message.length() > LCD_WIDTH) {

    LCD.print(message.substring(0,LCD_WIDTH));

    for (int i = 1; (i < LCD_HEIGTH) && (message.length() > LCD_WIDTH*i); i++) {
    
      LCD.setCursor(0,i);
      LCD.print(message.substring(LCD_WIDTH*i, LCD_WIDTH*(i+1)));

    }
    
  } else {

    LCD.print(message);
    
  }

  if (led == RED_LED) {

    digitalWrite(RED_LED, HIGH);
    digitalWrite(GREEN_LED, LOW);
    
  } else if (led == GREEN_LED) {

    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, HIGH);
    
  } else {

    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
    
  }

}
