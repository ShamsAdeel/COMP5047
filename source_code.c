#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include <SD_MMC.h>
#include <FFat.h>
#include <SPIFFS.h>
#include <Audio.h>
#include <HardwareSerial.h>

// WiFi credentials
const char* ssid = "SSID"; // To replicate our project, enter your WiFi SSID
const char* password = "SSID_KEY"; // To replicate our project, enter the specific WiFi SSID's corresponding password

// OpenAI Speech-to-Text API URL and key
const char* stt_api_url = "https://api.openai.com/v1/audio/transcriptions"; //STT API URL
const char* stt_api_key = "STT_API_KEY" // OpenAI STT API key

// Gemini API endpoint URL
const char* gemini_url = "https://generativelanguage.googleapis.com/v1/models/gemini-pro:generateContent"; // Gemini API URL 
const char* gemini_api_key = "GEMINI_API_KEY"; //Gemini API key

// Text-to-Speech API URL and key
const char* tts_api_url = "https://api.openai.com/v1/audio/speech"; // TTS API URL
const char* tts_api_key = "TTS_API_KEY"; // TTS API key

// Google Maps API key
const char *maps_api_key = "MAPS_API_KEY"; // Google Maps API key

// File names for storage of audio clips, transcripts, evaluations, and ratings

const char* fullstoryTTS = "/base_story_TTS.txt"; // base prompt for announcement
const char* base_story = "/base_story.txt"; // master base prompt
const char* storySoFar = "/storySoFar.txt"; // story context

// Files to store player ratings

const char* p1_rating = "/p1_rating.txt";
const char* p2_rating = "/p2_rating.txt";
const char* p3_rating = "/p3_rating.txt";
const char* p4_rating = "/p4_rating.txt";

// Files to store player speech
const char* p1_response1="/p1_response1.wav";
const char* p1_response2="/p1_response2.wav";

const char* p2_response1="/p2_response1.wav";
const char* p2_response2="/p2_response2.wav";

const char* p3_response1="/p3_response1.wav";
const char* p3_response2="/p3_response2.wav";

const char* p4_response1="/p4_response1.wav";
const char* p4_response2="/p4_response2.wav";

// Files to store player speech transcript
const char* p1_trans1="/p1_trans1.txt";
const char* p1_trans2="/p1_trans2.txt";

const char* p2_trans1="/p2_trans1.txt";
const char* p2_trans2="/p2_trans2.txt";

const char* p3_trans1="/p3_trans1.txt";
const char* p3_trans2="/p3_trans2.txt";

const char* p4_trans1="/p4_trans1.txt";
const char* p4_trans2="/p4_trans2.txt";

// Files to store player's speech evaluation text
const char* p1_eval1="/p1_eval1.txt";
const char* p1_eval2="/p1_eval2.txt";

const char* p2_eval1="/p2_eval1.txt";
const char* p2_eval2="/p2_eval2.txt";

const char* p3_eval1="/p3_eval1.txt";
const char* p3_eval2="/p3_eval2.txt";

const char* p4_eval1="/p4_eval1.txt";
const char* p4_eval2="/p4_eval2.txt";

// Files to store player feedback speech
const char* p1_feed1="/p1_feed1.mp3";
const char* p1_feed2="/p1_feed2.mp3";

const char* p2_feed1="/p2_feed1.mp3";
const char* p2_feed2="/p2_feed2.mp3";

const char* p3_feed1="/p3_feed1.mp3";
const char* p3_feed2="/p3_feed2.mp3";

const char* p4_feed1="/p4_feed1.mp3";
const char* p4_feed2="/p4_feed2.mp3";

// Transcript and audio file announcing the winner
const char *winner_feedback = "/winner_feedback.txt";
const char *winner_feedback_speech = "/winner_feedback_speech.mp3";

// Initialize the HTTP client to send requests to the Gemini API
HTTPClient http_client;
WiFiServer wifi_server(80);

// I2S Speaker Connections
#define I2S_DOUT      22
#define I2S_BCLK      26
#define I2S_LRC       25

// Speaker volume
#define VOLUME 80

// In-audio loop pause
#define IN_AUDIO_PAUSE 2

// Define SD card pins
const int SD_CS = 13;
const int SPI_MOSI = 23;
const int SPI_MISO = 19;
const int SPI_SCK = 18;

// Define GPS module pins
#define GPS_BAUD 9600
#define GPS_RX 2 
#define GPS_TX 4

HardwareSerial GPS(2); // Attach the GPS peripheral to the second UART port

// Function to commence connection to WiFi
bool connectToWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    Serial.print("Connecting to WiFi.");
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to WiFi!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        return true;
    }
    
    Serial.println("\nFailed to connect to WiFi!");
    return false;
}

// Function to initialise the SD card
bool initSDCard() {
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

    if (!SD.begin(SD_CS)) {
        Serial.println("SD Card initialization failed!");
        return false;
    }
    Serial.println("SD Card initialized.");
    return true;
}

// Function to read the text file from SD card
String readTextFromSD(const char* filename) {
  File file = SD.open(filename);
  if (!file) {
    Serial.println("Error opening file from SD card.");
    return "";
  }
  
  String text = "";
  while (file.available()) {
    text += (char)file.read();
  }
  file.close();
  return text;
}

// Function to write response to SD card
bool writeResponseToSD(const String& response, const char* filename) {
  
  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return false;
  }

  // Write the response
  size_t bytesWritten = file.print(response);
  file.close();
  
  if (bytesWritten == 0) {
    Serial.println("Failed to write to file");
    return false;
  }
  
  Serial.println("Response successfully written to " + String(filename));
  return true;
  }

// Function to store Gemini's response to text file
void processResponse(String evaluation, const char *outputFile) {
  Serial.println(evaluation);
  
  // Write the evaluation to a file on the SD card
  if (writeResponseToSD(evaluation, outputFile)) {
    Serial.println("Evaluation saved to SD card: " + String(outputFile));
  } else {
    Serial.println("Failed to save evaluation to SD card");
  }
}

