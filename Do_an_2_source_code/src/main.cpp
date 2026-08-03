#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <WebServer.h>

// EEPROM settings
#define EEPROM_SIZE 64
#define PASSWORD_ADDR 0
#define INIT_FLAG_ADDR 63 

// Khai bao phan cung
HardwareSerial FingerSerial(1); // UART1
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&FingerSerial);

WebServer server(80);
const String apiKey = "sixsixsix";
IPAddress local_ip(172, 20, 10, 2);
IPAddress gateway(172, 20, 10, 1); 
IPAddress subnet(255, 255, 255, 240); // Subnet Mask

const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte rowPins[ROWS] = {19, 18, 5, 17};
byte colPins[COLS] = {16, 4, 15};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
LiquidCrystal_I2C lcd(0x27, 16, 2);

const int relayPin = 26, buzzerPin = 32;
String correctPassword = "6789", inputPassword = "";
int failedAttempts = 0;

int mode = 0;

bool changePasswordMode = false;
bool awaitingCurrentPassword = false;
bool awaitingNewPassword = false;
bool systemLocked = false;
bool doorOpening = false;
bool Wificonnected = false;
int starCount = 0;

unsigned long lastServerHandle = 0;
unsigned long firstStarPressTime = 0;
unsigned long doorOpenStart = 0;
unsigned long wifiStart = millis();
bool waitingForSecondStar = false;


void savePasswordToEEPROM(const String password) {
  for (int i = 0; i < password.length(); i++) {
    EEPROM.write(PASSWORD_ADDR + i, password[i]);
  }
  EEPROM.write(PASSWORD_ADDR + password.length(), '\0');
  EEPROM.commit();
}

String readPasswordFromEEPROM() {
  String password = "";
  char ch;
  for (int i = 0; i < 32; i++) {
    ch = EEPROM.read(PASSWORD_ADDR + i);
    if (ch == '\0') break;
    password += ch;
  }
  return password;
}

void setup() {
  Serial.begin(9600);
  lcd.init();lcd.backlight();
  pinMode(relayPin, OUTPUT);pinMode(buzzerPin, OUTPUT);digitalWrite(relayPin, LOW);
  if (!WiFi.config(local_ip, gateway, subnet)) {
      Serial.println("STATIC IP FAILED");
    }
  WiFi.begin("666", "11111111");
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 5000) {
   delay(500);
   Serial.print(".");
 }
   if (WiFi.status() == WL_CONNECTED) {
    Wificonnected = true;
    Serial.println("WiFi connected");
    Serial.print("Assigned IP: ");
    Serial.println(WiFi.localIP());
    delay(1000);

    server.on("/lock", handleLock);
    server.on("/unlock", handleUnlock);
    server.on("/logs", handleGetLogs);
    server.on("/status", []() {
      String state = systemLocked ? "locked" : "unlocked";
      server.send(200, "text/plain", state);
    });

    server.begin();
    Serial.println("Web server started!");
  } 
  else {
    Wificonnected = false;
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Offline mode");
    Serial.println("WiFi connect failed. Offline mode.");
    delay(2000);
  }

  EEPROM.begin(EEPROM_SIZE);
    if (EEPROM.read(INIT_FLAG_ADDR) != 0xA5) {
    // EEPROM chưa khởi tạo, lưu mật khẩu mặc định và đánh dấu
    savePasswordToEEPROM(correctPassword);
    EEPROM.write(INIT_FLAG_ADDR, 0xA5);
    EEPROM.commit();
    Serial.println("EEPROM initialized with default password \"6789\"");
  }

  correctPassword = readPasswordFromEEPROM();
  Serial.print("Password loaded from EEPROM: '");
  Serial.print(correctPassword);
  Serial.println("'");
  FingerSerial.begin(57600, SERIAL_8N1, 14, 13);
  finger.begin(57600);
  lcd.clear();
  delay(2000);
  showMenu();
}

void showMenu() {
  lcd.clear();lcd.setCursor(0, 0);lcd.print("1: Nhap Mat khau");
  lcd.setCursor(0, 1);lcd.print("2: Van tay");
  mode = 0;
  inputPassword = "";
  changePasswordMode = false;
  awaitingCurrentPassword = false;
  awaitingNewPassword = false;
  starCount = 0;
  waitingForSecondStar = false;
}
void showFingerprintMenu() {
  lcd.clear();lcd.setCursor(0, 0);lcd.print("1: Xac thuc ");
  lcd.setCursor(0, 1);lcd.print("*:Dang ky|#:Xoa");
  mode = 2;  // đảm bảo đang ở chế độ vân tay
}

