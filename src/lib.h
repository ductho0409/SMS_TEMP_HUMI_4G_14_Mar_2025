
// #define TINY_GSM_MODEM_SIM800
#define TINY_GSM_MODEM_SIM7600

#define TINYGSM
#define AT_BAUDRATE 115200

#include <esp_task_wdt.h>
#include <Arduino.h>
#include <Wire.h>
#include "SSD1306Wire.h"
#include <DallasTemperature.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include "time.h"
#include "esp_sntp.h"
#include "ConfigManager.h"
#include <HTTPUpdate.h>
#include <esp_partition.h>
#include <FS.h>
#include <time.h>
#include <ESP32Time.h>
#include <math.h>
#include <ArtronShop_SHT3x.h>
#include "esp_wifi.h"
#include <NTPClient.h>
#include <HTTPClient.h>

ArtronShop_SHT3x sht3x(0x44, &Wire);
SSD1306Wire display(0x3c, SDA, SCL);
#define BASE_URL "http://theodoi.setcom.com.vn/"
#define PROJECT_NAME "SMS_TEMP_HUMI_4G_14_Mar_2025"
const char *sUpdateFirmware = BASE_URL PROJECT_NAME "/firmware.bin";

const char *url_caidat_html = BASE_URL PROJECT_NAME "/caidat.html";
const char *fileName_caidat_html = "/caidat.html";

const char *url_index_html = BASE_URL PROJECT_NAME "/index.html";
const char *fileName_index_html = "/index.html";

const char *url_main_js = BASE_URL PROJECT_NAME "/main.js";
const char *fileName_main_js = "/main.js";

const char *url_styles_css = BASE_URL PROJECT_NAME "/styles.css";
const char *fileName_styles_css = "/styles.css";

const char *settingsHTML = (char *)"/caidat.html";
const char *stylesCSS = (char *)"/styles.css";
const char *mainJS = (char *)"/main.js";
// Đường dẫn tới file trong SPIFFS
String OfflineLogfilePath = "/offlineLog.txt";
String ErrorFilePath = "/offlineError.txt";
bool testEsp32 = false;
String logIdx = "90";
String smsIdx = "185";
String errorIdx = "95";

float tempToSendSMS;
#define CallSMSnumberLength 3
#define ONE_WIRE_BUS 18
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

#define SerialMon Serial
#define SerialAT Serial2
#define TINY_GSM_DEBUG SerialMon
String LogMQTTContent = "";
String LogMQTTContent1 = "";

int version = 0;
// For RTC clock
const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 7 * 3600;
const int daylightOffset_sec = 0;

const int ngayNhanTin_hangTuan = 1;
const int RSSI_MAX = -15;
const int RSSI_MIN = -100;

int SL_CAM_BIEN = 3;
int SL_SDT_GOI = 10;
int SL_SDT_NhanTin = 10;
int SL_SDT_tinNhanHangNgay = 5;
const int doDaiToiDa_SDT = 15;
int *choPhepSensorCanhBao;
int *IdxArray;
float *hieuChinhCamBien;
int *thoiGianChoCanhBao;
int *thoiGianChoLapLai_CanhBao;
int *soTinNhanToiDa;
unsigned long *T;
unsigned long *saving_T;
float *giaTriToiThieu;
float *giaTriToiDa;
bool *trangThaiCanhBao;
bool *trangThaiCanhBaoLanTruoc;
unsigned long *currentMillis;
float *giaTriCamBien;
int *soTinNhanDaGuiHienTai;
char **soGoiDienArray;
char **soNhanTinArray;
char **soNhanTinHangNgayArray;
char **donVi_hienThi;

// #define DUMP_AT_COMMANDS
//     #define DEBUG 1

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

// Khai báo TinyGSMClient cho 4G
TinyGsmClient gsmClient(modem);
PubSubClient mqtt4G(gsmClient);

// Khai báo WiFiClient cho WiFi
WiFiClient wifiClient;
PubSubClient mqttWifi(wifiClient);

// Con trỏ MQTT dùng chung cho cả WiFi và 4G
PubSubClient *mqtt;

// Biến để lựa chọn dùng MQTT nào 4G hay wifi: False là 4G; True là wifi
bool wifi_using = false;

byte willQoS = 1;
const char *willMessage = "0";
boolean willRetain = true;
bool sendBootReasonStatus = false;

bool timeIsSet = false;
float evtemp, evhumi;

int powerDetectPin;
int relayOutputPin;
// #define enable_button_old 26

// #define led 2
// #define enable_button 19
int enable_button = 0;
int led = 0;
int switch_record = 0;

ESP32Time espTime;

#define boot_pin 0
#define PEN 5
#define DoorSensor 13

bool setSim800lTimeFlag = false;
int lastSwitchState;
struct tm timeinfo;
struct Config
{
  bool resetStatusAtBoot;
  bool runMode;
  char device_mode[20];
  char device_name[20];
  char device_idx[30];

  char wifi_name[100];
  char wifi_pass[20];

  char apn[20];
  char user[5];
  char pwd[5];

