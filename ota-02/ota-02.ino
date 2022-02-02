#include <TFT_eSPI.h>        //LCD函式庫
#include <Button2.h>

#include <WiFi.h>
#include <Update.h>

//TFT & button
TFT_eSPI tft = TFT_eSPI();

#define BUTTON_A_PIN  0  //按鍵A，PIN 0
#define BUTTON_B_PIN  35 //按鍵B，PIN 35
Button2 buttonA = Button2(BUTTON_A_PIN);
Button2 buttonB = Button2(BUTTON_B_PIN);


//WiFi
WiFiClient client;

// Variables to validate
// response from S3
int contentLength = 0;
bool isValidContentType = false;

// Your SSID and PSWD that the chip needs
// to connect to
const char* SSID = "newpos";
const char* PSWD = "ho123456789";

// S3 Bucket Config
String host = "ble.com.tw"; // Host => bucket-name.s3.region.amazonaws.com
int port = 80; // Non https. For HTTPS 443. As of today, HTTPS doesn't work.
String bin = "/AppUpdate/ota-01.ino.esp32.bin"; // bin file name with a slash in front.




// Utility to extract header value from headers
String getHeaderValue(String header, String headerName) {
  return header.substring(strlen(headerName.c_str()));
}

// OTA Logic
void execOTA() {
  Serial.println("正在連線到: " + String(host));
  // Connect to 網站
  if (client.connect(host.c_str(), port)) {
    // Connection Succeed.
    // Fecthing the bin
    Serial.println("二進位檔名: " + String(bin));

    // Get the contents of the bin file
    client.print(String("GET ") + bin + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Cache-Control: no-cache\r\n" +
                 "Connection: close\r\n\r\n");

    // Check what is being sent
    //    Serial.print(String("GET ") + bin + " HTTP/1.1\r\n" +
    //                 "Host: " + host + "\r\n" +
    //                 "Cache-Control: no-cache\r\n" +
    //                 "Connection: close\r\n\r\n");

    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println("Client Timeout !");
        client.stop();
        return;
      }
    }
    // Once the response is available,
    // check stuff

    /*
       Response Structure
        HTTP/1.1 200 OK
        x-amz-id-2: NVKxnU1aIQMmpGKhSwpCBh8y2JPbak18QLIfE+OiUDOos+7UftZKjtCFqrwsGOZRN5Zee0jpTd0=
        x-amz-request-id: 2D56B47560B764EC
        Date: Wed, 14 Jun 2017 03:33:59 GMT
        Last-Modified: Fri, 02 Jun 2017 14:50:11 GMT
        ETag: "d2afebbaaebc38cd669ce36727152af9"
        Accept-Ranges: bytes
        Content-Type: application/octet-stream
        Content-Length: 357280
        Server: AmazonS3

        {{BIN FILE CONTENTS}}

    */
    while (client.available()) {
      // read line till /n
      String line = client.readStringUntil('\n');
      // remove space, to check if the line is end of headers
      line.trim();

      // if the the line is empty,
      // this is end of headers
      // break the while and feed the
      // remaining `client` to the
      // Update.writeStream();
      if (!line.length()) {
        //headers ended
        break; // and get the OTA started
      }

      // Check if the HTTP Response is 200
      // else break and Exit Update
      if (line.startsWith("HTTP/1.1")) {
        if (line.indexOf("200") < 0) {
          Serial.println("Got a non 200 status code from server. Exiting OTA Update.");
          break;
        }
      }

      // extract headers here
      // Start with content length
      if (line.startsWith("Content-Length: ")) {
        contentLength = atoi((getHeaderValue(line, "Content-Length: ")).c_str());
        Serial.println("Got " + String(contentLength) + " bytes from server");
      }

      // Next, the content type
      if (line.startsWith("Content-Type: ")) {
        String contentType = getHeaderValue(line, "Content-Type: ");
        Serial.println("Got " + contentType + " payload.");
        if (contentType == "application/octet-stream") {
          isValidContentType = true;
        }
      }
    }
  } else {
    // Connect to S3 failed
    // May be try?
    // Probably a choppy network?
    Serial.println("Connection to " + String(host) + " failed. Please check your setup");
    // retry??
    // execOTA();
  }

  // Check what is the contentLength and if content type is `application/octet-stream`
  Serial.println("contentLength : " + String(contentLength) + ", isValidContentType : " + String(isValidContentType));

  // check contentLength and content type
  if (contentLength && isValidContentType) {
    // Check if there is enough to OTA Update
    bool canBegin = Update.begin(contentLength);

    // If yes, begin
    if (canBegin) {
      Serial.println("Begin OTA. This may take 2 - 5 mins to complete. Things might be quite for a while.. Patience!");
      // No activity would appear on the Serial monitor
      // So be patient. This may take 2 - 5mins to complete
      size_t written = Update.writeStream(client);

      if (written == contentLength) {
        Serial.println("Written : " + String(written) + " successfully");
      } else {
        Serial.println("Written only : " + String(written) + "/" + String(contentLength) + ". Retry?" );
        // retry??
        // execOTA();
      }

      if (Update.end()) {
        Serial.println("OTA done!");
        showstr2("OTA done!");    //Show文字
        if (Update.isFinished()) {
          Serial.println("Update successfully completed. Rebooting.");
          showstr2("Update successfully!! Rebooting.");    //Show文字
          delay(2000);
          showstr3();   //Show文字 .清空畫面
          ESP.restart();
        } else {
          Serial.println("Update not finished? Something went wrong!");
        }
      } else {
        Serial.println("Error Occurred. Error #: " + String(Update.getError()));
      }
    } else {
      // not enough space to begin OTA
      // Understand the partitions and
      // space availability
      Serial.println("Not enough space to begin OTA");
      client.flush();
    }
  } else {
    Serial.println("There was no content in the response");
    client.flush();
  }
}