void beepSuccess() {
  tone(buzzerPin, 1800, 100);delay(120);noTone(buzzerPin);
}
void beepError() {
tone(buzzerPin, 800, 100);
delay(100);
tone(buzzerPin, 500, 150);
delay(100);
noTone(buzzerPin);
}

void openDoor() {
  beepSuccess();
  lcd.clear();lcd.setCursor(0, 0);lcd.print(" Dang mo cua...");Serial.print('Dang mo cua');
  digitalWrite(relayPin, HIGH);
  doorOpening = true;
  doorOpenStart = millis();  
  addLog("Door unlocked (local)");
}
void handleDoorState() {
  if(doorOpening){
    if( millis() - doorOpenStart >= 5000){
      digitalWrite(relayPin , LOW);
      doorOpening = false;
      failedAttempts = 0;
      showMenu();
    }
  }
}

void loop() {
    if (Wificonnected && millis() - lastServerHandle > 1000) {
    server.handleClient();
    lastServerHandle = millis();
  }
   if (systemLocked) {
    delay(100);
    return;
  }
  char key = keypad.getKey();
  if (key) {
    tone(buzzerPin, 1200, 40);delay(60);noTone(buzzerPin);
    if (mode == 0) {
      if (key == '1') {
        mode = 1;lcd.clear();lcd.setCursor(0, 0);lcd.print("Nhap mat khau:");
      } else if (key == '2') {
        mode = 2;lcd.clear();lcd.setCursor(0, 0);lcd.print("1: Xac thuc ");
        lcd.setCursor(0, 1);lcd.print("*:Dang ky|#:Xoa");
      }
    } else if (mode == 1) {
      handlePasswordInput(key);
    }
      else if (mode == 2) {    
      handleFingerprintMode(key); 
    }
  }

  // Thoát khỏi chờ nhấn * lần thứ hai sau 5 giây
 if (waitingForSecondStar && millis() - firstStarPressTime > 5000) {
    waitingForSecondStar = false;starCount = 0;
   if (mode == 1) {
    showMenu();
   } else if (mode == 2) {
    lcd.clear();lcd.setCursor(0, 0);lcd.print("1: Xac thuc ");
    lcd.setCursor(0, 1);lcd.print("*:Dang ky|#:Xoa");
   }
  } 
 handleDoorState();
}

void handlePasswordInput(char key) {
  if (changePasswordMode) {
    handleChangePassword(key);
  } else {
    if (key == '*') {
      starCount++;
      inputPassword = "";
      if (starCount == 1) {
        lcd.clear(); lcd.setCursor(0, 0);lcd.print("Nhan * lan nua");
        lcd.setCursor(0, 1);lcd.print("de doi mat khau");
        firstStarPressTime = millis();waitingForSecondStar = true;
      } else if (starCount == 2 && waitingForSecondStar) {
        waitingForSecondStar = false;
        changePasswordMode = true;
        awaitingCurrentPassword = true;
        lcd.clear();lcd.setCursor(0, 0);lcd.print("Doi mat khau");
        delay(1000);lcd.clear();lcd.setCursor(0, 0);lcd.print("Nhap MK hien tai:");
        starCount = 0;
      }
    } else {
      starCount = 0;
      if (key == '#') {
        if (inputPassword == correctPassword) {
          lcd.clear();lcd.setCursor(0, 0);lcd.print("Xac thuc MK");
          lcd.setCursor(0, 1);lcd.print("thanh cong !");delay(1500);openDoor();
        } else {
          failedAttempts++;
          beepError();lcd.clear();lcd.setCursor(0, 0);lcd.print("Sai mat khau!");addLog("Fail Door unlocked (Local )");
          delay(2000);
          int lockTime = 0;
          if (failedAttempts == 2) {
          lockTime = 5000;
          } else if (failedAttempts >= 3) {
          lockTime = 10000;
          } 
           if (lockTime > 0) {
           lcd.clear();
           lcd.setCursor(0, 0);
           lcd.print("Thu lai sau ");
           lcd.setCursor(0, 1);
           lcd.print(String(lockTime / 1000) + " giay");
           delay(lockTime);
           }
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Nhap mat khau:");
        }
        inputPassword = "";
      } else {
        if (inputPassword.length() < 6) {
          inputPassword += key;
          lcd.setCursor(0, 1);lcd.print("                ");
          lcd.setCursor(0, 1);lcd.print(inputPassword);
        }
      }
    }
  }
}

