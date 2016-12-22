#line 1 "D:/ti_workspace/log_temp/log_temp.ino"
#include <SLFS.h>
#define CONF_FILE "/storage/conf_1.txt"

#include "Energia.h"

boolean checkConf();
void createConf();
void setup();
void loop();

#line 4
boolean checkConf() {
  
  if (SerFlash.open(CONF_FILE, FS_MODE_OPEN_READ) == false) {
    
    Serial.print("ERROR opening /storage/conf.txt!  Error code: ");
    Serial.println(SerFlash.lastErrorString());
    Serial.println("File seems to doesn't exists.");
    SerFlash.close();
    return false;
  }
  Serial.println("Conf exists.");
  return true;
}

void createConf() {
  Serial.println("Creating /storage/conf.txt and writing some text-");

  
  if (SerFlash.open(CONF_FILE,
      FS_MODE_OPEN_CREATE(512, _FS_FILE_OPEN_FLAG_COMMIT)) == false) {
    Serial.print("ERROR creating /storage/conf.txt!  Error code: ");
    Serial.println(SerFlash.lastErrorString());  

    Serial.println("Halting.");
    while(1) delay(1000);  
  }

  
  SerFlash.println("Hi there, this is my file!");
  
  
  
  
}

void setup() {
  
  Serial.begin(115200);
  Serial.println("Boot.");

  SerFlash.begin();

  int32_t retval = SerFlash.del(CONF_FILE);
  Serial.print("Deleting /storage/conf.txt return code: ");
  Serial.println(SerFlash.lastErrorString());

  if(!checkConf()) {
    createConf();
  }

  if (SerFlash.open(CONF_FILE, FS_MODE_OPEN_READ) == false) {
    
    Serial.print("ERROR opening /storage/conf.txt!  Error code: ");
    Serial.println(SerFlash.lastErrorString());
    Serial.println("Halting.");
    while (1) delay(1000);  
  }

  char buf[1024];  

  size_t read_size = SerFlash.readBytes(buf, 1023);
  if (read_size >= 0) {
    buf[read_size] = '\0';  
    Serial.print("Read ");
      Serial.print(read_size);
      Serial.println(" bytes from /storage/mine.txt - displaying contents:");

      Serial.print(buf);

      SerFlash.close();
  } else {
    Serial.print("There was an error reading from the /storage/mine.txt file! Error code: ");
    Serial.println(SerFlash.lastErrorString());
  }

  SerFlash.close();
}

void loop() {
  
  
}