  char broker[30];
  char mqttUser[10];
  char mqttPass[10];
  char SERVER_TOPIC_RECEIVE_DATA[20];
  unsigned int brokerPort;
  unsigned int MQTT_update_interval;

  char giaTriToiDa_config[20];
  char giaTriToiThieu_config[20];

  char thoiGianChoCanhBao_config[20];
  char thoiGianChoLapLai_CanhBao_config[20];
  char soTinNhanToiDa_config[20];

  char hieuChinhCamBien_config[20];

  bool doorSensorEnable;
  unsigned int powerWaitingToSend;
  unsigned int powerWaitingToRepeat;
  unsigned int maximumOfSendingpower;

  bool sendConfigMqtt;
  bool firstTime;
  bool sendReportStatus;
  unsigned int reportClock;
  unsigned int lastDay;
  // Canh bao mat dien
  bool powerLostWarningEnable;
  unsigned int waitingTimetoSendPowerLost;
  bool powerRecoverEnable;

  // Canh bao loi cam bien
  bool sensorErrorWarningEnable;
  unsigned int waitingTimetoSendSensorError;

  // Cho phép nhắn tin hàng ngay
  bool dailySMSEnable;
  // Cho phép relay đầu ra
  bool relayOutputEnable;

  // Wifi thứ 2
  char second_wifi_name[100];
  char second_wifi_pass[20];

  bool mainWifi;
  char soGoiDien[150];
  char soNhanTin[150];
  char soNhanTinHangNgay[75];
  char choPhepSensorCanhBao_config[20];
  bool trangthaiNhantinTheoTuan;
  bool volteStatus;
  char donVi_hienThi_config[20];
} config;

ConfigManager configManager;

void addToLog(String L)
{

  if (!getLocalTime(&timeinfo))
    return;
  // Chuyển đổi thành chuỗi ngày giờ
  char timeString[50]; // Để lưu trữ chuỗi ngày giờ
  strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
  if (LogMQTTContent1.length() > 8000)
  {
    LogMQTTContent1 = LogMQTTContent1 + "}}}}}}";
    return;
  }
  else
    LogMQTTContent1 = LogMQTTContent1 + "[" + String(timeString) + "]: " + L + "\r\n";
  // SerialMon.printf("LOG: %s\n", LogMQTTContent1.c_str());
}
void delayWithWatchdog(long timeToDelay)
{
  long long now_forDelay = millis();
  while (true)
  {
    esp_task_wdt_reset();
    if (millis() - now_forDelay > timeToDelay)
      break;
  }
}
bool checkDeviceMode(String s)
{
  char buf[50] = "";
  s.toCharArray(buf, s.length());
  // SerialMon.printf("Device mode: %s\r\n", s);
  if (strstr(config.device_mode, buf) != NULL)
    return true;
  else
    return false;
}
// Biến toàn cục để theo dõi trang hiện tại
int currentPage = 0;
int soLuongToiDaTrenTrang = 3;
unsigned long lastPageUpdateTime = 0;
const long pageUpdateInterval = 3000; // Thời gian cập nhật trang là 3 giây

void oledTempDisplay(String h, float *values, String s, bool b4, bool timeDisplay)
{
  unsigned long currentMillis = millis();

  if (currentMillis - lastPageUpdateTime > pageUpdateInterval)
  {

    currentPage = (currentPage + 1) % ((SL_CAM_BIEN + soLuongToiDaTrenTrang - 1) / soLuongToiDaTrenTrang);
    lastPageUpdateTime = currentMillis;
  }

  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  // Hiển thị thời gian (nếu cần)
  if (timeDisplay)
  {
    if (getLocalTime(&timeinfo))
    {
      h = h + " (" + String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min) + ":" + String(timeinfo.tm_sec) + ")";
    }
    else
    {
      h = h + " Time...";
    }
  }

  display.drawStringMaxWidth(0, 0, 128, h);
  display.drawStringMaxWidth(0, 10, 128, "                      ------------------");

  // Tính toán vị trí bắt đầu và kết thúc của trang
  int startIndex = currentPage * soLuongToiDaTrenTrang;
  int endIndex = startIndex + soLuongToiDaTrenTrang;
  if (endIndex > SL_CAM_BIEN)
    endIndex = SL_CAM_BIEN;

  int xPosition = 0; // Vị trí bắt đầu trên trục x

  for (int i = startIndex; i < endIndex; ++i)
  {
    char buffer[100]; // Buffer để chứa chuỗi

    // Kiểm tra xem giá trị có nhỏ hơn -900 không
    if (values[i] < -900.0)
    {
      strcpy(buffer, "Err");
    }
    else
    {
      // Định dạng giá trị bình thường
      snprintf(buffer, sizeof(buffer), "%.1f", values[i]);
    }

    // Đặt font và hiển thị
    display.setFont(SL_CAM_BIEN == 1 ? ArialMT_Plain_24 : ArialMT_Plain_16);
    display.drawString(xPosition, 26, String(buffer));

    // Chọn font phù hợp với số lượng cảm biến
    if (SL_CAM_BIEN == 1)
    {
      // Hiển thị giá trị và đơn vị với cùng kích thước font
      display.setFont(ArialMT_Plain_24);
      display.drawString(xPosition, 26, String(buffer) + " " + String(donVi_hienThi[i]));
    }
    else
    {
      // Hiển thị giá trị với font lớn hơn và đơn vị với font nhỏ hơn
      display.setFont(ArialMT_Plain_16);
      display.drawString(xPosition, 26, String(buffer));
      display.setFont(ArialMT_Plain_10);
      display.drawString(xPosition, 42, String(donVi_hienThi[i]));
    }

    xPosition += (SL_CAM_BIEN == 1) ? 84 : 42; // Tăng vị trí x cho mỗi giá trị
  }
  // Hiển thị thông tin trang ở góc dưới bên trái
  String pageInfo = "Trang:" + String(currentPage + 1) + "/" + String((SL_CAM_BIEN + soLuongToiDaTrenTrang - 1) / soLuongToiDaTrenTrang);
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 54, pageInfo);

  // Hiển thị trạng thái và thông tin bổ sung
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.drawString(128, 54, b4 ? "RUNNING" : "STOP");
  display.drawString(128, 44, s);

  display.display();
}

