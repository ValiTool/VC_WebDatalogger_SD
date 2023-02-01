/*  Download/Upload files to your ESP32 + SD datalogger/sensor using wifi
    
 By My Circuits 2022, based on code of David Bird 2018 
   
 Requiered libraries:
 
 ESP32WebServer - https://github.com/Pedroalbuquerque/ESP32WebServer download and place in your Libraries folder
 
 Information from David Birds original code - not from My Circuits contribution (with my contribution/simplification to the code do whatever you wish, just mention us!):
  
 This software, the ideas and concepts is Copyright (c) David Bird 2018. All rights to this software are reserved.
 
 Any redistribution or reproduction of any part or all of the contents in any form is prohibited other than the following:
 1. You may print or download to a local hard disk extracts for your personal and non-commercial use only.
 2. You may copy the content to individual third parties for their personal use, but only if you acknowledge the author David Bird as the source of the material.
 3. You may not, except with my express written permission, distribute or commercially exploit the content.
 4. You may not transmit it or store it in any other website or other form of electronic retrieval system for commercial purposes.

 The above copyright ('as annotated') notice and this permission notice shall be included in all copies or substantial portions of the Software and where the
 software use is visible to an end-user.
 
 THE SOFTWARE IS PROVIDED "AS IS" FOR PRIVATE USE ONLY, IT IS NOT FOR COMMERCIAL USE IN WHOLE OR PART OR CONCEPT. FOR PERSONAL USE IT IS SUPPLIED WITHOUT WARRANTY 
 OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE AUTHOR OR COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 See more at http://www.dsbird.org.uk
 
*/
//Libraries
#include <WiFi.h> //Built-in
#include <WiFiUdp.h>             
#include <ESP32WebServer.h>    //https://github.com/Pedroalbuquerque/ESP32WebServer download and place in your Libraries folder
#include <ESPmDNS.h>
#include <FS.h>
#include <SD.h> 
#include <SPI.h>
#include <NTPClient.h>
//CSS Headers
#include "CSS.h" //Includes headers of the web and de style file

ESP32WebServer server(80); //Starts the web server at port 80, you can change this value if the port is unavalible.

//Wi Fi Credentials
const char* ssid = "WiFi_Fibertel_mev_2.4GHz";
const char* password = "x43c7765bs";

WiFiUDP ntpUDP;

#define servername "mcserver" //Define the name to your server...

/*PIN Asignmment*/

#define currentSensorPin 32
#define voltageSensorPin 33


// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionally you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
NTPClient timeClient(ntpUDP, "0.ubnt.pool.ntp.org", -10800, 60000);
//#define SD_pin 16 //G16 in my case

//Constants
float voltageCalib = 0.138;
float sensitivity = 0.033; // V/A (Sensor sensitivity), the original sensor has a sensit = 0.066 V/A, but because the ACS712 ouput is 5V max, a Voltage Divider is used in order to protect the max 3.3v input of the ESP32, so the sensitivity also is cut to half.
//float currentSensorOffset = 1.25;
float currentSensorOffset = 1.07; 
double R1_V = 68000; // R1 from voltage divisor, 68kOhm. If you can measure the REAL values of the resistance, replace it here for more accuarcy.
double R2_V = 6800; // R2 from the voltage divisor, 6.8kOhm.

//Variables:
bool   SD_present = false; //Controls if the SD card is present or not
String lastDay = "2023-01-19"; //Initial value, meant to compare the date, in order to create date-based instructions, i.e: Creating a new file on the SD when the Day changes, in order to maintain the data sorted.
String dayStamp = "2023-01-19"; //Probably could remove some of these variables, to further improvement.
String hourStamp = "00";
String minSecStamp = "00:00";
String newFileName = "2023-01-19";
String formattedDate = "";




/*********  SETUP  **********/

void setup(void)
{  
  Serial.begin(115200);
  
  initWiFi();
  initSDCard();
  
  //Set your preferred server name, if you use "mcserver" the address would be http://mcserver.local/
  if (!MDNS.begin(servername)) 
  {          
    Serial.println(F("Error setting up MDNS responder!")); 
    ESP.restart(); 
  }

  /*********  Server Commands  **********/
  server.on("/",         SD_dir);
  server.on("/upload",   File_Upload);
  server.on("/fupload",  HTTP_POST,[](){ server.send(200);}, handleFileUpload);

  server.begin();
  
  Serial.println("HTTP server started");
}