// Invokes Gemini API to evaluate the first contribution
String evaluateFContribution(const char* base_prompt, const char* player_contribution, const char* evaluation) {

  // Read the base prompt generated by Gemini
  String baseprompt = readTextFromSD(base_prompt);
  
  // Read the story started by the first player
  String playerStory = readTextFromSD(player_contribution);

  // Construct the complete URL with API key
  String url = String(gemini_url) + "?key=" + String(gemini_api_key);
  
  Serial.println("Connecting to Gemini API...");
  Serial.println("URL: " + url);
  
  if (!http_client.begin(url)) {
    Serial.println("Connection to API failed!");
    http_client.end();
    return "";
  }

  // Set the request headers
  http_client.addHeader("Content-Type", "application/json");

  // Create a JSON document for the request
  JsonDocument requestDoc;
  
    // Build the JSON structure
requestDoc["contents"][0]["parts"][0]["text"] = 
    String("We are hosting a collaborative storytelling contest for children. It consists of four players. A base story prompt is given. ") +
    "Players take turns to speak for thirty seconds each. For the sake of fairness, each player has to ensure that they leave sufficient room for others to continue the story from where they stop. " +
    "You have to assess the players on the basis of how well they speak, their articulation, contribution to the plot, usage of filler words, creativity, imagination, the story snippet they contributed, and whether they left sufficient room for others to continue the story from where they stopped. " +
    "This is the base prompt we gave to the players (generated by the host): \"" + baseprompt + "\"; " +
    "This is the first contribution to the base story by the first player of the game (we transcribed his/her speech): \"" + playerStory + "\"; Assess the player's contribution to the story on the basis " +
    "of the aforesaid factors (along with the fact whether he/she did a decent start to the story or not) and provide a short constructive feedback in text that can be spoken in approximately thirty seconds. " +
    "At the end of your feedback, provide a rating out of ten (say specifically \"I rate your contribution <rating> out of 10\"). Write everything in a single paragraph. No special characters. "+
    "If the player's contribution to the story, which I provided in the form of the transcription of his/her speech, does not seem sufficient to utilise thirty seconds of speaking time (indicating he/she spoke less), then penalise them for that. ";


  // Serialize the request
  String requestBody;
  serializeJson(requestDoc, requestBody);

  // Debug print the request body
  Serial.println("Sending request with body:");
  Serial.println(requestBody);

  // Send the request
  int httpCode = http_client.POST(requestBody);

  // Debug print the response code
  Serial.print("HTTP Response code: ");
  Serial.println(httpCode);

  if (httpCode != 200) {
    String error = http_client.getString();
    Serial.println("Error response:");
    Serial.println(error);
    http_client.end();
    return "Error: HTTP " + String(httpCode) + " - " + error;
  }

  // Get the response
  String response = http_client.getString();
  Serial.println("Raw response:");
  Serial.println(response);
  
  // Parse the response
  JsonDocument responseDoc;
  DeserializationError error = deserializeJson(responseDoc, response);
  
  if (error) {
    Serial.print("JSON parsing failed: ");
    Serial.println(error.c_str());
    http_client.end();
    return "Error: JSON parsing failed - " + String(error.c_str());
  }

  // Extract the feedback text
  String feedback;
  if (responseDoc["candidates"][0]["content"]["parts"][0]["text"].is<const char*>()) {
    feedback = responseDoc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
  } else {
    Serial.println("Unexpected response format");
    Serial.println(response);
    feedback = "Error: Unable to parse response - unexpected format";
  }

  Serial.println("Feedback: " + feedback);
  
  // Write the feedback to a file on the SD card
  if (writeResponseToSD(feedback, evaluation)) {
    Serial.println("Evaluation saved to SD card: " + String(evaluation));
  } else {
    Serial.println("Failed to save evaluation to SD card");
  }

  http_client.end();
  return evaluation;
}

// Invokes Gemini API to evaluate intermediary contributions
String evaluateContribution(const char* base_prompt, const char* story_path, const char* player_contribution, const char* evaluation) {

  // Read the base prompt generated by Gemini
  String baseprompt = readTextFromSD(base_prompt);

  // Read the story that has been contributed so far
  String storySoFar = readTextFromSD(story_path);
  
  // Read the story continued by the player
  String playerStory = readTextFromSD(player_contribution);

  // Construct the complete URL with API key
  String url = String(gemini_url) + "?key=" + String(gemini_api_key);
  
  Serial.println("Connecting to Gemini API...");
  Serial.println("URL: " + url);
  
  if (!http_client.begin(url)) {
    Serial.println("Connection to API failed!");
    http_client.end(); // Frees resources before returning
    return "";
  }

  // Set the request headers
  http_client.addHeader("Content-Type", "application/json");

  // Create a JSON document for the request
  JsonDocument requestDoc;
  
    // Build the JSON structure
requestDoc["contents"][0]["parts"][0]["text"] = 
    String("We are hosting a collaborative storytelling contest for children. It consists of four players. A base story prompt is given. ") +
    "Players take turns to speak for thirty seconds. For the sake of fairness, each player has to ensure that they leave sufficient room for others to continue the story from where they stop. " +
    "You have to assess the players on the basis of how well they speak, their articulation, contribution to the plot, usage of filler words, creativity, imagination, the story snippet they contributed, and whether they left sufficient room for others to continue the story from where they stopped. " +
    "This is the base prompt we gave to the players (generated by the host): \"" + baseprompt + "\"; This is the collaborative story that has been stitched so far: \"" + storySoFar + "\"; " +
    "And this is the current player's contribution to the story: \"" + playerStory + "\"; Assess the current player's contribution to the story on the basis " +
    "of the aforesaid factors and provide a short constructive feedback in text that can be spoken in approximately thirty seconds. " +
    "At the end of your feedback, provide a rating out of ten (say specifically \"I rate your contribution <rating> out of 10\"). Write everything in a single paragraph. "+
    "If the player's contribution to the story, which I provided in the form of the transcription of his/her speech, does not seem sufficient to utilise thirty seconds of speaking time (indicating he/she spoke less), then penalise them for that. ";
;


  // Serialize the request
  String requestBody;
  serializeJson(requestDoc, requestBody);

  // Debug print the request body
  Serial.println("Sending request with body:");
  Serial.println(requestBody);

  // Send the request
  int httpCode = http_client.POST(requestBody);

  // Debug print the response code
  Serial.print("HTTP Response code: ");
  Serial.println(httpCode);

  if (httpCode != 200) {
    String error = http_client.getString();
    Serial.println("Error response:");
    Serial.println(error);
    http_client.end();
    return "Error: HTTP " + String(httpCode) + " - " + error;
  }

  // Get the response
  String response = http_client.getString();
  Serial.println("Raw response:");
  Serial.println(response);
  
  // Parse the response
  JsonDocument responseDoc;
  DeserializationError error = deserializeJson(responseDoc, response);
  
  if (error) {
    Serial.print("JSON parsing failed: ");
    Serial.println(error.c_str());
    http_client.end();
    return "Error: JSON parsing failed - " + String(error.c_str());
  }

  // Extract the feedback text
  String feedback;
  if (responseDoc["candidates"][0]["content"]["parts"][0]["text"].is<const char*>()) {
    feedback = responseDoc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
  } else {
    Serial.println("Unexpected response format");
    Serial.println(response);
    feedback = "Error: Unable to parse response - unexpected format";
  }

  Serial.println("Feedback: " + feedback);
  
  // Write the feedback to a file on the SD card
  if (writeResponseToSD(feedback, evaluation)) {
    Serial.println("Evaluation saved to SD card: " + String(evaluation));
  } else {
    Serial.println("Failed to save evaluation to SD card");
  }

  http_client.end();
  return evaluation;
}