void oledInforDisplay(String title, String content, String foot, long dl)
{
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawStringMaxWidth(0, 0, 128, title);
  display.drawStringMaxWidth(0, 10, 128, "-----------------------------------------");
  display.setFont(ArialMT_Plain_16);
  display.drawStringMaxWidth(0, 26, 128, content);
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.drawString(128, 54, foot);
  display.display();
  delayWithWatchdog(dl);
}
void oledSetup(int br = 255)
{
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setBrightness(br);
}

int demPhanTu(const char data[])
{
  int count = 0;

  // Tạo bản sao của chuỗi để không làm thay đổi chuỗi gốc
  char *dataCopy = strdup(data); // strdup sẽ phân bổ bộ nhớ mới và sao chép chuỗi

  // Sử dụng strtok để tách chuỗi
  const char delimiters[] = ";\n";
  char *token = strtok(dataCopy, delimiters);

  while (token != NULL)
  {
    count++;
    token = strtok(NULL, delimiters);
  }

  // Giải phóng bộ nhớ được phân bổ bởi strdup
  free(dataCopy);

  return count;
}

void tachChuoiCon(const char *input, char **&chuoiConArray)
{
  // Khởi tạo mảng các chuỗi
  chuoiConArray = new char *[SL_CAM_BIEN];
  for (int i = 0; i < SL_CAM_BIEN; ++i)
  {
    chuoiConArray[i] = strdup(""); // Khởi tạo với chuỗi rỗng
  }

  // Tạo một bản sao của chuỗi đầu vào
  char *inputCopy = strdup(input);

  // Tách chuỗi thành các chuỗi con và lưu vào mảng
  char *token = strtok(inputCopy, ";");
  int numCon = 0;
  while (token != NULL && numCon < SL_CAM_BIEN)
  {
    free(chuoiConArray[numCon]);           // Giải phóng chuỗi rỗng mặc định trước khi gán chuỗi mới
    chuoiConArray[numCon] = strdup(token); // Cấp phát và sao chép chuỗi con
    token = strtok(NULL, ";");
    numCon++;
  }

  // Giải phóng bộ nhớ cho bản sao của chuỗi đầu vào
  free(inputCopy);
}

void tachSoDienThoai(const char *input, char **&soGoiDienArray)
{
  // Khởi tạo mảng các chuỗi
  soGoiDienArray = new char *[SL_SDT_GOI];
  for (int i = 0; i < SL_SDT_GOI; ++i)
  {
    soGoiDienArray[i] = new char[doDaiToiDa_SDT];
    soGoiDienArray[i][0] = '\0'; // Khởi tạo phần tử với chuỗi trống
  }

  // Tạo một bản sao của chuỗi đầu vào
  char *inputCopy = strdup(input);

  // Loại bỏ các ký tự không phải số, chỉ giữ lại số và ký tự xuống dòng
  int j = 0;
  for (int i = 0; inputCopy[i] != '\0'; ++i)
  {
    if (isdigit(inputCopy[i]) || inputCopy[i] == '\n')
    {
      inputCopy[j++] = inputCopy[i];
    }
  }
  inputCopy[j] = '\0'; // Kết thúc chuỗi

  // Tách chuỗi thành các số điện thoại và lưu vào mảng
  char *token = strtok(inputCopy, "\n");
  int numPhones = 0;
  while (token != NULL && numPhones < SL_SDT_GOI)
  {
    strncpy(soGoiDienArray[numPhones], token, doDaiToiDa_SDT - 1);
    soGoiDienArray[numPhones][doDaiToiDa_SDT - 1] = '\0'; // Đảm bảo chuỗi kết thúc đúng cách
    token = strtok(NULL, "\n");
    numPhones++;
  }

  // Giải phóng bộ nhớ cho bản sao của chuỗi đầu vào
  free(inputCopy);
}
bool checkKeyPressed(uint32_t timeout_ms = 1000L)
{
  for (uint32_t start = millis(); millis() - start < timeout_ms;)
  {
    esp_task_wdt_reset();
    if (digitalRead(enable_button) == LOW || Serial.available() > 0)
    {
      delay(10); // Chống rung và cho phép dữ liệu nối tiếp đầy đủ
      if (digitalRead(enable_button) == LOW || Serial.available() > 0)
      {
        return true;
      }
    }
  }
  return false;
}