void handleChangePassword(char key) {
  if (awaitingCurrentPassword) {
    if (key == '#') {
      if (inputPassword == correctPassword) {
        awaitingCurrentPassword = false;awaitingNewPassword = true;
        lcd.clear();lcd.setCursor(0, 0);lcd.print("Nhap MK moi:");
        inputPassword = "";
      } else {
        beepError();lcd.clear();lcd.setCursor(0, 0);lcd.print("Sai MK hien tai");
        delay(2000);
        lcd.clear();lcd.setCursor(0, 0);lcd.print("Nhap MK hien tai:");
        inputPassword = "";
      }
    } else if (key == '*') {
      changePasswordMode = false;awaitingCurrentPassword = false;
      inputPassword = "";
      lcd.clear();lcd.setCursor(0, 0);lcd.print("Huy doi MK");
      delay(2000);showMenu();
    } else {
      if (inputPassword.length() < 6) {
        inputPassword += key;
        lcd.setCursor(0, 1);lcd.print("                ");
        lcd.setCursor(0, 1);lcd.print(inputPassword);
      }
    }
  } else if (awaitingNewPassword) {
    if (key == '#') {
      if (inputPassword.length() >= 4) {
        correctPassword = inputPassword;savePasswordToEEPROM(correctPassword);
        beepSuccess();lcd.clear();lcd.setCursor(0, 0);lcd.print("Da doi MK ");addLog("Password has been changed !");
        delay(2000);showMenu();
      } else {
        beepError();lcd.clear();lcd.setCursor(0, 0);lcd.print("MK moi >3 so");
        delay(2000);
        inputPassword = "";
        lcd.clear();lcd.setCursor(0, 0);lcd.print("Nhap MK moi:");
      }
    } else if (key == '*') {
      changePasswordMode = false;awaitingNewPassword = false;
      inputPassword = "";
      lcd.clear();lcd.setCursor(0, 0);lcd.print("Huy doi MK");
      delay(2000);showMenu();
    } else {
      if (inputPassword.length() < 6) {
        inputPassword += key;
        lcd.setCursor(0, 1);lcd.print("                ");
        lcd.setCursor(0, 1);lcd.print(inputPassword);
      }
    }
  }
}

void handleLock() {
    handleCORS();
  if (server.method() == HTTP_OPTIONS) {
    return;
  }

  if (server.hasArg("key") && server.arg("key") == apiKey) {
    server.send(200, "text/plain", "Da khoa he thong");
    systemLocked = true;
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("He thong bi khoa");
    lcd.setCursor(0, 1); lcd.print("Lien he admin");
    addLog("Door locked (API)");
  } else {
    server.send(403, "text/plain", "Access denied");
  }
}
void handleUnlock() {
  handleCORS();
  
  if (server.method() == HTTP_OPTIONS) {
    return;
  }
  if(server.hasArg("key") && server.arg("key") == apiKey){
  server.send(200, "text/plain", "Da mo cua");
  systemLocked = false;
  lcd.clear();lcd.setCursor(0, 0);lcd.print("Dang mo cua...");
  digitalWrite(relayPin , HIGH);
  doorOpening = true;
  doorOpenStart = millis();  
  addLog("Door unlocked (API)");
  } else {
   server.send(403, "text/plain", "Access denied");
  }
}