// Invokes Gemini API to evaluate intermediary contributions
String evaluateLContribution(const char* base_prompt, const char* story_path, const char* player_contribution, const char* evaluation) {

  // Read the base prompt generated by Gemini
  String baseprompt = readTextFromSD(base_prompt);

  // Read the story that has been contributed so far
  String storySoFar = readTextFromSD(story_path);
  
  // Read the story continued by the player
  String playerStory = readTextFromSD(player_contribution);

  // Construct the complete URL with API key
  String url = String(gemini_url) + "?key=" + String(gemini_api_key);
  
  Serial.println("Connecting to Gemini API...");
  Serial.println("URL: " + url);
  
  if (!http_client.begin(url)) {
    Serial.println("Connection to API failed!");
    http_client.end(); // Frees resources before returning
    return "";
  }

  // Set the request headers
  http_client.addHeader("Content-Type", "application/json");

  // Create a JSON document for the request
  JsonDocument requestDoc;
  
    // Build the JSON structure
requestDoc["contents"][0]["parts"][0]["text"] = 
    String("We are hosting a collaborative storytelling contest for children. It consists of four players. A base story prompt is given. ") +
    "Players take turns to speak for thirty seconds. For the sake of fairness, each player has to ensure that they leave sufficient room for others to continue the story from where they stop. " +
    "You have to assess the players on the basis of how well they speak, their articulation, contribution to the plot, usage of filler words, creativity, imagination, the story snippet they contributed, and whether they left sufficient room for others to continue the story from where they stopped. " +
    "This is the base prompt we gave to the players (generated by the host): \"" + baseprompt + "\"; This is the collaborative story that has been stitched so far: \"" + storySoFar + "\"; " +
    "The final player has spoken. This was his contribution to the story: \"" + playerStory + "\"; Assess the final player's contribution to the story on the basis " +
    "of the aforesaid factors and whether he provided an appropriate ending to the story or not; provide a short constructive feedback in text that can be spoken in approximately thirty seconds. " +
    "At the end of your feedback, provide a rating out of ten (say specifically \"I rate your contribution <rating> out of 10\"). Write everything in a single paragraph. "+
    "If the player's contribution to the story, which I provided in the form of the transcription of his/her speech, does not seem sufficient to utilise thirty seconds of speaking time (indicating he/she spoke less), then penalise them for that. ";
;


  // Serialize the request
  String requestBody;
  serializeJson(requestDoc, requestBody);

  // Debug print the request body
  Serial.println("Sending request with body:");
  Serial.println(requestBody);

  // Send the request
  int httpCode = http_client.POST(requestBody);

  // Debug print the response code
  Serial.print("HTTP Response code: ");
  Serial.println(httpCode);

  if (httpCode != 200) {
    String error = http_client.getString();
    Serial.println("Error response:");
    Serial.println(error);
    http_client.end();
    return "Error: HTTP " + String(httpCode) + " - " + error;
  }

  // Get the response
  String response = http_client.getString();
  Serial.println("Raw response:");
  Serial.println(response);
  
  // Parse the response
  JsonDocument responseDoc;
  DeserializationError error = deserializeJson(responseDoc, response);
  
  if (error) {
    Serial.print("JSON parsing failed: ");
    Serial.println(error.c_str());
    http_client.end();
    return "Error: JSON parsing failed - " + String(error.c_str());
  }

  // Extract the feedback text
  String feedback;
  if (responseDoc["candidates"][0]["content"]["parts"][0]["text"].is<const char*>()) {
    feedback = responseDoc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
  } else {
    Serial.println("Unexpected response format");
    Serial.println(response);
    http_client.end(); // Frees resources before returning
    feedback = "Error: Unable to parse response - unexpected format";
  }

  Serial.println("Feedback: " + feedback);
  
  // Write the feedback to a file on the SD card
  if (writeResponseToSD(feedback, evaluation)) {
    Serial.println("Evaluation saved to SD card: " + String(evaluation));
  } else {
    Serial.println("Failed to save evaluation to SD card");
  }

  http_client.end();
  return evaluation;
}

// Buffer size for chunked upload (4KB)
const size_t CHUNK_SIZE = 4096;

WiFiClientSecure client;

class ChunkedUploader {
private:
    WiFiClientSecure* client;
    String boundary;
    size_t contentLength;
    File audioFile;
    
public:
    ChunkedUploader(WiFiClientSecure* _client, const String& _boundary) 
        : client(_client), boundary(_boundary) {}
        
    bool begin(const char* filename, size_t fileSize) {
        audioFile = SD.open(filename);
        if (!audioFile) {
            Serial.println("Failed to open audio file!");
            return false;
        }
        
        String head = "--" + boundary + "\r\n"
                     "Content-Disposition: form-data; name=\"model\"\r\n\r\n"
                     "whisper-1\r\n"
                     "--" + boundary + "\r\n"
                     "Content-Disposition: form-data; name=\"file\"; filename=\"audio.mp3\"\r\n"
                     "Content-Type: audio/mp3\r\n\r\n";
        String tail = "\r\n--" + boundary + "--\r\n";
        
        contentLength = head.length() + fileSize + tail.length();
        
        String headers = "POST " + String(stt_api_url) + " HTTP/1.1\r\n";
        headers += "Host: api.openai.com\r\n";
        headers += "Authorization: Bearer " + String(stt_api_key) + "\r\n";
        headers += "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
        headers += "Content-Length: " + String(contentLength) + "\r\n";
        headers += "Connection: keep-alive\r\n\r\n";
        
        if (client->write((uint8_t*)headers.c_str(), headers.length()) != headers.length()) {
            Serial.println("Failed to send headers!");
            http_client.end(); // Frees resources before returning WRONG
            return false;
        }
        
        if (client->write((uint8_t*)head.c_str(), head.length()) != head.length()) {
            Serial.println("Failed to send multipart head!");
            http_client.end(); // Frees resources before returning
            return false;
        }
        
        return true;
    }
    
    bool uploadChunk() {
    uint8_t buffer[CHUNK_SIZE];
    size_t bytesRead = audioFile.read(buffer, CHUNK_SIZE);
    
    if (bytesRead > 0) {
        size_t written = client->write(buffer, bytesRead);
        if (written != bytesRead) {
            Serial.printf("Write failed! Expected %d bytes, wrote %d bytes\n", bytesRead, written);
            client->stop();  // Ensures client connection stops
            return false;
        }
        return true;
    }
    return false;
}

    
    void finish() {
        String tail = "\r\n--" + boundary + "--\r\n";
        client->write((uint8_t*)tail.c_str(), tail.length());
        audioFile.close();
    }
    
    String readResponse() {
        String response = "";
        uint32_t timeout = millis() + 30000;  // 30 second timeout
        
        while (millis() < timeout) {
            while (client->available()) {
                char c = client->read();
                response += c;
                timeout = millis() + 5000;  // Reset timeout on data received
            }
            
            if (response.indexOf("\r\n\r\n") > 0 && response.indexOf("}") > 0) {
                break;  // We have the complete response
            }
            
            delay(IN_AUDIO_PAUSE);  // Small delay to prevent tight loop
        }
        return response;
    }
};

// Function to convert Text to Speech (TTS)
void convertTextToSpeech(const char* textPath, const char* filePath) {
    Serial.println("Commencing conversion of text to speech.");

    // Read the text file
    String textContent;
    File textFile = SD.open(textPath);
    if (!textFile) {
        Serial.println("Failed to open the file");
        return;
    }
    while (textFile.available()) {
        textContent += (char)textFile.read();
    }
    textFile.close();

    // Create a secure client
    WiFiClientSecure client;
    client.setInsecure(); // Skip certificate verification
    client.setTimeout(30);  // Timeout specified in seconds

    HTTPClient https;

    if (https.begin(client, tts_api_url)) {
        https.addHeader("Authorization", String("Bearer ") + tts_api_key);
        https.addHeader("Content-Type", "application/json");

        String payload = "{\"model\":\"tts-1\",\"voice\":\"nova\",\"input\":\"" + textContent + "\"}";

        int httpResponseCode = https.POST(payload);

        if (httpResponseCode > 0) {
            if (httpResponseCode == HTTP_CODE_OK) {
                File audioFile = SD.open(filePath, FILE_WRITE);
                if (audioFile) {
                    if (https.writeToStream(&audioFile) > 0) {
                        audioFile.close();
                        Serial.println("Audio saved to " + String(filePath));
                    } else {
                        Serial.println("Error writing to audio file.");
                    }
                } else {
                    Serial.println("Failed to create audio file.");
                }
            } else {
                Serial.printf("Received unexpected HTTP response code: %d\n", httpResponseCode);
            }
        } else {
            Serial.printf("Error on HTTP request: %s\n", https.errorToString(httpResponseCode).c_str());
        }
        https.end(); // Ensures cleanup after handling the response
    } else {
        Serial.println("Failed to begin HTTPS connection");
        https.end(); // Cleanup if connection initiation fails
    }
}