int dBmtoPercentage(int dBm)
{
  int quality;
  if (dBm <= RSSI_MIN)
  {
    quality = 0;
  }
  else if (dBm >= RSSI_MAX)
  {
    quality = 100;
  }
  else
  {
    quality = 2 * (dBm + 100);
  }

  return quality;
} // dBmtoPercentage
void SIMON()
{
  if (config.volteStatus)
    return;

  SerialMon.println("---->SIM ON SIM800");
  pinMode(PEN, OUTPUT);
  digitalWrite(PEN, HIGH);
}
// Làm sạch ký tự Json
String sanitizeString(const String &input)
{
  String danhSachKyTu = " 0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZàảãáạâầẩẫấậăằẳẵắặèẻẽéẹêềểễếệìỉĩíịòỏõóọôồổỗốộơờởỡớợùủũúụưừửữứựỳỷỹýỵđ!@#$%^&*()_+-=[]{}|;':\",.<>?/\\ÀẢÃÁẠÂẦẨẪẤẬĂẰẲẴẮẶÈẺẼÉẸÊỀỂỄẾỆÌỈĨÍỊÒỎÕÓỌÔỒỔỖỐỘƠỜỞỠỚỢÙỦŨÚỤƯỪỬỮỨỰỲỶỸÝỴĐ";
  String result = "";
  for (unsigned int i = 0; i < input.length(); i++)
  {
    char c = input.charAt(i);
    // Chấp nhận các ký tự Unicode của tiếng Việt
    if (danhSachKyTu.indexOf(c) != -1)
      result += c;
  }
  return result;
}
void SIMOFF()
{
  if (config.volteStatus)
    return;

  SerialMon.println("---->SIM OFF SIM800");
  pinMode(PEN, OUTPUT);
  digitalWrite(PEN, LOW);
}

String jsonToUpperCaseKeys(const char *jsonString)
{
  DynamicJsonDocument jsonDocument(4000);
  DynamicJsonDocument jsonDocumentReturn(4000);
  DeserializationError error = deserializeJson(jsonDocument, jsonString);

  // Kiểm tra lỗi khi parse chuỗi JSON
  if (error)
  {
    String ss = String(jsonString);
    ss.toUpperCase();
    return ss;
  }

  // Tạo một JSON mới với tất cả các key viết hoa
  JsonObject root = jsonDocument.as<JsonObject>();
  // Sao chép dữ liệu từ root vào newRoot

  int i = 0;
  for (JsonPair kv : root)
  {
    String oldKey = kv.key().c_str();
    oldKey.toUpperCase();
    jsonDocumentReturn[oldKey] = kv.value();
  }

  // Chuyển JSON mới thành chuỗi và trả về
  String modifiedJson;
  serializeJson(jsonDocumentReturn, modifiedJson);
  return modifiedJson;
}
void activeRelayOutput(bool b)
{

  // pinMode(BUILTIN_LED, OUTPUT);
  if (!config.relayOutputEnable)
    digitalWrite(relayOutputPin, LOW);

  else
  {
    if (b)
      digitalWrite(relayOutputPin, HIGH);
    else
      digitalWrite(relayOutputPin, LOW);
  }
  // // digitalWrite(BUILTIN_LED, digitalRead(relayOutputPin));
  // SerialMon.print("Relay is: " + String(digitalRead(relayOutputPin) ? "ON  " : "OFF  "));
  // // addToLog("Relay is: " + String(digitalRead(relayOutputPin) ? "ON" : "OFF"));
}
int hardwareVersion()
{
  int version;
  pinMode(13, INPUT_PULLUP);
  pinMode(12, INPUT_PULLUP);
  if (digitalRead(13) == LOW && digitalRead(12) == HIGH)
  {
    version = 1;
  }
  else if (digitalRead(13) == LOW && digitalRead(12) == LOW)
  {
    version = 3;
  }
  else
  {
    version = 2;
  }
  return version;
}