void checkFingerprint() {
  lcd.clear(); lcd.setCursor(0, 0); lcd.print("Dat ngon tay...");
  unsigned long startTime = millis();
  while (millis() - startTime < 5000) {
    int result = finger.getImage();
    if (result == FINGERPRINT_OK) {
      result = finger.image2Tz();
      if (result == FINGERPRINT_OK) {
        result = finger.fingerSearch();
        if (result == FINGERPRINT_OK) {
          lcd.clear(); lcd.setCursor(0, 0); lcd.print("Xac thuc van tay");
          lcd.setCursor(0, 1); lcd.print("thanh cong !");
          delay(1000);
          beepSuccess();
          lcd.clear();lcd.setCursor(0, 0);lcd.print("Dang mo cua...");
          digitalWrite(relayPin, HIGH);delay(5000);digitalWrite(relayPin,LOW);
          addLog("Successful check fingerprint ");
          showMenu();
          return;
        } else {
          beepError();
          lcd.clear();
          lcd.setCursor(0, 0);lcd.print(" Van tay khong ");
          lcd.setCursor(0, 1);lcd.print("hop le !");
          addLog("Fail checkFingerprint");
          delay(1000);
          lcd.clear(); lcd.setCursor(0, 0); lcd.print("Dat ngon tay ...");
          startTime = millis();
        }
      }
    }
    delay(100);
  }
  beepError();lcd.clear();lcd.setCursor(0, 0);lcd.print("Het thoi gian");
  lcd.setCursor(0, 1);lcd.print("cho !");
  delay(2000);showMenu();
}
void handleFingerprintMode(char key) {
  mode  = 2 ;
   if (key == '1') {
    checkFingerprint();  
  }else if ( key == '*'){
      starCount++;
    if (starCount == 1) {
      lcd.clear();lcd.setCursor(0, 0);lcd.print("Nhan * lan nua");
      lcd.setCursor(0, 1);lcd.print("de DK van tay");
      firstStarPressTime = millis();
      waitingForSecondStar = true;
    } else if (starCount == 2 && waitingForSecondStar) {
      waitingForSecondStar = false;
      starCount = 0;
      lcd.clear();lcd.setCursor(0, 0);lcd.print("Dang ky van tay");
      delay(2000);registerFingerprint(); 
    }
  } 
   else if (key == '#') {
    deleteFingerprint(key);
  }
}

void registerFingerprint(){
  int id;
  int p = -1;
  // --- Tìm ID trống ---
  int templates = finger.templateCount;
  Serial.print("Templates: "); Serial.println(templates);
  for (id = 1; id < 127; id++) {
    if (finger.loadModel(id) != FINGERPRINT_OK) break;
  }
  if (id >= 127) {
    lcd.clear();lcd.setCursor(0, 0);lcd.print("Bo nho day!");
    delay(2000);showFingerprintMenu();
    return;
  }
  lcd.clear();lcd.setCursor(0, 0);lcd.print("ID van tay: ");lcd.print(id);
  delay(2000);
  // ---- Lần chụp 1 ----
  lcd.clear();lcd.setCursor(0, 0);lcd.print("Dat ngon tay ...");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) continue;
    else if (p == FINGERPRINT_PACKETRECIEVEERR) {
      lcd.setCursor(0, 1);lcd.print("Loi giao tiep!");
      delay(1500);showFingerprintMenu();
      
    } else if (p == FINGERPRINT_IMAGEFAIL) {
      lcd.setCursor(0, 1);lcd.print("Loi anh!");
      delay(1500);showFingerprintMenu();
    }
  }
  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    lcd.clear();lcd.print("Loi chuyen doi!");
    delay(2000);showFingerprintMenu();
    return;
  }
   p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    // Ngón tay đã tồn tại
    lcd.clear();lcd.setCursor(0, 0);lcd.print("Van tay da ton ");
    lcd.setCursor(0, 1);lcd.print("tai !| ID: ");
    lcd.print(finger.fingerID);
    beepError();
    delay(2500);showFingerprintMenu();
    return;
  }
  lcd.clear();lcd.setCursor(0, 0);lcd.print("bo ngon tay ra...");
  delay(2000);
  while (finger.getImage() != FINGERPRINT_NOFINGER);
  // ---- Lần chụp 2 ----
  p = -1;
  lcd.clear();lcd.setCursor(0, 0);lcd.print("Dat lai ngon ");
  lcd.setCursor(0, 1);lcd.print("tay...");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) continue;
  }
  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    lcd.clear();
    lcd.print("Loi chuyen doi 2!");
    delay(2000);
    showFingerprintMenu();
    return;
  }
  // ---- Tạo model ----
  lcd.clear();lcd.setCursor(0, 0);lcd.print("DANG tao mau ");
  lcd.setCursor(0, 1);lcd.print("van tay...");
  delay(2000);
  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    lcd.clear();lcd.print("Tao mau that bai!");
    delay(2000);showFingerprintMenu();
    return;
  }
  // ---- Lưu model ----
  lcd.clear();lcd.setCursor(0, 0);lcd.print("Luu ID: ");lcd.print(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    beepSuccess();lcd.setCursor(0, 1);lcd.print("Thanh cong!"); addLog("successful registerFingerprint");
  } else {
    beepError();lcd.setCursor(0, 1);lcd.print("That bai!"); addLog("Fail registerFingerprint");
  }
  delay(2000);showFingerprintMenu();
}