// Internal function to upload audio file for the Speech to Text (STT) feature
String uploadAudioFile(const char* filename) {
    File file = SD.open(filename);
    if (!file) {
        Serial.println("Failed to open audio file!");
        return "";
    }
    
    size_t fileSize = file.size();
    file.close();
    
    Serial.printf("Audio file size: %d bytes\n", fileSize);
    
    // Skip certificate verification
    client.setInsecure();
    client.setTimeout(30);  // Timeout specified in seconds
    
    Serial.println("Connecting to OpenAI API...");
    if (!client.connect("api.openai.com", 443)) {
        Serial.println("Connection failed!");
        return "";
    }
    Serial.println("Connected to API endpoint");
    
    String boundary = "Boundary" + String(random(0xFFFF), HEX);
    ChunkedUploader uploader(&client, boundary);
    
    if (!uploader.begin(filename, fileSize)) {
        client.stop();
        return "";
    }
    
    Serial.println("Uploading file in chunks...");
    int chunks = 0;
    while (uploader.uploadChunk()) {
        chunks++;
        if (chunks % 10 == 0) {
            Serial.printf("Uploaded %d chunks\n", chunks);
            delay(1);  // Small delay to prevent watchdog triggers
        }
    }
    
    uploader.finish();
    Serial.println("Upload complete, waiting for response...");
    
    String response = uploader.readResponse();
    client.stop();
    
    int jsonStart = response.indexOf("\r\n\r\n") + 4;
    return response.substring(jsonStart);
}

// Function to store converted Speech to Text
void convertSpeechToText(const char *inputFile, const char *outputFile) {  
    Serial.println("Starting speech to text conversion.");
    
    String response = uploadAudioFile(inputFile);
    if (response.length() == 0) {
        Serial.println("Failed to get response from OpenAI STT API.");
        return;
    }
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, response);
    
    if (error) {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
        Serial.println("Raw response:");
        Serial.println(response);
        return;
    }
    
    const char* transcription = doc["text"];
    if (transcription) {
        File file = SD.open(outputFile, FILE_WRITE);
        if (!file) {
            Serial.println("Failed to open output file!");
            return;
        }
        
        file.print(transcription);
        file.close();
        Serial.println("Transcription saved successfully!");
        Serial.println("Transcribed text:");
        Serial.println(transcription);
    } else {
        Serial.println("No transcription in response!");
        Serial.println("Raw response:");
        Serial.println(response);
    }
}

String generateStory(const char* fullStoryPath, const char* storyOnlyPath, String location) {
    // Use "university" as default location if empty
    if (location.isEmpty() || location.equals("Unknown Location")) {
        location = "university";
        Serial.println("Using default location: university");
    }

    // Create the complete URL
    String url = String(gemini_url) + "?key=" + String(gemini_api_key);
    
    Serial.println("Connecting to Gemini API...");
    
    if (!http_client.begin(url)) {
        Serial.println("Connection to API failed!");
        http_client.end(); // Frees resources before returning
        return "Error: Failed to connect to API";
    }

    try {
        // Set headers
        http_client.addHeader("Content-Type", "application/json");

        // Create the prompt with proper escaping
        String prompt = "I want you to generate a very short story prompt (in less than thirty words) "
                       "that can be used as the base of a story that children can build on. "
                       "I will provide a location/address, so the story has to be built around that. "
                       "You have to start like this: \"Hmmm... Seems that we are at " + location + 
                       ". Let me create a plot around this: \", and continue with a short story prompt. "
                       "We are in Australia, so it has to be Australia-centric.";

        // Build request JSON
        StaticJsonDocument<2048> requestDoc;
        JsonArray contents = requestDoc.createNestedArray("contents");
        JsonObject content = contents.createNestedObject();
        JsonArray parts = content.createNestedArray("parts");
        JsonObject part = parts.createNestedObject();
        part["text"] = prompt;

        // Serialize with proper buffer
        String requestBody;
        serializeJson(requestDoc, requestBody);

        Serial.println("Sending request...");

        // Make the request
        int httpCode = http_client.POST(requestBody);

        if (httpCode != 200) {
            String error = http_client.getString();
            Serial.printf("HTTP error %d: %s\n", httpCode, error.c_str());
            http_client.end(); // Frees resources before returning
            return "Error: HTTP " + String(httpCode) + " - " + error;
        }

        // Get and parse response
        String response = http_client.getString();
        
        StaticJsonDocument<4096> responseDoc;
        DeserializationError error = deserializeJson(responseDoc, response);
        
        if (error) {
            Serial.printf("JSON parsing failed: %s\n", error.c_str());
            http_client.end(); // Frees resources before returning
            return "Error: JSON parsing failed - " + String(error.c_str());
        }

        // Extract story with better error handling
        String fullStory;
        if (responseDoc.containsKey("candidates") && 
            responseDoc["candidates"][0].containsKey("content") &&
            responseDoc["candidates"][0]["content"].containsKey("parts") &&
            responseDoc["candidates"][0]["content"]["parts"][0].containsKey("text")) {
            
            fullStory = responseDoc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
        } else {
            Serial.println("Invalid response structure");
            http_client.end(); // Frees resources before returning
            return "Error: Invalid API response structure";
        }

        // Process the story text
        int colonPos = fullStory.indexOf(':');
        String storyOnly = "";
        if (colonPos != -1) {
            storyOnly = fullStory.substring(colonPos + 1);
            storyOnly.trim();
        } else {
            storyOnly = fullStory; // Fallback if no colon found
        }

        // Save to SD card with error handling
        if (!writeResponseToSD(fullStory, fullStoryPath)) {
            Serial.println("Warning: Failed to save full story to SD");
        }

        if (!writeResponseToSD(storyOnly, storyOnlyPath)) {
            Serial.println("Warning: Failed to save story-only version to SD");
        }

        return fullStory;
        
    } catch (const std::exception& e) {
        Serial.printf("Exception caught: %s\n", e.what());
        return "Error: Exception occurred - " + String(e.what());
    }
    
    // Ensure cleanup
    http_client.end();
}

HTTPClient http;
StaticJsonDocument<4096> doc;

String createReverseGeocodeUrl(float latitude, float longitude) {
    return "https://maps.googleapis.com/maps/api/geocode/json"
           "?latlng=" + String(latitude, 6) + "," + String(longitude, 6) +
           "&key=" + maps_api_key;
}

bool makeHttpRequest(const String& url) {
    if(!WiFi.isConnected()) {
        Serial.println("Error: WiFi not connected");
        return false;
    }

    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode != HTTP_CODE_OK) {
        http.end();
        return false;
    }
    
    String payload = http.getString();
    http.end();
    
    doc.clear();
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        return false;
    }

    const char* status = doc["status"];
    if (!status || strcmp(status, "OK") != 0) {
        return false;
    }
    
    return true;
}

float convertToDecimalDegrees(String coord, String direction) {
  // Extract degrees and minutes from the coordinate string
  double rawCoord = coord.toDouble();
  int degrees = (int)(rawCoord / 100);
  double minutes = rawCoord - (degrees * 100);

  // Convert to decimal degrees
  float decimalDegrees = degrees + (minutes / 60);

  // Adjust sign based on direction (S/W should be negative)
  if (direction == "S" || direction == "W") {
    decimalDegrees = -decimalDegrees;
  }
  
  return decimalDegrees;
}