String Mqtt_scanWifiAndCreateJson()
{
  int networkCount = WiFi.scanNetworks();
  if (networkCount == 0)
  {
    Serial.println("No WiFi networks found.");
    return "{}"; // Trả về JSON trống nếu không có mạng WiFi nào được tìm thấy
  }

  // Tạo một đối tượng JSON
  StaticJsonDocument<512> jsonDoc;
  JsonObject wifiObj = jsonDoc.to<JsonObject>();

  for (int i = 0; i < networkCount; ++i)
  {
    String ssid = WiFi.SSID(i);
    // Bỏ qua các mạng WiFi không có tên (SSID trống)
    if (ssid.length() > 0)
    {
      int signalStrength = WiFi.RSSI(i);                                  // Lấy giá trị dBm
      int signalStrengthPercent = map(signalStrength, -100, -40, 0, 100); // Chuyển đổi sang phần trăm
      wifiObj[ssid] = signalStrengthPercent;                              // Lưu giá trị phần trăm với tên SSID
    }
  }

  // Chuyển đổi JSON thành chuỗi
  String wifiListJson;
  serializeJson(jsonDoc, wifiListJson);

  for (int i = 0; i < SL_CAM_BIEN; i++)
  {
    // Gửi chuỗi JSON tới topic duy nhất
    char tp[100];
    sprintf(tp, "domoticz/parameter/%d", IdxArray[i]);
    mqtt->publish(tp, wifiListJson.c_str());
  }

  return wifiListJson;
}
String timeString()
{
  if (getLocalTime(&timeinfo))
  {
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return buffer;
  }
  else
    return "No RTC time";
}