void setup() {
  //Begin Serial
  Serial.begin(115200);
  delay(10);

  Serial.println("OTA02");
  //TFT init
  tft.begin();               // 初始化LCD
  tft.setRotation(1);  // landscape
  tft.fillScreen(TFT_BLACK); // 用全黑清除螢幕
  tft.setSwapBytes(true);
  tft.drawRect(0, 0, 240, 130, TFT_RED);//方形空心

  //文字
  showstr1("OTA02");

  buttonA.setPressedHandler(press);   //建立A按鍵按下Pressed的事件
  buttonA.setReleasedHandler(release);//建立A按鍵放開Released的事件
  buttonB.setPressedHandler(press);   //建立B按鍵按下Pressed的事件
  buttonB.setReleasedHandler(release);//建立B按鍵放開Released的事件

}

void loop() {
  // chill

  delay(20);

  buttonA.loop();  //重複按鍵的觸發設定
  buttonB.loop();
}


void press(Button2& btn) {
  if (btn == buttonA) {   //按下A按鍵 

    //SerialBT.write('A');


  } else if (btn == buttonB) {  //按下B按鍵 

    //SerialBT.write('B');
  }


}

void release(Button2& btn) {
  if (btn == buttonA) {          //放開按鍵A 

    //SerialBT.write('1');

    Serial.println("Connecting to " + String(SSID));

    showstr2("Connecting to " + String(SSID));    //Show文字

    // Connect to provided SSID and PSWD
    WiFi.begin(SSID, PSWD);

    // Wait for connection to establish
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print("."); // Keep the serial monitor lit!
      delay(500);
    }

    // Connection Succeed
    Serial.println("");
    Serial.println("Connected to " + String(SSID));



    // Execute OTA Update
    showstr2("Execute OTA Update");    //Show文字
    execOTA();



  } else if (btn == buttonB) {   //放開按鍵A 

    //SerialBT.write('2');
  }
}

void showstr1(String s1) {
  //文字
  tft.setFreeFont(&FreeSerifBold12pt7b);
  tft.setCursor(10, 70, 4);
  tft.setTextColor(TFT_BLUE);
  tft.setTextSize(2);
  tft.printf(s1.c_str());
}
void showstr2(String s1) {
  //文字
  tft.fillScreen(TFT_BLACK); // 用全黑清除螢幕
  tft.setCursor(10, 70, 2);
  tft.setTextColor(TFT_RED);
  tft.setTextSize(1);
  tft.printf(s1.c_str());
}
void showstr3() {
  //文字
  tft.fillScreen(TFT_BLACK); // 用全黑清除螢幕
}