float getGPSData(String dataType) {
  String nmeaData = "";
  String lat, latDir, lon, lonDir;

  // Read one line of NMEA data
  while (GPS.available() > 0) {
    char c = GPS.read();
    if (c == '\n') {
      break;  // End of line
    }
    nmeaData += c;
  }

  // Check if the line contains GPGGA or GPRMC sentence
  if (nmeaData.startsWith("$GPGGA") || nmeaData.startsWith("$GPRMC")) {
    // Split NMEA sentence by commas
    int commaIndex = 0;
    String nmeaFields[15];  // Array to store fields of NMEA sentence
    int fieldIndex = 0;

    while (fieldIndex < 15) {
      commaIndex = nmeaData.indexOf(',', commaIndex + 1);
      if (commaIndex == -1) break;
      nmeaFields[fieldIndex++] = nmeaData.substring(commaIndex + 1, nmeaData.indexOf(',', commaIndex + 1));
    }

    // Extract latitude and longitude fields
    lat = nmeaFields[1];
    latDir = nmeaFields[2];
    lon = nmeaFields[3];
    lonDir = nmeaFields[4];
  }

  // Return latitude or longitude as a float
  if (dataType == "latitude") {
    return convertToDecimalDegrees(lat, latDir);
  } else if (dataType == "longitude") {
    return convertToDecimalDegrees(lon, lonDir);
  }

  return 0.0;  // Return 0.0 if no valid data
}

// This function uses the Google Maps API to obtain the name/address of the current location by providing coordinates 
String getPlaceName(float latitude, float longitude) {
    String reverseGeocodeUrl = createReverseGeocodeUrl(latitude, longitude);
    
    if (!makeHttpRequest(reverseGeocodeUrl)) {
        return "Unknown Location";
    }

    JsonArray results = doc["results"];
    if (!results.isNull() && results.size() > 0) {
        // Try to get the name from the first result
        const char* name = results[0]["name"];
        if (name) {
            return String(name);
        }
        
        // If no name, try to get the first line of the formatted address
        const char* address = results[0]["formatted_address"];
        if (address) {
            String fullAddress = String(address);
            int commaPos = fullAddress.indexOf(',');
            if (commaPos > 0) {
                return fullAddress.substring(0, commaPos);
            }
            return fullAddress;
        }
    }
    
    return "Some unknown location"; // Returns this so as to ensure the generateStory function works regardless of whether the GPS has a lock or not
}

//------------------------------------------------------------------------------------------

// Function to read the file and extract the rating from Gemini's evaluation
int readRatingFromFeedback(const char* filename) {
    // Open the feedback file on the SD card
    File file = SD.open(filename);
    if (!file) {
        Serial.println("Failed to open file!");
        return -1;
    }

    String content = "";
    while (file.available()) {
        content += (char)file.read();
    }
    file.close();

    int position = content.indexOf(" out of 10");
    if (position == -1) {
        Serial.println("Rating pattern not found!");
        return -1;
    }

    // Find start of the number by backtracking
    int start = position - 1;
    while (start >= 0 && isDigit(content.charAt(start))) {
        start--;
    }
    start++;  // Move to the first digit of the rating

    // Extract the rating substring and convert it to an integer
    String ratingStr = content.substring(start, position);
    int rating = ratingStr.toInt();

    Serial.print("Rating found: ");
    Serial.println(rating);
    return rating;
}

// Finds the player with the highest rating
int findHighestRatedPlayer() {
    int highestRating = -1; // Initialize the highest rating
    int highestPlayer = -1; // Initialize the highest player number

    // Cycle through all player rating files from p1_rating to p8_rating
    for (int player = 1; player <= 4; player++) {
        String filename = String("/p") + player + "_rating.txt"; // Create filename for each player
        File file = SD.open(filename.c_str()); // Open the corresponding file

        if (!file) {
            Serial.print("Failed to open file for player ");
            Serial.println(player);
            continue; // Skip to the next player if the file doesn't open
        }

        // Read the number from the file
        if (file.available()) {
            String content = file.readStringUntil('\n'); // Read the line
            int rating = content.toInt(); // Convert the string content to an integer
            
            // Check if this rating is the highest
            if (rating > highestRating) {
                highestRating = rating; // Update highest rating
                highestPlayer = player; // Update highest player number
            }
        }
        
        file.close(); // Close the file
    }

    return highestPlayer; // Return the player number with the highest rating
}

// Function to create files for score-keeping
void createFileWithZero(const char* filename) {
    // Open the file for writing (this will create the file if it doesn't exist)
    File file = SD.open(filename, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to create file!");
        return; // Exit the function if file creation fails
    }

    // Write the value '0' to the file
    file.println(0);
    file.close(); // Close the file after writing

    Serial.println("File created with value 0.");
}

// Function to add to the continuing story
void addContextToStory(const char* story_context, const char* transc_file) {
    // Open the master story context file in append mode
    File context = SD.open(story_context, FILE_APPEND);
    if (!context) {
        Serial.println("Failed to open destination file for appending!");
        return;
    }

    // Open the source file in read mode
    File contribution = SD.open(transc_file, FILE_READ);
    if (!contribution) {
        Serial.println("Failed to open source file for reading!");
        context.close();  // Close destination file if source file fails to open
        return;
    }

    // Append the contents of the source file to the destination file
    while (contribution.available()) {
        context.write(contribution.read());  // Read from source and write to destination
    }

    // Close both files
    contribution.close();
    context.close();
    Serial.println("Player contribution copied to master story file.");
}

// Function to store player's rating
void addNumberToFile(int numberToAdd, const char* filePath) {
  // Open the file in read mode to get the current number
  File file = SD.open(filePath, FILE_READ);
  
  int currentNumber = 0; // Default to 0 if the file is empty or unreadable
  if (file) {
    String content = file.readString(); // Read the file contents
    currentNumber = content.toInt(); // Convert the contents to an integer
    file.close();
  } else {
    Serial.println("Failed to open file for reading!");
  }

  // Calculate the new total
  int newTotal = currentNumber + numberToAdd;

  // Open the file in write mode to overwrite with the new total
  file = SD.open(filePath, FILE_WRITE);
  if (file) {
    file.print(newTotal); // Write the new total to the file
    file.close();
    Serial.print("New total written to file: ");
    Serial.println(newTotal);
  } else {
    Serial.println("Failed to open file for writing!");
  }
}

//----------------------------------------------------------------------------------

// Construct an audio object
Audio audio;

//---------------------------------------------------------------------------------------------