bool connectToWiFi(bool mainWifi)
{
  const char *ssid = mainWifi ? config.wifi_name : config.second_wifi_name;
  const char *password = mainWifi ? config.wifi_pass : config.second_wifi_pass;
  unsigned long wifiTimeout = 30000; // Thời gian timeout (30 giây)
  if (testEsp32)
    wifiTimeout = 10000;

  if (ssid == nullptr)
  {
    return false; // `ssid` hoặc `password` không hợp lệ
  }
  if (strlen(ssid) == 0)
  {
    return false; // `ssid` hoặc `password` không hợp lệ
  }
  delayWithWatchdog(500);
  // Kết nối vào WiFi
  Serial.print("Đang kết nối tới ");
  Serial.printf(mainWifi ? "WiFi chính...'%s'\t'%s'\n" : "WiFi phụ...%s\t%s\n", ssid, password);
  WiFi.begin(ssid, password);
  unsigned long startTime = millis();
  int dem = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    String sBuf = String(dem++) + "/" + String(wifiTimeout / 1000);
    oledInforDisplay(String(ssid),
                     mainWifi ? "WiFi 1..." : "WiFi 2..." + String(digitalRead(powerDetectPin) ? "" : "(+)"),
                     sBuf, 1);
    if (millis() - startTime >= wifiTimeout)
      return false; // Kết nối không thành công
    if (checkKeyPressed(1000))
      return false;
    SerialMon.println(sBuf);
  }

  Serial.println(mainWifi ? "Đã kết nối tới wifi chính..." : "Đã kết nối tới wifi phụ...");
  return true; // Kết nối thành công
}
String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++)
  {

    if (data.charAt(i) == separator || i == maxIndex)
    {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
void loadParaDefault()
{
  config.runMode = 1;
  strncpy(config.device_name, "SMS_NEW", 20);
  strncpy(config.device_idx, "1", 30);

  strcpy(config.device_mode, "SMS_WIFI");
  strncpy(config.broker, "theodoi.setcom.com.vn", 30);

  strncpy(config.second_wifi_name, "HOME", 100);
  strncpy(config.second_wifi_pass, "0107455786", 20);

  strncpy(config.SERVER_TOPIC_RECEIVE_DATA, "domoticz/in", 20);
  // strncpy(config.no1, "0965261265", 11);
  // config.e_c_no1 = true;
  // config.e_s_no1 = true;
  // strncpy(config.no2, "0888047200", 11);
  // config.e_c_no2 = true;
  // config.e_s_no2 = true;
  strncpy(config.mqttUser, "", 10);
  strncpy(config.mqttPass, "", 10);
  strncpy(config.giaTriToiDa_config, "30;30;100", 20);
  strncpy(config.giaTriToiThieu_config, "0;0;0", 20);
  strncpy(config.hieuChinhCamBien_config, "0;0;0", 20);

  strncpy(config.thoiGianChoCanhBao_config, "300;300;300", 20);
  strncpy(config.thoiGianChoLapLai_CanhBao_config, "300;300;300", 20);
  strncpy(config.soTinNhanToiDa_config, "10;10;10", 20);

  config.brokerPort = 1883;
  config.MQTT_update_interval = 10;
  config.reportClock = 7;
  config.powerLostWarningEnable = true;
  config.powerRecoverEnable = false;
  config.waitingTimetoSendPowerLost = 600;
  config.sensorErrorWarningEnable = true;
  config.waitingTimetoSendSensorError = 600;

  oledInforDisplay("Menu cai dat", "...OK", "", 2000);
  config.firstTime = true;

  strncpy(config.wifi_name, "SETCOM_T1", 100);
  strncpy(config.wifi_pass, "66668888", 20);

  configManager.save();
  ESP.restart();
}
float readTemp(int i)
{
  if (testEsp32)
    return (random(20, 40));

  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(i);
  if (tempC != DEVICE_DISCONNECTED_C)
  {

    return (tempC);
  }
  else
  {
    return -999;
  }
}
String readTemHumi()
{
  if (sht3x.measure())
  {
    float t = sht3x.temperature();
    float h = sht3x.humidity();
    // valueDisplay(t, h);
    //   SerialMon.printf("%.1f  %.1f\n", t, h);
    evtemp = t; // Cập nhật biến toàn cục temp
    evhumi = h; // Cập nhật biến toàn cục humi
    return String(t, 1) + ";" + String(h) + ";1";
  }
  else
  {
    if (!sht3x.begin())
      //   // inforDisplay("TH sensor error !");
      evtemp = -999; // Cập nhật biến toàn cục temp
    evhumi = -999;   // Cập nhật biến toàn cục humi
    // SerialMon.printf("%.1f  %.1f\n", -999, -99);
    return "-999;-99;1";
  }
}
void update_progress(int cur, int total)
{
  // Serial.printf("UPDATE: %.0f%% of 100%% (%d bytes)...\n", (double)(cur) / (total) * 100.0, total);
  char sbuf[50];
  sprintf(sbuf, "UPDATE: %.0f%%", (double)(cur) / (total) * 100.0);
  SerialMon.println(sbuf);
  oledInforDisplay("Cap nhat phan mem", sbuf, "setcom.com.vn", 10);
}
void update_started()
{
  Serial.println("CALLBACK:  HTTP update process started");
}
void update_finished()
{
  Serial.println("CALLBACK:  HTTP update process finished");
}
void update_error(int err)
{
  Serial.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}
void httpUpdateFirmware()
{
  int so_lan_thu_ket_noi_Wifi = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    esp_task_wdt_reset();
    so_lan_thu_ket_noi_Wifi++;
    if (so_lan_thu_ket_noi_Wifi > 50)
      ESP.restart();

    delayWithWatchdog(500);
    Serial.print(".");
  }

  WiFiClient client;

  // Add optional callback notifiers
  httpUpdate.onStart(update_started);
  httpUpdate.onEnd(update_finished);
  httpUpdate.onProgress(update_progress);
  httpUpdate.onError(update_error);
  SerialMon.println(sUpdateFirmware);
  t_httpUpdate_return ret = httpUpdate.update(client, sUpdateFirmware);

  switch (ret)
  {
  case HTTP_UPDATE_FAILED:
    Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
    break;

  case HTTP_UPDATE_NO_UPDATES:
    Serial.println("HTTP_UPDATE_NO_UPDATES");
    break;

  case HTTP_UPDATE_OK:
    Serial.println("HTTP_UPDATE_OK");
    break;
  }
}
String getRestartReason()
{
  esp_reset_reason_t reason = esp_reset_reason();
  // In lý do khởi động
  Serial.println("Dưới đây là lý do khởi động lại của ESP:");
  // In lý do khởi động
  switch (reason)
  {
  case ESP_RST_POWERON:
    Serial.println("Khởi động sau khi nguồn điện được bật");
    return "ESP_RST_POWERON";
    break;
  case ESP_RST_EXT:
    Serial.println("Khởi động từ tín hiệu ngoại vi");
    return "ESP_RST_EXT";
    break;
  case ESP_RST_SW:
    Serial.println("Khởi động do yêu cầu từ phần mềm");
    return "ESP_RST_SW";
    break;
  case ESP_RST_PANIC:
    Serial.println("Khởi động do lỗi panic");
    return "ESP_RST_PANIC";
    break;
  case ESP_RST_INT_WDT:
    Serial.println("Khởi động do timeout của WDT nội bộ");
    return "ESP_RST_INT_WDT";
    break;
  case ESP_RST_TASK_WDT:
    Serial.println("Khởi động do timeout của WDT của task");
    return "ESP_RST_TASK_WDT";
    break;
  case ESP_RST_WDT:
    Serial.println("Khởi động do timeout của WDT tổng quát");
    return "ESP_RST_WDT";
    break;
  case ESP_RST_DEEPSLEEP:
    Serial.println("Khởi động sau khi thoát khỏi chế độ Deep Sleep");
    return "ESP_RST_DEEPSLEEP";
    break;
  case ESP_RST_BROWNOUT:
    Serial.println("Khởi động do brownout");
    return "ESP_RST_BROWNOUT";
    break;
  case ESP_RST_SDIO:
    Serial.println("Khởi động từ SDIO");
    return "ESP_RST_SDIO";
    break;
  default:
    Serial.println("Lý do khởi động không xác định");
    return "NOT SPECIFIED";
    break;
  }
}
void mqttSendBootReason()
{
  char ms[200];
  char tp[50];

  char timeString[50]; // Để lưu trữ chuỗi ngày giờ
  strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
  sprintf(ms, "{\"idx\":%s, \"svalue\":\"(%s) (%s) [%s]: %s\"}", logIdx.c_str(), config.device_idx, config.device_name,
          timeString, getRestartReason().c_str());
  sprintf(tp, "domoticz/in");
  mqtt->publish(tp, ms);
}

void fillArrayFromString(float arr[], String numbers, float defaultValue)
{
  // Sử dụng hàm split để phân tách các số từ chuỗi
  int i = 0;
  char *token = strtok(strdup(numbers.c_str()), ";\n");
  while (token != NULL && i < SL_CAM_BIEN)
  {
    // Chuyển đổi token sang kiểu int và đưa vào mảng
    arr[i] = atof(token);
    i++;

    // Lấy token tiếp theo
    token = strtok(NULL, ";");
  }

  // Điền giá trị mặc định cho các phần tử còn thiếu
  while (i < SL_CAM_BIEN)
  {
    arr[i] = defaultValue;
    i++;
  }
}
void fillArrayFromString_INT(int arr[], String numbers, float defaultValue)
{
  // Sử dụng hàm split để phân tách các số từ chuỗi
  int i = 0;
  char *token = strtok(strdup(numbers.c_str()), ";\n");
  while (token != NULL && i < SL_CAM_BIEN)
  {
    // Chuyển đổi token sang kiểu int và đưa vào mảng
    arr[i] = atoi(token);
    i++;

    // Lấy token tiếp theo
    token = strtok(NULL, ";");
  }

  // Điền giá trị mặc định cho các phần tử còn thiếu
  while (i < SL_CAM_BIEN)
  {
    arr[i] = defaultValue;
    i++;
  }
}
void WifiBluetoothOff()
{

  oledInforDisplay("ESP", "ESP OFF", "", 2000);

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  btStop();
}
void WifiBluetoothOn()
{
  oledInforDisplay("ESP", "ESP ON", "", 2000);
  WiFi.mode(WIFI_STA);
  btStart();
  WiFi.begin();
}


bool checkATSimNet()
{

  SIMON();
  int i = 0;

  // modem.setBaud(9600);
  // SerialAT.begin(9600);

  if (!testEsp32)
  {
    //--------------------------
    SerialMon.print("Test AT...");
    oledInforDisplay("Module SIM check...", "Test AT ..", "", 500);
    if (!modem.testAT())
    {
      oledInforDisplay("Module SIM check...", "Test AT..fail", "", 500);
      addToLog("Test AT..fail");
      SerialMon.println("NG");
      SIMOFF();
      return false;
    }
    SerialMon.println("OK");
    oledInforDisplay("Module SIM check...", "Test AT..OK", "", 500);

    //--------------------------

    SerialMon.print("Check SIM ..");
    oledInforDisplay("Module SIM check...", "Check SIM ..", "", 500);
    i = 0;
    while (!modem.getSimStatus())
    {
      oledInforDisplay("Module SIM check...", "Check SIM ..fail", "", 500);
      addToLog("Kiểm tra lắp SIM chưa..NG");
      if (++i >= 2)
      {
        SIMOFF();
        return false;
      }
    }

    SerialMon.println("OK");
    oledInforDisplay("Module SIM check...", "Check SIM .. OK", "", 500);
    //--------------------------


    SerialMon.print("Check NET ..");
    oledInforDisplay("Module SIM check...", "Check NET ..", "", 500);
  }

  i = 0;
  while (!modem.waitForNetwork())
  {
    esp_task_wdt_reset();
    //   SerialMon.println(".");
    // oledInforDisplay("Menu cai dat", ">>S:" + String(modem.getRegistrationStatus()), String(i), 2000);
    if (++i > 2)
    {
      oledInforDisplay("Module SIM check...", "Check NET ..fail", "", 500);
      addToLog("Check NET..fail");
      SIMOFF();
      return false;
    }
  }

  SerialMon.println(" OK");
  oledInforDisplay("Module SIM check...", "Check NET ..OK", "", 500);

  String StringBuffer = String(map(modem.getSignalQuality(), 2, 30, 5, 100));
  oledInforDisplay("SIGNAL", StringBuffer + "%", "", 2000);
  addToLog("Cường độ sóng di động GSM: " + StringBuffer + "%");

  return true;
}

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);
DNSServer dnsServer;
WebServer server(80);