void deleteFingerprint(char key){
int p = -1;
  lcd.clear();lcd.setCursor(0, 0);lcd.print("Dat ngon tay...");
  // Bước 1: Chờ người dùng đặt ngón tay
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) continue;
    if (p == FINGERPRINT_PACKETRECIEVEERR) {
      lcd.setCursor(0, 1);lcd.print("Loi giao tiep!");
    } else if (p == FINGERPRINT_IMAGEFAIL) {
      lcd.setCursor(0, 1);lcd.print("Loi anh!");
    }
  }
  // Bước 2: Chuyển hình ảnh thành đặc trưng
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) {
    lcd.clear();lcd.print("Loi chuyen doi!");
    delay(2000);showFingerprintMenu();
    return;
  }
  // Bước 3: Tìm xem có trùng với ID nào đã lưu không
  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    int id = finger.fingerID;
    
    // Bước 4: Xóa ID đó
    p = finger.deleteModel(id);
    
    if (p == FINGERPRINT_OK) {
      beepSuccess();
      lcd.clear();lcd.setCursor(0, 0);lcd.print("Xoa thanh cong!");
      lcd.setCursor(0, 1);lcd.print("ID: ");lcd.print(id); addLog("successful delete Fingerprint");
    } else {
      beepError();lcd.clear();lcd.setCursor(0, 0);lcd.print("Xoa that bai!"); addLog("Fail delete Fingerprint ");
    }
  } else {
    // Không tìm thấy vân tay
    beepError();lcd.clear();lcd.setCursor(0, 0);lcd.print("Van tay chua ");lcd.setCursor(0, 1);lcd.print("dang ky !");
  }
delay(2500);showFingerprintMenu();
}

struct LogEntry {
  String action;
  unsigned long timestamp;
};
LogEntry logs[10];
int logIndex = 0;

String formatElapsedTime(unsigned long seconds) {
  unsigned long days = seconds / 86400;        // 24 * 3600
  seconds %= 86400;
  unsigned long hours = seconds / 3600;
  seconds %= 3600;
  unsigned long minutes = seconds / 60;
  seconds %= 60;

  String result = "";

  if (days > 0) {
    result += String(days) + "d";
    if (hours > 0) result += " " + String(hours) + "h";
  } 
  else if (hours > 0) {
    result += String(hours) + "h";
    if (minutes > 0) result += " " + String(minutes) + "m";
  } 
  else if (minutes > 0) {
    result += String(minutes) + "m";
    if (seconds > 0) result += " " + String(seconds) + "s";
  } 
  else {
    result += String(seconds) + "s";
  }

  result += " ago";
  return result;
}


void addLog(String action) {
  logs[logIndex].action = action;
  logs[logIndex].timestamp = millis();
  logIndex = (logIndex + 1) % 10; 
  Serial.println("LOG: " + action);
}


void handleGetLogs() {
  handleCORS();
  if (server.method() == HTTP_OPTIONS) return;

  String response = "";
  unsigned long now = millis();

 
  for (int i = 0; i < 10; i++) {
    int index = (logIndex - 1 - i + 10) % 10;  
    if (logs[index].action != "") {
      unsigned long elapsed = (now - logs[index].timestamp) / 1000;
      response += String(i + 1) + ". " + logs[index].action + " - " + formatElapsedTime(elapsed) + "\n";
    }
  }

  server.send(200, "text/plain", response);
}

void handleCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  if (server.method() == HTTP_OPTIONS) {
    server.send(200);
    return;
  }
}