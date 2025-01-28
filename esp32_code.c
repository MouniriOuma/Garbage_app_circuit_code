#include <Wire.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h> // Firebase ESP Client library
#include <time.h>

// --- Wi-Fi Credentials ---
const char* ssid = ""; // Replace with your Wi-Fi name
const char* password = "";         // Replace with your Wi-Fi password

// --- Firebase Credentials ---
#define API_KEY "" // Firebase API key
#define DATABASE_URL "" // Database URL

// --- MQ135 Gas Sensor ---
#define PPM_PIN 32 // MQ135 connected to GPIO 32

// --- HC-SR04 Ultrasonic Sensor ---
const int trigPin = 13;  // HC-SR04 TRIG connected to GPIO 13
const int echoPin = 12;  // HC-SR04 ECHO connected to GPIO 12

// Sound speed constant in cm/uS
#define SOUND_SPEED 0.034
#define CM_TO_INCH 0.393701

// Variables for HC-SR04
long duration;
float distanceCm;
float distanceInch;

// Define Firebase objects
FirebaseData fbdo;         // Firebase data object
FirebaseAuth auth;         // Firebase authentication object
FirebaseConfig config;     // Firebase configuration object

unsigned long lastMillis=0;

// NTP Server and Timezone
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;       // Adjust for your timezone
const int daylightOffset_sec = 0;  // Adjust for daylight savings time if applicable

String macAddress;
void setup() {
  // Initialize serial communication
  Serial.begin(115200);

  // Initialize MQ135 sensor pin
  pinMode(PPM_PIN, INPUT);

  // Initialize HC-SR04 pins
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT);  // Sets the echoPin as an Input

  // Connect to Wi-Fi
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP()); // Display the assigned IP address

  // Initialize NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Synchronizing time with NTP server...");
  while (!time(nullptr)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nTime synchronized!");

  // Get MAC address
  macAddress = WiFi.macAddress();
  Serial.println("MAC Address: " + macAddress);

  // Configure Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.timeout.serverResponse =10000;

  // Enable anonymous authentication
  Serial.println("Authenticating anonymously...");
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Anonymous authentication successful!");
  } else {
    Serial.println("Failed to authenticate anonymously.");
    Serial.printf("Reason: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("MQ135 and HC-SR04 System Initialized with Firebase!");
}
// Function to get the current time as a string
String getCurrentTime() {
  time_t now = time(nullptr);
  struct tm timeInfo;
  gmtime_r(&now, &timeInfo); // Convert to GMT time structure

  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeInfo); // Format: YYYY-MM-DD HH:MM:SS
  return String(buffer);
}

void loop() {
  // --- MQ135 Gas Sensor Reading ---
  int16_t ppmValue = analogRead(PPM_PIN);
  float voltage = (ppmValue / 4095.0) * 3.3; // Convert raw ADC value to voltage
  int mappedppmValue = voltage * 1000; // Example conversion to PPM

  Serial.print("PPM (MQ135): ");
  Serial.println(mappedppmValue);

  // --- HC-SR04 Ultrasonic Sensor Reading ---
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  duration = pulseIn(echoPin, HIGH, 30000); // 30ms timeout

  if (duration == 0) {
    Serial.println("Error: No pulse received from HC-SR04");
    distanceCm = -1; // Indicates no valid distance
  } else {
    distanceCm = duration * SOUND_SPEED / 2;
    distanceInch = distanceCm * CM_TO_INCH;

    Serial.print("Distance (cm): ");
    Serial.println(distanceCm);
  }

  // --- Send Data to Firebase ---
  // Get current formatted timestamp
// Get current timestamp
  String timestamp = getCurrentTime();
  if (Firebase.ready() && (millis() - lastMillis > 1000)) {
    lastMillis = millis();
    String path = "/" + macAddress + "/sensorData/" + timestamp;
    Serial.println("Path: " + path);
    // Save gas sensor data
    if (Firebase.RTDB.setInt(&fbdo, path + "/ppm", mappedppmValue)) {
      Serial.println("PPM data saved!");
    } else {
      Serial.println("Failed to save PPM data!");
      Serial.println(fbdo.errorReason());
    }

    // Save distance data
    if (Firebase.RTDB.setFloat(&fbdo, path + "/distance_cm", distanceCm)) {
      Serial.println("Distance data saved!");
    } else {
      Serial.println("Failed to save distance data!");
      Serial.println(fbdo.errorReason());
    }
  }
  delay(6000);
}