const char *ssid = "CauhinhSMS";
const char *password = "";

void runWifiWebServer()
{
  Serial.begin(115200);

  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  dnsServer.start(DNS_PORT, "*", apIP);

  server.on("/", []()
            {
    String html = "<html><head><title>ESP32 Captive Portal</title>"
                  "<meta charset='UTF-8'>" 
                  "<style>body { font-family: Arial, sans-serif; text-align: center; margin: 0; padding: 0; background-color: #f0f0f0; }"
                  "div.container { max-width: 300px; margin: 50px auto; padding: 20px; background-color: white; border-radius: 15px; }"
                  "input { width: 95%; padding: 10px; margin: 10px 0; border: 1px solid #ddd; border-radius: 5px; }"
                  "input[type=submit] { background-color: #4CAF50; color: white; border: none; }"
                  "input[type=submit]:hover { background-color: #45a049; }</style></head>"
                  "<body><div class='container'><h1>ESP32 Wi-Fi Login</h1>"
                  "<form action='/get'><label>SSID:</label><input type='text' name='ssid'><br>"
                  "<label>Password:</label><input type='text' name='pass'><br>"
                  "<input type='submit' value='Connect'></form></div></body></html>";
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", html); });

  server.on("/get", []()
            {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    Serial.println("SSID: " + ssid);
    Serial.println("Password: " + pass);

    strncpy(config.wifi_name,ssid.c_str(), 100);
    strncpy(config.wifi_pass, pass.c_str(), 20);
    configManager.save();
    
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", "<html><body><h1>Settings Saved</h1></body></html>");
    ESP.restart(); });

  server.begin();

  while (true)
  {
    dnsServer.processNextRequest();
    server.handleClient();
  }
}

void docGiaTriCamBien()
{
  int chiSoCuaCamBienNhiet = 0;

  // Đọc giá trị từ cảm biến nhiệt độ độ ẩm nếu cần
  // Giả sử readTemHumi() cập nhật giá trị vào evtemp và evhumi
  bool daDocNhietDoAm = false;
  for (int i = 0; i < SL_CAM_BIEN - 1; i++)
  {
    if (IdxArray[i] == IdxArray[i + 1])
    {
      daDocNhietDoAm = true;
      break;
    }
  }
  if (daDocNhietDoAm)
  {
    readTemHumi();
  }

  for (int i = 0; i < SL_CAM_BIEN; i++)
  {
    if (i < SL_CAM_BIEN - 1 && IdxArray[i] == IdxArray[i + 1])
    {
      // Xử lý như cảm biến nhiệt độ và độ ẩm
      giaTriCamBien[i] = evtemp + hieuChinhCamBien[i];
      i++; // Bỏ qua phần tử kế tiếp (đã xử lý)
      giaTriCamBien[i] = evhumi + hieuChinhCamBien[i];
    }
    else
    {
      // Xử lý như cảm biến nhiệt độ thông thường
      giaTriCamBien[i] = readTemp(chiSoCuaCamBienNhiet) + hieuChinhCamBien[i];
      chiSoCuaCamBienNhiet++; // Tăng chỉ số của cảm biến nhiệt
    }
  }
}

String readAndPrintSms(int totalMessages)

{
  SerialMon.print("Đang đọc tin nhắn SMS...");
  if (!checkDeviceMode("SMS_ONLY") & !checkDeviceMode("SMS_WIFI"))
  {
    SerialMon.println("NG");
    return "{}";
  }
  if (!checkATSimNet())
  {
    SerialMon.println("NG");
    return "{}";
  }

  modem.sendAT("+CSCS=\"GSM\"");

  SerialMon.println("Reading all SMS from SIM");
  oledInforDisplay("SMS Read", "Reading SMS", "", 200);

  DynamicJsonDocument doc(1024); // Tạo một đối tượng JSON Document với kích thước phù hợp

  for (int i = 1; i <= totalMessages; i++)
  {
    esp_task_wdt_reset();
    modem.sendAT(GF("+CMGR="), i);
    String response = modem.stream.readStringUntil('\n'); // Đọc dòng đầu tiên của kết quả phản hồi

    if (response.indexOf("+CMGR:") >= 0)
    {
      String messageContent = modem.stream.readStringUntil('\n');
      messageContent.trim(); // Loại bỏ khoảng trắng thừa

      String sender = getValue(response, ',', 1);
      sender = sender.substring(1, sender.length() - 1);

      String dateTime = getValue(response, ',', 3);
      dateTime = dateTime.substring(1, dateTime.length());

      String dateTime1 = getValue(response, ',', 4);
      dateTime1 = dateTime1.substring(1, dateTime1.length() - 2);

      dateTime = dateTime + "  " + dateTime1;

      String formattedOutput = dateTime + " " + sender + " " + messageContent;
      doc["SMS" + String(i)] = formattedOutput; // Thêm SMS vào document
    }
  }

  String result;
  serializeJson(doc, result); // Chuyển đổi JSON Document thành chuỗi
  SerialMon.println("OK");

  for (int i = 0; i < SL_CAM_BIEN; i++)
  {
    // Gửi chuỗi JSON tới topic duy nhất
    char tp[100];
    sprintf(tp, "domoticz/parameter/%d", IdxArray[i]);
    mqtt->publish(tp, result.c_str());
  }

  return result;
}

String generateRandomString()
{
  String charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  String randomString = "";

  for (int i = 0; i < 8; i++)
  {
    int randomIndex = random(charset.length());
    randomString += charset[randomIndex];
  }

  return randomString;
}

int getSignalStrengthPercentage(const char *ssid)
{

  int n = WiFi.scanNetworks();

  for (int i = 0; i < n; ++i)
  {
    if (WiFi.SSID(i) == ssid)
    {
      int rssi = WiFi.RSSI(i);
      // Tính toán phần trăm cường độ tín hiệu trực tiếp trong hàm
      int quality = (rssi + 100) * 2;
      if (quality < 0)
        quality = 0;
      if (quality > 100)
        quality = 100;
      return quality;
    }
  }
  return -1; // Trả về -1 nếu không tìm thấy mạng WiFi
}
void GPRSconnect()
{
  // Test kết nối mqtt4G
  checkATSimNet();
  const char apn[] = "v-internet";
  const char user[] = "";
  const char pass[] = "";
  SerialMon.print("GPRS: connecting to ");
  SerialMon.print(apn);
  SerialMon.print("...");
  if (!modem.gprsConnect(apn, user, pass))
  {
    Serial.println("Không thể kết nối GPRS");
  }
  SerialMon.println(" OK");
}