// Function to generate winner's feedback
String evaluateWinner(const char* base_prompt, const char* story_path, const char* player_contribution1, const char* player_contribution2, const char* evaluation, int playerNumber) {

  // Read the base story prompt 
  String baseprompt = readTextFromSD(base_prompt);

  // Read the story that has been contributed so far
  String storySoFar = readTextFromSD(story_path);
  
  // Read the contributions of the winning player (round 1)
  String playerStory1 = readTextFromSD(player_contribution1);

  // Read the contributions of the winning player (round 2)
  String playerStory2 = readTextFromSD(player_contribution2);

  String consolidatedStory = playerStory1 + playerStory2; 

  // Construct the complete URL with API key
  String url = String(gemini_url) + "?key=" + String(gemini_api_key);
  
  Serial.println("Connecting to Gemini API...");
  Serial.println("URL: " + url);
  
  if (!http_client.begin(url)) {
    Serial.println("Connection to API failed!");
    http_client.end(); // Frees resources before returning
    return "";
  }

  // Set the request headers
  http_client.addHeader("Content-Type", "application/json");

  // Create a JSON document for the request
  JsonDocument requestDoc;
  
    // Build the JSON structure
requestDoc["contents"][0]["parts"][0]["text"] = 
    requestDoc["contents"][0]["parts"][0]["text"] = 
    String("We hosted a collaborative storytelling contest for children. It consists of four players. A base story prompt was given. ") +
    "Players take turns to speak. For the sake of fairness, each player has to ensure that they leave sufficient room for others to continue the story from where they stop. " +
    "We assess the players on the basis of how well they speak, their articulation, contribution to the plot, usage of filler words, creativity, imagination, the story snippet they contributed, and whether they left sufficient room for others to continue the story from where they stopped. " +
    "This was the base prompt we gave to the players (generated by the host): \"" + baseprompt + "\". This is the collaborative story that was stitched by the players together from the base prompt: \"" + storySoFar + "\". " +
    "And this is player " + String(playerNumber) + "'s contribution to the consolidated story (the winner, whom we chose): \"" + consolidatedStory + "\". Assess the winner player’s contribution to the story, specify why he/she won on the basis " +
    "of the aforesaid factors, and provide a short constructive feedback in text that can be spoken in not more than two minutes. Use encouraging words and be enthusiastic. " +
    "Write everything in a single paragraph. Don't use any special characters. Give a small buildup before announcing the number of the player who won. ";


  // Serialize the request
  String requestBody;
  serializeJson(requestDoc, requestBody);

  // Debug print the request body
  Serial.println("Sending request with body:");
  Serial.println(requestBody);

  // Send the request
  int httpCode = http_client.POST(requestBody);

  // Debug print the response code
  Serial.print("HTTP Response code: ");
  Serial.println(httpCode);

  if (httpCode != 200) {
    String error = http_client.getString();
    Serial.println("Error response:");
    Serial.println(error);
    http_client.end();
    return "Error: HTTP " + String(httpCode) + " - " + error;
  }

  // Get the response
  String response = http_client.getString();
  Serial.println("Raw response:");
  Serial.println(response);
  
  // Parse the response
  JsonDocument responseDoc;
  DeserializationError error = deserializeJson(responseDoc, response);
  
  if (error) {
    Serial.print("JSON parsing failed: ");
    Serial.println(error.c_str());
    http_client.end();
    return "Error: JSON parsing failed - " + String(error.c_str());
  }

  // Extract the feedback text
  String feedback;
  if (responseDoc["candidates"][0]["content"]["parts"][0]["text"].is<const char*>()) {
    feedback = responseDoc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
  } else {
    Serial.println("Unexpected response format");
    Serial.println(response);
    feedback = "Error: Unable to parse response - unexpected format";
  }

  Serial.println("Feedback: " + feedback);
  
  // Write the winner feedback to a file on the SD card
  if (writeResponseToSD(feedback, evaluation)) {
    Serial.println("Winner feedback saved to SD card: " + String(evaluation));
  } else {
    Serial.println("Failed to save winner feedback to SD card");
  }

  http_client.end();
  return evaluation;
}

// Define LED pin to act as an indicator time remaining
const int LED = 15;

// Function to store audio clip at the specified file path
bool recordAudio(const String& filePath) {
    // Local configuration constants
    const int I2S_SAMPLE_RATE = 16000;
    const int BYTES_PER_SAMPLE = 2; // 16-bit samples
    const int BUFFER_SIZE = 1024;

    // Initialize I2S and SD
    const i2s_port_t I2S_PORT = I2S_NUM_1;

    pinMode(LED, OUTPUT);
    // Set up PWM for LED dimming
    ledcAttachPin(LED, 0);  // Channel 0 for PWM
    ledcSetup(0, 5000, 8);      // 5 kHz PWM, 8-bit resolution

    // Create local configuration structs
    const i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = BUFFER_SIZE,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    const i2s_pin_config_t i2s_mic_pins = {
        .bck_io_num = 12,     // Bit Clock
        .ws_io_num = 4,       // Word Select
        .data_out_num = -1,   // not used
        .data_in_num = 17     // Data-in from mic
    };

    // Uninstall existing driver if any
    i2s_driver_uninstall(I2S_PORT);

    // Install I2S driver
    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("Failed to install I2S driver: %d\n", err);
        return false;
    }
    
    Serial.println("Installed I2S driver.");

    // Set I2S pins
    err = i2s_set_pin(I2S_PORT, &i2s_mic_pins);
    if (err != ESP_OK) {
        Serial.printf("Failed to set I2S pins: %d\n", err);
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }
    
    Serial.println("I2S pins set.");

    // Zero DMA buffers
    i2s_zero_dma_buffer(I2S_PORT);

    // Start I2S
    i2s_start(I2S_PORT);

    // Remove existing file if it exists
    if (SD.exists(filePath)) {
        SD.remove(filePath);
    }

    // Open file for writing
    File audioFile = SD.open(filePath, FILE_WRITE);
    if (!audioFile) {
        Serial.println("Failed to open file for writing");
        i2s_stop(I2S_PORT);
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }

    // Prepare WAV header
    const int waveDataSize = I2S_SAMPLE_RATE * 30 * BYTES_PER_SAMPLE;
    byte wav_header[44];

    // RIFF chunk descriptor
    wav_header[0] = 'R'; wav_header[1] = 'I'; wav_header[2] = 'F'; wav_header[3] = 'F';
    int totalDataSize = waveDataSize + 36;
    wav_header[4] = (byte)(totalDataSize & 0xFF);
    wav_header[5] = (byte)((totalDataSize >> 8) & 0xFF);
    wav_header[6] = (byte)((totalDataSize >> 16) & 0xFF);
    wav_header[7] = (byte)((totalDataSize >> 24) & 0xFF);
    wav_header[8] = 'W'; wav_header[9] = 'A'; wav_header[10] = 'V'; wav_header[11] = 'E';
    wav_header[12] = 'f'; wav_header[13] = 'm'; wav_header[14] = 't'; wav_header[15] = ' ';
    wav_header[16] = 16; wav_header[17] = 0; wav_header[18] = 0; wav_header[19] = 0;
    wav_header[20] = 1; wav_header[21] = 0; // Audio format (PCM)
    wav_header[22] = 1; wav_header[23] = 0; // Mono channel
    wav_header[24] = (byte)(I2S_SAMPLE_RATE & 0xFF);
    wav_header[25] = (byte)((I2S_SAMPLE_RATE >> 8) & 0xFF);
    wav_header[26] = (byte)((I2S_SAMPLE_RATE >> 16) & 0xFF);
    wav_header[27] = (byte)((I2S_SAMPLE_RATE >> 24) & 0xFF);
    int byteRate = I2S_SAMPLE_RATE * 1 * BYTES_PER_SAMPLE;
    wav_header[28] = (byte)(byteRate & 0xFF);
    wav_header[29] = (byte)((byteRate >> 8) & 0xFF);
    wav_header[30] = (byte)((byteRate >> 16) & 0xFF);
    wav_header[31] = (byte)((byteRate >> 24) & 0xFF);
    wav_header[32] = 1 * BYTES_PER_SAMPLE; // Block align
    wav_header[33] = 0;
    wav_header[34] = BYTES_PER_SAMPLE * 8; // Bits per sample
    wav_header[35] = 0;
    wav_header[36] = 'd'; wav_header[37] = 'a'; wav_header[38] = 't'; wav_header[39] = 'a';
    wav_header[40] = (byte)(waveDataSize & 0xFF);
    wav_header[41] = (byte)((waveDataSize >> 8) & 0xFF);
    wav_header[42] = (byte)((waveDataSize >> 16) & 0xFF);
    wav_header[43] = (byte)((waveDataSize >> 24) & 0xFF);

    // Write WAV header
    audioFile.write(wav_header, sizeof(wav_header));

    // Allocate buffers
    uint8_t* i2s_read_buff = (uint8_t*)calloc(BUFFER_SIZE * 4, sizeof(uint8_t));
    int16_t* processed_samples = (int16_t*)calloc(BUFFER_SIZE, sizeof(int16_t));

    if (!i2s_read_buff || !processed_samples) {
        Serial.println("Failed to allocate buffers");
        audioFile.close();
        i2s_stop(I2S_PORT);
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }

    // Recording loop
    uint32_t startTime = millis();
    const uint32_t recordingDuration = 30 * 1000; // 30 seconds
    size_t bytesRead = 0;

    Serial.println("Recording started!");
    while (millis() - startTime < 30 * 1000) {
        if (i2s_read(I2S_PORT, i2s_read_buff, BUFFER_SIZE * 4, &bytesRead, portMAX_DELAY) == ESP_OK) {
            if (bytesRead > 0) {
                int32_t* samples_32 = (int32_t*)i2s_read_buff;
                for (int i = 0; i < (bytesRead / 4); i++) { // Adjust based on sample size
                    processed_samples[i] = samples_32[i] >> 16; // Convert to 16-bit
                }
                audioFile.write((const uint8_t*)processed_samples, bytesRead / 2); // Write processed data
            }
        }
        // Calculate LED brightness based on elapsed time
        float remainingTimeRatio = 1.0 - float(millis() - startTime) / recordingDuration;
        int brightness = int(remainingTimeRatio * 255); // Convert ratio to PWM (0-255)
        ledcWrite(0, brightness); // Adjust LED brightness
    }

    Serial.println("Recording stopped.");

    ledcWrite(0, 0); // Adjust LED brightness

    // Cleanup
    free(i2s_read_buff);
    free(processed_samples);
    audioFile.close();

    // Stop I2S and uninstall driver
    i2s_stop(I2S_PORT);
    i2s_driver_uninstall(I2S_PORT);
    
    return true;
}