/*********  LOOP  **********/

void loop(void){
////*Server handle*/
  
  server.handleClient(); //Listen for client connections

////////////NTP Functions////////////

while(!timeClient.update()) {
    timeClient.forceUpdate();
    }

  //Date extract from the FormattedDate method, raw String: 2023-01-24T21:40:00Z
  
  formattedDate = timeClient.getFormattedDate();

  int splitT = formattedDate.indexOf("T"); //This line finds the index position when the Date ends.
  dayStamp = formattedDate.substring(0,splitT); //The method "substring" extracts a portion of the string value at a given index range.

  //Hour extract
  hourStamp = formattedDate.substring(splitT+1,splitT+3);
  
  //Minute-Sec extract, used to avoid the delay() function.
  minSecStamp = formattedDate.substring(splitT+4,formattedDate.length()-1);
  
  
  if (!lastDay.equals(dayStamp)){ //If the day changes, creates a new File in the SD Card.
    
    Serial.println("New File"); //Here goes the instruction, in this case only prints on the Serial for debbuging reasons.
    lastDay = dayStamp; //Update the variable used to compare.
    newFileName = "/" + lastDay + ".txt";
    writeFile(SD,newFileName,"Mediciones del día: "+lastDay+ "\n");
    }
    else{
      Serial.println("Same File");
      }
/*Debbuging-Uncomment when everything is ready*/
  
  float voltage = abs(return_voltage_value(voltageSensorPin));
  float current = abs(return_current_value(currentSensorPin));
  Serial.println("Medición: " + dayStamp + " " + hourStamp + " " + minSecStamp);
  Serial.print("Voltage: ");
  Serial.println(voltage);
  Serial.print("Intensidad: ");
  Serial.println(current);


//Sensors info update and write to the SD Card.
  
  if(minSecStamp == "00:00"||minSecStamp == "20:00"||minSecStamp == "40:00"){ //20 minute interval sample, A little "hard-coded" but I didn´t want to rely on millis() to trigger the sensors.
    //Here goes the data update from the sensors.
    Serial.println("SAMPLE");
    appendFile(SD,newFileName,dayStamp + " " + hourStamp + ":" + minSecStamp + " " + voltage + " " + current + "\n");
    }
  delay(1000);
} 
/*END LOOP*/

/*********  FUNCTIONS  **********/


/********* Sensors Functions **********/

double return_voltage_value(int pin_no)
{
  double tmp = 0;
  double ADCVoltage = 0;
  double inputVoltage = 0;
  double avg = 0;
  for (int i = 0; i < 150; i++)
  {
    tmp = tmp + analogRead(pin_no);
  }
  avg = tmp / 150;
  ADCVoltage = ((avg * 3.3) / (4095)) + voltageCalib; //Serial to voltage conversion. Voltage sensed in the ADC IOpin.
  inputVoltage = ADCVoltage / (R2_V / (R1_V + R2_V)); // formula for calculating voltage in i.e. GND
  return inputVoltage;
}
double return_current_value(int pin_no)
{
  double tmp = 0;
  double avg = 0;
  double ADCVoltage = 0;
  double amps = 0;
  for (int z = 0; z < 150; z++)
  {
    tmp = tmp + analogRead(pin_no);
  }
  avg = tmp / 150;
  ADCVoltage = ((avg / 4095.0) * 3.3); // Gets you mV
  amps = ((ADCVoltage - currentSensorOffset) / sensitivity);
  return amps;
}




