//------------------------------------------------------------------------

// Function to delete all files at the end
void deleteGameFiles() {
    // List of all file paths created during the game
    const char* filesToDelete[] = {
        p1_response1, p1_response2, p2_response1, p2_response2,
        p3_response1, p3_response2, p4_response1, p4_response2,
        p1_trans1, p1_trans2, p2_trans1, p2_trans2,
        p3_trans1, p3_trans2, p4_trans1, p4_trans2,
        p1_eval1, p1_eval2, p2_eval1, p2_eval2,
        p3_eval1, p3_eval2, p4_eval1, p4_eval2,
        p1_feed1, p1_feed2, p2_feed1, p2_feed2,
        p3_feed1, p3_feed2, p4_feed1, p4_feed2,
        winner_feedback, winner_feedback_speech,
        fullstoryTTS, base_story, storySoFar
    };

    // Loop through each file and delete it if it exists
    for (int i = 0; i < sizeof(filesToDelete) / sizeof(filesToDelete[0]); i++) {
        if (SD.exists(filesToDelete[i])) {
            SD.remove(filesToDelete[i]);
            Serial.print("Deleted file: ");
            Serial.println(filesToDelete[i]);
        } else {
            Serial.print("File not found: ");
            Serial.println(filesToDelete[i]);
        }
    }
}

void setup() {

    // Set baud rate
    Serial.begin(115200);
    
    Serial.println("Starting up..."); 

    // Initiate WiFi connection
    connectToWiFi(); 

    // Initiate SPI connection to SD card
    initSDCard(); 

    GPS.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX); // initialising the GPS with the following configuration: 9600 baud rate, serial protocol (8N1 kind)

    while(!GPS); // wait for the GPS receiver to initialise
    
    Serial.println("Initialising I2S speaker setup");
    // Initialise I2S speaker setup
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(VOLUME);
    Serial.println("I2S speaker setup complete!");

}