/********* SD Card functions **********/
//SD-Card Inicialization
void initSDCard(){
  if(!SD.begin()){
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if(cardType == CARD_MMC){
    Serial.println("MMC");
    SD_present = true;
  } else if(cardType == CARD_SD){
    Serial.println("SDSC");
    SD_present = true;
  } else if(cardType == CARD_SDHC){
    Serial.println("SDHC");
    SD_present = true;
  } else {
    Serial.println("UNKNOWN");
    SD_present = true;
  }
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
}

//Write File in the SD, the arguments are: path SD, path of the file i.e /text.txt, content to write.
void writeFile(fs::FS &fs, String path, String message){
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

//Append content into an existent file
void appendFile(fs::FS &fs, String path, String message){
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file){
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)){
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

//Wi-Fi Inicialization
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

//Initial page of the server web, list directory and give you the chance of deleting and uploading
void SD_dir()
{
  if (SD_present) 
  {
    //Action acording to post, dowload or delete, by MC 2022
    if (server.args() > 0 ) //Arguments were received, ignored if there are not arguments
    { 
      Serial.println(server.arg(0));
  
      String Order = server.arg(0);
      Serial.println(Order);
      
      if (Order.indexOf("download_")>=0)
      {
        Order.remove(0,9);
        SD_file_download(Order);
        Serial.println(Order);
      }
  
      if ((server.arg(0)).indexOf("delete_")>=0)
      {
        Order.remove(0,7);
        SD_file_delete(Order);
        Serial.println(Order);
      }
    }

    File root = SD.open("/");
    if (root) {
      root.rewindDirectory();
      SendHTML_Header();    
      webpage += F("<table align='center'>");
      webpage += F("<tr><th>Name/Type</th><th style='width:20%'>Type File/Dir</th><th>File Size</th></tr>");
      printDirectory("/",0);
      webpage += F("</table>");
      SendHTML_Content();
      root.close();
    }
    else 
    {
      SendHTML_Header();
      webpage += F("<h3>No Files Found</h3>");
    }
    append_page_footer();
    SendHTML_Content();
    SendHTML_Stop();   //Stop is needed because no content length was sent
  } else ReportSDNotPresent();
}

//Upload a file to the SD
void File_Upload()
{
  append_page_header();
  webpage += F("<h3>Select File to Upload</h3>"); 
  webpage += F("<FORM action='/fupload' method='post' enctype='multipart/form-data'>");
  webpage += F("<input class='buttons' style='width:25%' type='file' name='fupload' id = 'fupload' value=''>");
  webpage += F("<button class='buttons' style='width:10%' type='submit'>Upload File</button><br><br>");
  webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer();
  server.send(200, "text/html",webpage);
}

//Prints the directory, it is called in void SD_dir() 
void printDirectory(const char * dirname, uint8_t levels)
{
  
  File root = SD.open(dirname);

  if(!root){
    return;
  }
  if(!root.isDirectory()){
    return;
  }
  File file = root.openNextFile();

  int i = 0;
  while(file){
    if (webpage.length() > 1000) {
      SendHTML_Content();
    }
    if(file.isDirectory()){
      webpage += "<tr><td>"+String(file.isDirectory()?"Dir":"File")+"</td><td>"+String(file.name())+"</td><td></td></tr>";
      printDirectory(file.name(), levels-1);
    }
    else
    {
      webpage += "<tr><td>"+String(file.name())+"</td>";
      webpage += "<td>"+String(file.isDirectory()?"Dir":"File")+"</td>";
      int bytes = file.size();
      String fsize = "";
      if (bytes < 1024)                     fsize = String(bytes)+" B";
      else if(bytes < (1024 * 1024))        fsize = String(bytes/1024.0,3)+" KB";
      else if(bytes < (1024 * 1024 * 1024)) fsize = String(bytes/1024.0/1024.0,3)+" MB";
      else                                  fsize = String(bytes/1024.0/1024.0/1024.0,3)+" GB";
      webpage += "<td>"+fsize+"</td>";
      webpage += "<td>";
      webpage += F("<FORM action='/' method='post'>"); 
      webpage += F("<button type='submit' name='download'"); 
      webpage += F("' value='"); webpage +="download_"+String(file.name()); webpage +=F("'>Download</button>");
      webpage += "</td>";
      webpage += "<td>";
      webpage += F("<FORM action='/' method='post'>"); 
      webpage += F("<button type='submit' name='delete'"); 
      webpage += F("' value='"); webpage +="delete_"+String(file.name()); webpage +=F("'>Delete</button>");
      webpage += "</td>";
      webpage += "</tr>";

    }
    file = root.openNextFile();
    i++;
  }
  file.close();

}

//Download a file from the SD, it is called in void SD_dir()
void SD_file_download(String filename)
{
  if (SD_present) 
  { 
    File download = SD.open("/"+filename);
    if (download) 
    {
      server.sendHeader("Content-Type", "text/text");
      server.sendHeader("Content-Disposition", "attachment; filename="+filename);
      server.sendHeader("Connection", "close");
      server.streamFile(download, "application/octet-stream");
      download.close();
    } else ReportFileNotPresent("download"); 
  } else ReportSDNotPresent();
}

//Handles the file upload a file to the SD
File UploadFile;
//Upload a new file to the Filing system
void handleFileUpload()
{ 
  HTTPUpload& uploadfile = server.upload(); //See https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WebServer/srcv
                                            //For further information on 'status' structure, there are other reasons such as a failed transfer that could be used
  if(uploadfile.status == UPLOAD_FILE_START)
  {
    String filename = uploadfile.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    Serial.print("Upload File Name: "); Serial.println(filename);
    SD.remove(filename);                         //Remove a previous version, otherwise data is appended the file again
    UploadFile = SD.open(filename, FILE_WRITE);  //Open the file for writing in SD (create it, if doesn't exist)
    filename = String();
  }
  else if (uploadfile.status == UPLOAD_FILE_WRITE)
  {
    if(UploadFile) UploadFile.write(uploadfile.buf, uploadfile.currentSize); // Write the received bytes to the file
  } 
  else if (uploadfile.status == UPLOAD_FILE_END)
  {
    if(UploadFile)          //If the file was successfully created
    {                                    
      UploadFile.close();   //Close the file again
      Serial.print("Upload Size: "); Serial.println(uploadfile.totalSize);
      webpage = "";
      append_page_header();
      webpage += F("<h3>File was successfully uploaded</h3>"); 
      webpage += F("<h2>Uploaded File Name: "); webpage += uploadfile.filename+"</h2>";
      webpage += F("<h2>File Size: "); webpage += file_size(uploadfile.totalSize) + "</h2><br><br>"; 
      webpage += F("<a href='/'>[Back]</a><br><br>");
      append_page_footer();
      server.send(200,"text/html",webpage);
    } 
    else
    {
      ReportCouldNotCreateFile("upload");
    }
  }
}

//Delete a file from the SD, it is called in void SD_dir()
void SD_file_delete(String filename) 
{ 
  if (SD_present) { 
    SendHTML_Header();
    File dataFile = SD.open("/"+filename, FILE_READ); //Now read data from SD Card 
    if (dataFile)
    {
      if (SD.remove("/"+filename)) {
        Serial.println(F("File deleted successfully"));
        webpage += "<h3>File '"+filename+"' has been erased</h3>"; 
        webpage += F("<a href='/'>[Back]</a><br><br>");
      }
      else
      { 
        webpage += F("<h3>File was not deleted - error</h3>");
        webpage += F("<a href='/'>[Back]</a><br><br>");
      }
    } else ReportFileNotPresent("delete");
    append_page_footer(); 
    SendHTML_Content();
    SendHTML_Stop();
  } else ReportSDNotPresent();
} 

//SendHTML_Header
void SendHTML_Header()
{
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate"); 
  server.sendHeader("Pragma", "no-cache"); 
  server.sendHeader("Expires", "-1"); 
  server.setContentLength(CONTENT_LENGTH_UNKNOWN); 
  server.send(200, "text/html", ""); //Empty content inhibits Content-length header so we have to close the socket ourselves. 
  append_page_header();
  server.sendContent(webpage);
  webpage = "";
}

//SendHTML_Content
void SendHTML_Content()
{
  server.sendContent(webpage);
  webpage = "";
}

//SendHTML_Stop
void SendHTML_Stop()
{
  server.sendContent("");
  server.client().stop(); //Stop is needed because no content length was sent
}

//ReportSDNotPresent
void ReportSDNotPresent()
{
  SendHTML_Header();
  webpage += F("<h3>No SD Card present</h3>"); 
  webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}

//ReportFileNotPresent
void ReportFileNotPresent(String target)
{
  SendHTML_Header();
  webpage += F("<h3>File does not exist</h3>"); 
  webpage += F("<a href='/"); webpage += target + "'>[Back]</a><br><br>";
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}

//ReportCouldNotCreateFile
void ReportCouldNotCreateFile(String target)
{
  SendHTML_Header();
  webpage += F("<h3>Could Not Create Uploaded File (write-protected?)</h3>"); 
  webpage += F("<a href='/"); webpage += target + "'>[Back]</a><br><br>";
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}

//File size conversion
String file_size(int bytes)
{
  String fsize = "";
  if (bytes < 1024)                 fsize = String(bytes)+" B";
  else if(bytes < (1024*1024))      fsize = String(bytes/1024.0,3)+" KB";
  else if(bytes < (1024*1024*1024)) fsize = String(bytes/1024.0/1024.0,3)+" MB";
  else                              fsize = String(bytes/1024.0/1024.0/1024.0,3)+" GB";
  return fsize;
}