void loop() {
  
   // Check and create rating files if they don’t exist
    if (!SD.exists(p1_rating)) createFileWithZero(p1_rating);
    if (!SD.exists(p2_rating)) createFileWithZero(p2_rating);
    if (!SD.exists(p3_rating)) createFileWithZero(p3_rating);
    if (!SD.exists(p4_rating)) createFileWithZero(p4_rating);

  // Introductory announcement playback
  audio.connecttoFS(SD, "/introduction.mp3");
  while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
    audio.stopSong();
    Serial.println("Introduction over!");

    delay(1000);

  // Instruction announcement playback
  audio.connecttoFS(SD, "/rules.mp3");
  while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
    audio.stopSong();
    Serial.println("Rules have been narrated!");

  // One second delay
  delay(1000);

  float latitude = getGPSData("latitude"); // Invoke the GPS function to obtain the latitude
  float longitude = getGPSData("longitude"); // Invoke the GPS function to obtain the longitude

  // Obtain the name/address of the device's current location
  String location = getPlaceName(latitude, longitude);

 // Generate the story prompt
 generateStory(fullstoryTTS, base_story, location);
 // fullstoryTTS points to the file path of the prompt to be read aloud: we use this to generate the speech
 // base_story points to the file path of the base story: stores the base prompt

 const char* first_prompt="/first_prompt.mp3"; // points to the file path of the speech generated
 convertTextToSpeech(fullstoryTTS, first_prompt); // converts text to speech 
 
 // Announce prompt
 audio.connecttoFS(SD, first_prompt); // tether to audio clip
 while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
    audio.stopSong();
 delay(2000);

// Player 1 Round 1 

// Start cue
audio.connecttoFS(SD, "/start.mp3");
while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
    audio.stopSong();
delay(500);

//Record response of player 1
recordAudio(p1_response1);

 // Stop cue
 audio.connecttoFS(SD, "/stop.mp3");
 while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
    audio.stopSong();
delay(500);

 // Convert player 1's speech to text
 convertSpeechToText(p1_response1, p1_trans1);

 // Evaluate player 1's response
 evaluateFContribution(base_story, p1_trans1, p1_eval1);

 // Add player 1's contribution to the story context
 addContextToStory(storySoFar, p1_trans1);

 // Read and store player 1's rating to their rating file
 addNumberToFile(readRatingFromFeedback(p1_eval1), p1_rating);

 // Convert player 1's feedback to speech
 convertTextToSpeech(p1_eval1, p1_feed1);

 audio.connecttoFS(SD, p1_feed1);
 while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
audio.stopSong();
 delay(3000);

// Player 2 Round 1

// Start cue
audio.connecttoFS(SD, "/start.mp3");
while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
audio.stopSong();
    delay(500);

//Record response of player 2
recordAudio(p2_response1);

 // Stop cue
 audio.connecttoFS(SD, "/stop.mp3");
 while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
    audio.stopSong();

delay(500);
 // Convert player 2's speech to text
 convertSpeechToText(p2_response1, p2_trans1);

 // Evaluate player 2's response
 evaluateContribution(base_story, storySoFar, p2_trans1, p2_eval1);

 // Add player 2's contribution to the story context
 addContextToStory(storySoFar, p2_trans1);

 // Read and store player 2's rating to their rating file
 addNumberToFile(readRatingFromFeedback(p2_eval1), p2_rating);

 // Convert player 2's feedback to speech
 convertTextToSpeech(p2_eval1, p2_feed1);

 audio.connecttoFS(SD, p2_feed1);
 while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
    audio.stopSong();

 delay(3000);

 // Player 3 Round 1

// Start cue
audio.connecttoFS(SD, "/start.mp3");
while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
    audio.stopSong();

    delay(500);

// Record response of player 3
recordAudio(p3_response1);

 // Stop cue
 audio.connecttoFS(SD, "/stop.mp3");
 while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
audio.stopSong();
delay(500);
 // Convert player 3's speech to text
 convertSpeechToText(p3_response1, p3_trans1);

 // Evaluate player 3's response
 evaluateContribution(base_story, storySoFar, p3_trans1, p3_eval1);

 // Add player 3's contribution to the story context
 addContextToStory(storySoFar, p3_trans1);

 // Read and store player 3's rating to their rating file
 addNumberToFile(readRatingFromFeedback(p3_eval1), p3_rating);

 // Convert player 3's feedback to speech
 convertTextToSpeech(p3_eval1, p3_feed1);

 audio.connecttoFS(SD, p3_feed1);
 while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
audio.stopSong();
 delay(3000);

// Player 4 Round 1

// Start cue
audio.connecttoFS(SD, "/start.mp3");
while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
    audio.stopSong();
    delay(500);

//Record response of player 4
recordAudio(p4_response1);

 // Stop cue
 audio.connecttoFS(SD, "/stop.mp3");
while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
audio.stopSong();
    delay(500);

 // Convert player 4's speech to text
 convertSpeechToText(p4_response1, p4_trans1);

 // Evaluate player 4's response
 evaluateContribution(base_story, storySoFar, p4_trans1, p4_eval1);

 // Add player 4's contribution to the story context
 addContextToStory(storySoFar, p4_trans1);

 // Read and store player 4's rating to their rating file
 addNumberToFile(readRatingFromFeedback(p4_eval1), p4_rating);

 // Convert player 4's feedback to speech
 convertTextToSpeech(p4_eval1, p4_feed1);

 audio.connecttoFS(SD, p4_feed1);
while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
audio.stopSong();
 delay(3000);

// Player 1 Round 2

// Start cue
audio.connecttoFS(SD, "/start.mp3");
while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
audio.stopSong();
    delay(500);

//Record second response of player 1
recordAudio(p1_response2);

 // Stop cue
 audio.connecttoFS(SD, "/stop.mp3");
while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
audio.stopSong();
    delay(500);

 // Convert player 1's second speech to text
 convertSpeechToText(p1_response2, p1_trans2);

 // Evaluate player 1's second response
 evaluateContribution(base_story, storySoFar, p1_trans2, p1_eval2);

 // Add player 1's second contribution to the story context
 addContextToStory(storySoFar, p1_trans2);

 // Read and store player 1's second rating to their rating file
 addNumberToFile(readRatingFromFeedback(p1_eval2), p1_rating);

 // Convert player 1's second feedback to speech
 convertTextToSpeech(p1_eval2, p1_feed2);

 audio.connecttoFS(SD, p1_feed2);
while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
audio.stopSong();
 delay(3000);

 // Player 2 Round 2

// Start cue
audio.connecttoFS(SD, "/start.mp3");
while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
audio.stopSong();
    delay(500);

//Record second response of player 2
recordAudio(p2_response2);

 // Stop cue
 audio.connecttoFS(SD, "/stop.mp3");
 while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
audio.stopSong();
    delay(500);

 // Convert player 2's second speech to text
 convertSpeechToText(p2_response2, p2_trans2);

 // Evaluate player 2's second response
 evaluateContribution(base_story, storySoFar, p2_trans2, p2_eval2);

 // Add player 2's second contribution to the story context
 addContextToStory(storySoFar, p2_trans2);

 // Read and store player 2's second rating to their rating file
 addNumberToFile(readRatingFromFeedback(p2_eval2), p2_rating);

 // Convert player 2's second feedback to speech
 convertTextToSpeech(p2_eval2, p2_feed2);

 audio.connecttoFS(SD, p2_feed2);
while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
audio.stopSong();
 delay(3000);

// Player 3 Round 2

// Start cue
audio.connecttoFS(SD, "/start.mp3");
while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
audio.stopSong();
delay(500);
//Record second response of player 3
recordAudio(p3_response2);

 // Stop cue
 audio.connecttoFS(SD, "/stop.mp3");
while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
audio.stopSong();
    delay(500);

 // Convert player 3's second speech to text
 convertSpeechToText(p3_response2, p3_trans2);

 // Evaluate player 3's second response
 evaluateContribution(base_story, storySoFar, p3_trans2, p3_eval2);

 // Add player 3's second contribution to the story context
 addContextToStory(storySoFar, p3_trans2);

 // Read and store player 3's second rating to their rating file
 addNumberToFile(readRatingFromFeedback(p3_eval2), p3_rating);

 // Convert player 3's second feedback to speech
 convertTextToSpeech(p3_eval2, p3_feed2);

 audio.connecttoFS(SD, p3_feed2);
while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
audio.stopSong();
 delay(3000);

// Player 4 Round 2

// Start cue
audio.connecttoFS(SD, "/start.mp3");
while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
audio.stopSong();
    delay(500);

//Record second response of player 4
recordAudio(p4_response2);

 // Stop cue
 audio.connecttoFS(SD, "/stop.mp3");
while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
audio.stopSong();
    delay(500);

 // Convert player 4's second speech to text
 convertSpeechToText(p4_response2, p4_trans2);

 // Evaluate player 4's second response
 evaluateLContribution(base_story, storySoFar, p4_trans2, p4_eval2);

 // Add player 4's second contribution to the story context
 addContextToStory(storySoFar, p4_trans2);

 // Read and store player 4's second rating to their rating file
 addNumberToFile(readRatingFromFeedback(p4_eval2), p4_rating);

 // Convert player 4's second feedback to speech
 convertTextToSpeech(p4_eval2, p4_feed2);

 audio.connecttoFS(SD, p4_feed2);
while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
audio.stopSong();
 delay(3000);

audio.connecttoFS(SD, "/deliberation.mp3");
while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
audio.stopSong();
delay(3000);

int bestPlayer = findHighestRatedPlayer();

if(bestPlayer==1)
{
  evaluateWinner(base_story, storySoFar, p1_trans1, p1_trans2, winner_feedback, 1);
}
else if(bestPlayer==2)
{
  evaluateWinner(base_story, storySoFar, p2_trans1, p2_trans2, winner_feedback, 2);
}
else if(bestPlayer==3)
{
  evaluateWinner(base_story, storySoFar, p3_trans1, p3_trans2, winner_feedback, 3);
}
else if(bestPlayer==4)
{
  evaluateWinner(base_story, storySoFar, p4_trans1, p4_trans2, winner_feedback, 4);
}

convertTextToSpeech(winner_feedback, winner_feedback_speech);
audio.connecttoFS(SD, winner_feedback_speech);
while (audio.isRunning()) {
        audio.loop();  // Continue playback
        delay(IN_AUDIO_PAUSE);     // Small delay to prevent a tight loop
    }
audio.stopSong();
Serial.println("Game over!");

delay(10000);

Serial.println("Deleting game files!");
deleteGameFiles(); // Deleting all the stored player data
delay(40000);
//All good things come to an end, alas
}