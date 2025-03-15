// Thay đổi thử nghiệm githu
#include <lib.h>
bool mqttLogUpdate(String idx = logIdx, String content = "");
bool checkATSimNet();
bool smsAlarm(const char *number, const char *noiDungTinNhan);
bool callAlarm(const char *number);
void checkNewUnreadSms(int n);
void paraSetup(bool loadDataOnly = false);
void addToLog(String L);
void downloadAllDataFile(bool forceDownload = false);
void downloadAndSaveFile(const char *url, const char *fileName);
void appendError(const char *logEntry);
void sendLogOverMQTT(String path, String topicRecv, bool js = false, bool isOfflineLog = false);
void AlarmNew(const char *tinNhan)
{
  checkATSimNet();
  checkNewUnreadSms(5);
  for (int j = 0; j < SL_SDT_GOI; j++)
  {
    SerialMon.println(soGoiDienArray[j]);
    callAlarm(soGoiDienArray[j]);
  }

  for (int j = 0; j < SL_SDT_NhanTin; j++)
  {
    SerialMon.println(soNhanTinArray[j]);
    smsAlarm(soNhanTinArray[j], tinNhan);
  }
  SIMOFF();
  WiFi.begin();
}

String getTimeFromSimModule()
{

  SIMON();
  //--------------------------
  SerialMon.print("Test AT...");
  oledInforDisplay("Module SIM check...", "Test AT ..", "", 500);
  if (!modem.testAT())
  {
    oledInforDisplay("Module SIM check...", "Test AT..fail", "", 500);
    addToLog("Test AT..fail");
    SerialMon.println("NG");
    SIMOFF();
    appendError("Get time from SIM800L..NG");
    return "";
  }
  SerialMon.println("OK");
  oledInforDisplay("Module SIM check...", "Test AT..OK", "", 500);

  String dateTime = modem.getGSMDateTime(DATE_FULL);
  dateTime.replace("\"", "");
  SIMOFF();
  return dateTime;
}
void setTimeFromModem()
{
  if (testEsp32)
    return;

  SerialMon.println("Đặt giờ từ modem GSM vào ESP32");
  String gsmTime = getTimeFromSimModule();
  if (gsmTime != "")
  {
    // Serial.println("Thời gian GSM: " + gsmTime);

    int year = gsmTime.substring(0, 2).toInt() + 2000;
    int month = gsmTime.substring(3, 5).toInt();
    int day = gsmTime.substring(6, 8).toInt();
    int hour = gsmTime.substring(9, 11).toInt();
    int minute = gsmTime.substring(12, 14).toInt();
    int second = gsmTime.substring(15, 17).toInt();

    // Serial.printf("Ngày: %02d/%02d/%04d\n", day, month, year);
    // Serial.printf("Giờ: %02d:%02d:%02d\n", hour, minute, second);
    // // Đặt giá trị thời gian vào cấu trúc tm
    struct tm timeinfo;
    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon = month - 1;
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min = minute;
    timeinfo.tm_sec = second;
    // Đặt thời gian cho Esp32Time
    espTime.setTimeStruct(timeinfo);

    Serial.println("Đã đặt thời gian cho ESP32");
    appendError("Set RTC time from SIM800L --> ESP32 .. OK");
    timeIsSet = true;
  }
  else
  {
    Serial.println("Không thể đọc giờ từ modem");
    appendError("Set RTC time from SIM800L --> ESP32 .. NG");
  }
}
void setSim800lTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Lỗi khi lấy thời gian từ ESP32");
    return;
  }

  SIMON();
  //--------------------------
  SerialMon.print("Test AT...");
  oledInforDisplay("Module SIM check...", "Test AT ..", "", 500);
  if (!modem.testAT())
  {
    oledInforDisplay("Module SIM check...", "Test AT..fail", "", 500);
    addToLog("Test AT..fail");
    SerialMon.println("NG");
    appendError("Set time from ESP32 --> SIM800L .. NG (test AT failed)");
    SIMOFF();
    return;
  }
  SerialMon.println("OK");
  oledInforDisplay("Module SIM check...", "Test AT..OK", "", 500);

  // Chuyển đổi thời gian thành chuỗi định dạng YY/MM/DD,hh:mm:ss±zz
  char timeString[30];
  snprintf(timeString, sizeof(timeString), "%02d/%02d/%02d,%02d:%02d:%02d+07",
           timeinfo.tm_year - 100, timeinfo.tm_mon + 1, timeinfo.tm_mday,
           timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

  // Gửi lệnh AT+CCLK=<thời gian> tới SIM800L
  modem.sendAT("+CCLK=\"" + String(timeString) + "\"");

  if (modem.waitResponse() == 1)
  {
    Serial.println("Đã đặt giờ cho SIM800L");
  }
  else
  {
    Serial.println("Không thể đặt giờ cho SIM800L");
    appendError("Set time from ESP32 --> SIM800L .. NG");
  }
  SIMOFF();
}
void sendLogOverMQTT(String path, String topicRecv, bool js, bool isOfflineLog)
{
  Serial.printf("Đang gửi dữ liệu của file %s qua MQTT\n", path.c_str());
  oledInforDisplay(path, "Log update...", "", 1000);

  if (!SPIFFS.begin())
  {
    Serial.println("Không thể khởi động SPIFFS");
    addToLog("Không thể khởi động SPIFFS");
    return;
  }

  if (!SPIFFS.exists(path))
  {
    Serial.printf("File %s không tồn tại\n", path.c_str());
    // addToLog("File " + path + " không tồn tại");
    return;
  }

  File file = SPIFFS.open(path, "r");
  if (!file)
  {
    Serial.printf("Không thể mở file %s\n", path.c_str());
    addToLog("Không thể mở file " + path);
    return;
  }

  int i = 0;
  bool addSpace = true; // Biến để kiểm soát việc thêm dấu cách
  while (file.available())
  {
    String line = file.readStringUntil('\n');
    line.trim();

    if (line.length() > 0)
    {
      // Thay thế dấu nháy kép " bằng ký tự escape \"
      line.replace("\"", "\\\"");

      if (!addSpace)
      {
        line += " "; // Thêm dấu cách vào cuối nếu không giống dòng trước đó
      }
      addSpace = !addSpace; // Đảo ngược giá trị của addSpace

      if (js)
      {
        char ms[200];
        if (isOfflineLog)
        {
          sprintf(ms, "{\"idx\":106, \"svalue\":\"%s\"}", line.c_str());
        }
        else
        {
          sprintf(ms, "{\"idx\":%s, \"svalue\":\"(%s) (%s): %s\"}",
                  errorIdx.c_str(), config.device_idx, config.device_name, line.c_str());
        }

        mqtt->publish(topicRecv.c_str(), ms);
        Serial.println(".");
      }
      else
      {
        //"offlineLogTopic"
        if (!mqtt->publish(topicRecv.c_str(), line.c_str()))
        {
          Serial.println("Failed to publish message to MQTT Broker");
          addToLog("Failed to publish message to MQTT Broker");
        }
        else
        {
          i++;
          oledInforDisplay(path, "Log update..." + String(i), "", 500);
          Serial.println(".");
        }
      }
    }
  }
  // Đóng tệp tin sau khi hoàn thành
  file.close();

  // Sau khi gửi dữ liệu lên MQTT, xóa tệp
  if (SPIFFS.remove(path))
  {
    Serial.printf("Tệp %s đã được xóa sau khi gửi thành công\n", path.c_str());
  }
  else
  {
    Serial.printf("Không thể xóa tệp %s sau khi gửi\n", path.c_str());
  }
}
void appendError(const char *logEntry)
{
  Serial.println("Ghi vào errorlog");
  if (!SPIFFS.begin())
  {
    Serial.println("Khởi tạo không thành công bộ nhớ SPIFFS, đang định dạng...");
    return;
  }

  // Kiểm tra dung lượng trống
  size_t totalBytes = SPIFFS.totalBytes();
  size_t usedBytes = SPIFFS.usedBytes();
  size_t freeBytes = totalBytes - usedBytes;
  if (freeBytes < 100000)
  {
    Serial.println("Không đủ dung lượng trống để ghi error");
    addToLog("Not enough free space to add ErrorLog");
    return; // Dừng việc ghi log
  }
  // Mở file trong chế độ ghi (append)
  File file = SPIFFS.open(ErrorFilePath, FILE_APPEND);
  if (!file)
  {
    Serial.println("Failed to open log file");
    addToLog("Failed to open log file errorLog");
    return;
  }
  // Ghi log vào file
  String updatedLog = timeString() + "\t" + logEntry;
  // Ghi log vào file
  file.println(updatedLog);
  file.close();
}
void appendLog(const char *logEntry)
{
  Serial.println("Đang khởi tạo SPIFFS...");
  if (!SPIFFS.begin())
  {
    Serial.println("Khởi tạo không thành công bộ nhớ SPIFFS, đang định dạng...");
    return;
  }
  // Kiểm tra dung lượng trống
  size_t totalBytes = SPIFFS.totalBytes();
  size_t usedBytes = SPIFFS.usedBytes();
  size_t freeBytes = totalBytes - usedBytes;
  if (freeBytes < 100000)
  {
    Serial.println("Không đủ dung lượng trống để ghi log");
    addToLog("Not enough free space to add offlineLog");
    return; // Dừng việc ghi log
  }
  // Mở file trong chế độ ghi (append)
  File file = SPIFFS.open(OfflineLogfilePath, FILE_APPEND);
  if (!file)
  {
    Serial.println("Failed to open log file");
    addToLog("Failed to open log file");
    return;
  }
  // Ghi log vào file
  file.println(logEntry);
  file.close();
}
void printOfflineLog(const String &path)
{
  Serial.printf("In nội dung %s lúc khởi động ...\n", path.c_str());

  if (!SPIFFS.begin())
  {
    Serial.println("Không thể khởi động SPIFFS");
    return;
  }

  File file = SPIFFS.open(path, "r");
  if (!file)
  {
    Serial.printf("Không thể mở file %s\n", path.c_str());
    return;
  }

  while (file.available())
  {
    String line = file.readStringUntil('\n');
    Serial.println(line);
  }

  file.close();
}
void smartConfig()
{
  // Tắt giao thức WiFi hiện tại
  oledInforDisplay("Menu cai dat", "App:EspTouch..", "", 0);
  WiFi.mode(WIFI_MODE_NULL);
  // Khởi động SmartConfig
  WiFi.beginSmartConfig();
  Serial.println("Vui lòng kết nối ESP32 với mạng WiFi qua ứng dụng di động SmartConfig.");
  // Chờ kết nối
  while (!WiFi.smartConfigDone())
  {
    delayWithWatchdog(500);
    Serial.print(".");
    esp_task_wdt_reset();
  }
  Serial.println("");
  Serial.println("Đã kết nối đến WiFi bằng SmartConfig!");
  String ssid = WiFi.SSID();
  String password = WiFi.psk();

  paraSetup(true);

  strncpy(config.wifi_name, ssid.c_str(), 100);
  strncpy(config.wifi_pass, password.c_str(), 20);
  configManager.save();
}

bool mqttLogUpdate(String idx, String content)
{
  char ms[4096];
  char tp[50];
  sprintf(ms, "{\"idx\":%s, \"svalue\":\"(%s) (%s) [%s]:\r\n %s\"}", idx.c_str(), config.device_idx, config.device_name,
          timeString().c_str(), content.c_str());
  sprintf(tp, "domoticz/in");
  if (mqtt->publish(tp, ms))
    return true;
  else
    return false;
}
void downloadAllDataFile(bool forceDownload)
{
  // Khởi tạo SPIFFS
  if (!SPIFFS.begin(true))
  {
    Serial.println("Không thể khởi tạo SPIFFS");
    return;
  }
  // Kiểm tra sự tồn tại của các tệp tin
  bool caidatExists = SPIFFS.exists("/caidat.html");
  bool indexExists = SPIFFS.exists("/index.html");
  bool mainExists = SPIFFS.exists("/main.js");
  bool stylesExists = SPIFFS.exists("/styles.css");
  if (forceDownload)
  {
    downloadAndSaveFile(url_caidat_html, fileName_caidat_html);
    downloadAndSaveFile(url_index_html, fileName_index_html);
    downloadAndSaveFile(url_main_js, fileName_main_js);
    downloadAndSaveFile(url_styles_css, fileName_styles_css);
  }
  // Trả về true nếu tất cả các tệp tin tồn tại, ngược lại trả về false
  if (!(caidatExists && indexExists && mainExists && stylesExists))
  {
    downloadAndSaveFile(url_caidat_html, fileName_caidat_html);
    downloadAndSaveFile(url_index_html, fileName_index_html);
    downloadAndSaveFile(url_main_js, fileName_main_js);
    downloadAndSaveFile(url_styles_css, fileName_styles_css);
  }
  else
  {
    Serial.println("OK");
  }
}
void downloadAndSaveFile(const char *url, const char *fileName)
{
  HTTPClient http;
  oledInforDisplay("Download data...", "download...", "", 0);
  Serial.print("Đang tải xuống tệp từ: ");
  Serial.println(url);

  // Kiểm tra sự tồn tại của tệp
  if (SPIFFS.exists(fileName))
  {
    Serial.println("Tệp đã tồn tại. Đang ghi đè...");
  }

  // Bắt đầu tải xuống
  http.begin(url);

  // Tải xuống và lưu trữ tệp
  int httpResponseCode = http.GET();
  if (httpResponseCode == HTTP_CODE_OK)
  {
    // Mở tệp để ghi
    File file = SPIFFS.open(fileName, "w");
    if (!file)
    {
      Serial.println("Lỗi khi mở tệp để ghi");
      return;
    }

    // Lấy dữ liệu từ phản hồi HTTP dưới dạng chuỗi
    String response = http.getString();

    // Ghi dữ liệu vào tệp
    file.print(response);

    // Đóng tệp và kết thúc tải xuống
    file.close();
    Serial.println("Đã tải xuống và lưu trữ tệp");
  }
  else
  {
    Serial.print("Lỗi HTTP: ");
    Serial.println(httpResponseCode);
  }

  // Kết thúc kết nối HTTP
  http.end();
  oledInforDisplay("Download data...", "download...OK", "", 2000);
}
void printLocalTime()
{

  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Chưa được đặt giờ");
    return;
  }
}
void timeavailable(struct timeval *t)
{
  Serial.println("Đã lấy được giờ từ internet");
  printLocalTime();
  setSim800lTimeFlag = true;
  timeIsSet = true;
}

bool dailyReportSMS(String smsNumberString)
{
  SerialMon.println("");
  if (!checkDeviceMode("SMS_ONLY") & !checkDeviceMode("SMS_WIFI"))
    return false;

  if (!testEsp32)
    if (!checkATSimNet())
      return false;

  char noiDungTinNhan_nhap[300];

  if (SL_CAM_BIEN == 1)
  {
    snprintf(noiDungTinNhan_nhap, sizeof(noiDungTinNhan_nhap),
             "Hom nay la %s\nThong tin buoi sang \nTen: %s idx:%s \nNhiet do la: %.1f \n Xin cam on",
             generateRandomString().c_str(),
             config.device_name,
             config.device_idx,
             giaTriCamBien[0]);
  }
  else
  {
    snprintf(noiDungTinNhan_nhap, sizeof(noiDungTinNhan_nhap),
             "Hom nay la %s \nThong tin buoi sang\nTen: %s idx:%s\nNhiet do tu: %.1f\nNhiet do phong: %.1f\nDo am phong: %.0f\nXin cam on",
             generateRandomString().c_str(),
             config.device_name,
             config.device_idx,
             giaTriCamBien[0],
             giaTriCamBien[1],
             giaTriCamBien[2]);
  }

  bool bien_tam = false;
  for (int i = 0; i < SL_SDT_tinNhanHangNgay; i++)
  {
    if (smsAlarm(soNhanTinHangNgayArray[i], noiDungTinNhan_nhap))
      bien_tam = true;
  }
  SIMOFF();
  return bien_tam;
}
void nhanSMS_hangTuan()
{

  if (config.dailySMSEnable)
    return;

  SerialMon.println("Kiểm tra tin nhắn hàng tuần");
  int currentDay = timeinfo.tm_wday;

  if (currentDay == ngayNhanTin_hangTuan - 1 &&
      !config.trangthaiNhantinTheoTuan &&
      timeinfo.tm_hour == 0 &&
      timeinfo.tm_min < 10)
  {
    bool success = false;
    for (int attempts = 0; attempts < 3 && !success; attempts++)
    {
      success = dailyReportSMS("0913579048");
      if (!success)
      {
        addToLog("Nhắn tin hàng tuần Thất bại");
        delay(5000); // Đợi 5 giây nếu thất bại
      }
      else
      {
        addToLog("Nhắn tin hàng tuần thành công");
        config.trangthaiNhantinTheoTuan = true;
        configManager.save();
        break;
      }
    }

    if (!success)
    {
      config.trangthaiNhantinTheoTuan = true;
      configManager.save();
      SerialMon.println("Gửi tin nhắn Thất bại hàng tuần sau 3 lần !");
      addToLog("Gửi tin nhắn Thất bại hàng tuần sau 3 lần !");
    }
  }

  if (currentDay != ngayNhanTin_hangTuan - 1 && config.trangthaiNhantinTheoTuan)
  {
    config.trangthaiNhantinTheoTuan = false;
    configManager.save();
  }
}

void mqttSendConfig()
{
  oledInforDisplay("MQTT", "Send message !", "", 500);

  DynamicJsonDocument jsonBuffer(8000);

  JsonObject root = jsonBuffer.to<JsonObject>();

  // Đóng gói các trường JSON trực tiếp vào JSON lớn
  root["projectName"] = PROJECT_NAME;
  root["volteStatus"] = config.volteStatus;
  root["dailySMSEnable"] = config.dailySMSEnable;
  root["soNhanTinHangNgay"] = config.soNhanTinHangNgay;
  root["ReportClock"] = config.reportClock;
  root["SendReportStatus"] = config.sendReportStatus;
  root["weeklyStatus"] = config.trangthaiNhantinTheoTuan;
  root["Min"] = config.giaTriToiThieu_config;
  root["Max"] = config.giaTriToiDa_config;
  root["tempBias"] = config.hieuChinhCamBien_config;
  root["CHOPHEPSENSORCANHBAO"] = config.choPhepSensorCanhBao_config;

  char clockString[30]; // Đủ lớn để chứa chuỗi ghép
  snprintf(clockString, sizeof(clockString), "%s", timeString().c_str());
  root["Clock"] = clockString;

  root["Hardware version"] = version;
  root["powerLostWarningEnable"] = config.powerLostWarningEnable;
  root["PowerRecoverEnable"] = config.powerRecoverEnable;
  root["waitingTimetoSendPowerLost"] = config.waitingTimetoSendPowerLost;
  root["sensorErrorWarningEnable"] = config.sensorErrorWarningEnable;
  root["waitingTimetoSendSensorError"] = config.waitingTimetoSendSensorError;
  root["device_name"] = config.device_name;
  root["RunMode"] = config.runMode;
  root["device_idx"] = config.device_idx;
  root["donViHienThi"] = config.donVi_hienThi_config;
  char verString[30]; // Đủ lớn để chứa chuỗi ghép
  snprintf(verString, sizeof(verString), "%s %s", __DATE__, __TIME__);
  root["Ver"] = verString;

  root["Send"] = config.thoiGianChoCanhBao_config;
  root["Repeat"] = config.thoiGianChoLapLai_CanhBao_config;
  root["Time"] = config.soTinNhanToiDa_config;
  root["UP_Interval"] = config.MQTT_update_interval;

  root["soGoiDien"] = config.soGoiDien;
  root["soNhanTin"] = config.soNhanTin;

  // root["no1"] = config.no1;
  // root["SMS1"] = config.e_s_no1;
  // root["Call1"] = config.e_c_no1;
  // root["no2"] = config.no2;
  // root["SMS2"] = config.e_s_no2;
  // root["Call2"] = config.e_c_no2;
  // root["no3"] = config.no3;
  // root["SMS3"] = config.e_s_no3;
  // root["Call3"] = config.e_c_no3;
  // root["no4"] = config.no4;
  // root["SMS4"] = config.e_s_no4;
  // root["Call4"] = config.e_c_no4;
  // root["no5"] = config.no5;
  // root["SMS5"] = config.e_s_no5;
  // root["Call5"] = config.e_c_no5;
  // root["no6"] = config.no6;
  // root["SMS6"] = config.e_s_no6;
  // root["Call6"] = config.e_c_no6;
  // root["no7"] = config.no7;
  // root["SMS7"] = config.e_s_no7;
  // root["Call7"] = config.e_c_no7;
  // root["no8"] = config.no8;
  // root["SMS8"] = config.e_s_no8;
  // root["Call8"] = config.e_c_no8;
  // root["no9"] = config.no9;
  // root["SMS9"] = config.e_s_no9;
  // root["Call9"] = config.e_c_no9;
  // root["no10"] = config.no10;
  // root["SMS10"] = config.e_s_no10;
  // root["Call10"] = config.e_c_no10;

  // cho phép relay đầu ra
  root["relayOutputEnable"] = config.relayOutputEnable;
  // Wifi thu 1
  root["first_wifi_name"] = config.wifi_name;
  root["first_wifi_pass"] = config.wifi_pass;

  // Wifi thu 2
  root["second_wifi_name"] = config.second_wifi_name;
  root["second_wifi_pass"] = config.second_wifi_pass;

  root["using_wifi"] = WiFi.SSID();
  // Chuyển đổi JSON lớn thành chuỗi JSON
  String jsonString;
  serializeJson(jsonBuffer, jsonString);
  jsonString = sanitizeString(jsonString);

  for (int i = 0; i < SL_CAM_BIEN; i++)
  {
    char tp[100];
    sprintf(tp, "domoticz/parameter/%d", IdxArray[i]);
    mqtt->publish(tp, jsonString.c_str());
    delayWithWatchdog(100);
  }
}
void mqttCallback(char *topic, byte *payload, unsigned int len)
{
  SerialMon.print("Message arrived [");
  SerialMon.print(topic);
  SerialMon.print("]:[");
  Serial.print(len);
  SerialMon.print("]: ");
  char localBuf[4000] = "";
  char buf[10] = "";
  for (int i = 0; i < len; i++)
  {
    esp_task_wdt_reset();
    localBuf[i] = (char)payload[i];
  }

  String s = String(localBuf);
  s = jsonToUpperCaseKeys(s.c_str());
  SerialMon.println(s);
  DynamicJsonDocument doc(4000);
  DeserializationError error = deserializeJson(doc, s.c_str());

  if (error)
  {

    if (s.indexOf("REBOOT") >= 0)
    {
      ESP.restart();
    }

    if (s.indexOf("DOWNLOADDATAFILE") >= 0)
    {
      downloadAllDataFile(true);
    }

    if (s.indexOf("ALARMTEST") >= 0)
    {
      //  WifiBluetoothOff();
      char noiDungTinNhan_nhap[300];
      snprintf(noiDungTinNhan_nhap, sizeof(noiDungTinNhan_nhap), "TIN NHAN KIEM TRA HE THONG ALARMTEST \nTen TB: %s (idx:%s)\n",
               config.device_name,
               config.device_idx);

      AlarmNew(noiDungTinNhan_nhap);
      //  WifiBluetoothOn();
      SIMOFF();
    }

    if (s.indexOf("REPORTTEST") >= 0)
    {

      dailyReportSMS(config.soNhanTinHangNgay);
    }

    if (s.indexOf("DUNG") >= 0)
    {
      // WifiBluetoothOff();
      if (!checkATSimNet())
        return;
      callAlarm("0965261265");
      char noiDungTinNhan_nhap[300];
      snprintf(noiDungTinNhan_nhap, sizeof(noiDungTinNhan_nhap), "TIN NHAN KIEM TRA HE THONG ALARMTEST\nTen TB: %s (idx:%s)\n",
               config.device_name,
               config.device_idx);
      smsAlarm("0965261265", noiDungTinNhan_nhap);
      //  WifiBluetoothOn();
      SIMOFF();
    }

    if (s.indexOf("THO") >= 0)
    {
      // WifiBluetoothOff();
      if (!checkATSimNet())
        return;
      callAlarm("0913579048");
      char noiDungTinNhan_nhap[300];
      snprintf(noiDungTinNhan_nhap, sizeof(noiDungTinNhan_nhap), "TIN NHAN KIEM TRA HE THONG (ALARMTEST)!\nTen TB: %s (idx:%s)\n",
               config.device_name,
               config.device_idx);
      smsAlarm("0913579048", noiDungTinNhan_nhap);
      // WifiBluetoothOn();
      SIMOFF();
    }

    if (s.indexOf("SCANWIFI") >= 0)
    {
      Mqtt_scanWifiAndCreateJson();
    }

    if (s.indexOf("READSMS") >= 0)
    {
      readAndPrintSms(30);
    }

    if (s.indexOf("SENDSMS") >= 0)
    {
      if (!checkATSimNet())
        return;
      modem.sendSMS(getValue(s, '@', 1), getValue(s, '@', 2));
    }

    if (s.indexOf("CALL") >= 0)
    {
      if (!checkATSimNet())
        return;
      modem.callNumber(getValue(s, '@', 1));
    }

    if (s.indexOf("DELETESMS") >= 0)
    {
      if (!checkATSimNet())
        return;
      modem.sendAT("+CMGD=1,4");
    }
    if (s.indexOf("UPDATE") >= 0)
    {
      if (!wifi_using)
      {
        SerialMon.println("Huỷ bỏ cập nhật do đang kết nối 4G");
        oledInforDisplay("Cap nhat phan mem", "update(4G) cancelled)", "setcom.com.vn", 1000);
        return;
      }
      downloadAllDataFile(true);
      mqtt->disconnect();
      SerialMon.println("Cap nhat");
      oledInforDisplay("Cap nhat phan mem", "update", "setcom.com.vn", 1000);
      unsigned long now100 = millis();
      while (true)
      {
        esp_task_wdt_reset();
        httpUpdateFirmware();
        if (millis() - now100 > 600000L)
          break;
      }
    }
  }
  else
  {
    if (doc.containsKey("SOGOIDIEN"))
    {
      const char *sub = doc["SOGOIDIEN"];
      strcpy(config.soGoiDien, sub);
      tachSoDienThoai(sub, soGoiDienArray);
    }
    if (doc.containsKey("SONHANTIN"))
    {
      const char *sub = doc["SONHANTIN"];
      strcpy(config.soNhanTin, sub);
      tachSoDienThoai(sub, soNhanTinArray);
    }
    if (doc.containsKey("CHOPHEPSENSORCANHBAO"))
    {
      const char *sub = doc["CHOPHEPSENSORCANHBAO"];
      strcpy(config.choPhepSensorCanhBao_config, sub);
      fillArrayFromString_INT(choPhepSensorCanhBao, config.choPhepSensorCanhBao_config, 1);
    }

    if (doc.containsKey("SONHANTINHANGNGAY"))
    {
      const char *sub = doc["SONHANTINHANGNGAY"];
      strcpy(config.soNhanTinHangNgay, sub);
      tachSoDienThoai(sub, soNhanTinHangNgayArray);
    }

    if (doc.containsKey("DONVIHIENTHI"))
    {
      const char *sub = doc["DONVIHIENTHI"];
      strcpy(config.donVi_hienThi_config, sub);
      tachChuoiCon(sub, donVi_hienThi);
    }

    if (doc.containsKey("DEVICE_NAME") || doc.containsKey("NAME"))
    {
      const char *sub = doc["DEVICE_NAME"];
      if (strlen(sub) > 0)
        strcpy(config.device_name, sub);
    }
    if (doc.containsKey("DEVICE_IDX") || doc.containsKey("IDX"))
    {
      const char *sub = doc["DEVICE_IDX"];
      if (strlen(sub) > 0)
      {

        for (int i = 0; i < SL_CAM_BIEN; i++)
        {
          char tp[100];
          sprintf(tp, "readconfig/%d", IdxArray[i]);
          mqtt->unsubscribe(tp);
        }
        strcpy(config.device_idx, sub);
        fillArrayFromString_INT(IdxArray, sub, 30);

        int SL_CAM_BIEN_phu = demPhanTu(sub);
        for (int i = 0; i < SL_CAM_BIEN_phu; i++)
        {
          char tp[100];
          sprintf(tp, "readconfig/%d", IdxArray[i]);
          mqtt->subscribe(tp);
        }
        if (SL_CAM_BIEN != SL_CAM_BIEN_phu)
        {
          configManager.save();
          ESP.restart();
        }
      }
    }

    if (doc.containsKey("SEND"))
    {
      const char *sub = doc["SEND"];
      strcpy(config.thoiGianChoCanhBao_config, sub);
      fillArrayFromString_INT(thoiGianChoCanhBao, config.thoiGianChoCanhBao_config, 300);
    }
    if (doc.containsKey("REPEAT"))
    {
      const char *sub = doc["REPEAT"];
      strcpy(config.thoiGianChoLapLai_CanhBao_config, sub);
      fillArrayFromString_INT(thoiGianChoLapLai_CanhBao, config.thoiGianChoLapLai_CanhBao_config, 300);
    }
    if (doc.containsKey("TIME"))
    {
      const char *sub = doc["TIME"];
      strcpy(config.soTinNhanToiDa_config, sub);
      fillArrayFromString_INT(soTinNhanToiDa, config.soTinNhanToiDa_config, 10);
    }

    if (doc.containsKey("MAX"))
    {
      const char *sub = doc["MAX"];
      strcpy(config.giaTriToiDa_config, sub);
      fillArrayFromString(giaTriToiDa, config.giaTriToiDa_config, 30);
    }
    if (doc.containsKey("MIN"))
    {
      const char *sub = doc["MIN"];
      strcpy(config.giaTriToiThieu_config, sub);
      fillArrayFromString(giaTriToiThieu, config.giaTriToiThieu_config, 0);
    }
    if (doc.containsKey("RUNMODE"))
    {
      bool e = doc["RUNMODE"];
      config.runMode = e;
      configManager.save();
    }
    if (doc.containsKey("UP_INTERVAL"))
    {
      unsigned int e = doc["UP_INTERVAL"];
      config.MQTT_update_interval = e;
    }

    if (doc.containsKey("REPORTCLOCK"))
    {
      unsigned int e = doc["REPORTCLOCK"];
      config.reportClock = e;
    }
    if (doc.containsKey("VOLTESTATUS"))
    {
      unsigned int e = doc["VOLTESTATUS"];
      config.volteStatus = e;
    }
    if (doc.containsKey("WEEKLYSTATUS"))
    {
      unsigned int e = doc["WEEKLYSTATUS"];
      config.trangthaiNhantinTheoTuan = e;
    }

    if (doc.containsKey("SENDREPORTSTATUS"))
    {
      bool e = doc["SENDREPORTSTATUS"];
      config.sendReportStatus = e;
    }

    if (doc.containsKey("POWERLOSTWARNINGENABLE"))
    {
      bool e = doc["POWERLOSTWARNINGENABLE"];
      config.powerLostWarningEnable = e;
    }
    if (doc.containsKey("WAITINGTIMETOSENDPOWERLOST"))
    {
      unsigned int e = doc["WAITINGTIMETOSENDPOWERLOST"];
      config.waitingTimetoSendPowerLost = e;
    }
    if (doc.containsKey("POWERRECOVERENABLE"))
    {
      bool e = doc["POWERRECOVERENABLE"];

      config.powerRecoverEnable = e;
    }

    if (doc.containsKey("SENSORERRORWARNINGENABLE"))

    {
      bool e = doc["SENSORERRORWARNINGENABLE"];
      config.sensorErrorWarningEnable = e;
    }
    if (doc.containsKey("WAITINGTIMETOSENDSENSORERROR"))
    {
      unsigned int e = doc["WAITINGTIMETOSENDSENSORERROR"];
      config.waitingTimetoSendSensorError = e;
    }

    if (doc.containsKey("TEMPBIAS"))
    {
      const char *sub = doc["TEMPBIAS"];
      strcpy(config.hieuChinhCamBien_config, sub);
      fillArrayFromString(hieuChinhCamBien, config.hieuChinhCamBien_config, 0);
    }

    if (doc.containsKey("DAILYSMSENABLE"))
    {
      bool e = doc["DAILYSMSENABLE"];
      config.dailySMSEnable = e;
    }

    if (doc.containsKey("RELAYOUTPUTENABLE"))
    {
      bool e = doc["RELAYOUTPUTENABLE"];
      config.relayOutputEnable = e;
    }

    if (doc.containsKey("SECOND_WIFI_NAME"))
    {
      const char *sub = doc["SECOND_WIFI_NAME"];
      strcpy(config.second_wifi_name, sub);
    }
    if (doc.containsKey("SECOND_WIFI_PASS"))
    {
      const char *sub = doc["SECOND_WIFI_PASS"];
      strcpy(config.second_wifi_pass, sub);
    }

    if (doc.containsKey("FIRST_WIFI_NAME"))
    {
      const char *sub = doc["FIRST_WIFI_NAME"];
      strcpy(config.wifi_name, doc["FIRST_WIFI_NAME"]);
    }

    if (doc.containsKey("FIRST_WIFI_PASS"))
    {
      const char *sub = doc["FIRST_WIFI_PASS"];
      strcpy(config.wifi_pass, doc["FIRST_WIFI_PASS"]);
    }

    configManager.save();
    mqttSendConfig();
  }
}
void mqttSetupParameter()
{

  SerialMon.print("MQTT Broker setup...");
  mqtt->setBufferSize(4096);
  mqtt->setKeepAlive(40);
  mqtt->setSocketTimeout(40);
  mqtt->setServer(config.broker, config.brokerPort);
  mqtt->setCallback(mqttCallback);
  SerialMon.println(" OK");
}
boolean mqttConnect()
{
  if (!wifi_using)
  {
    GPRSconnect();
  }

  SerialMon.print("Connecting to ");
  SerialMon.print(config.broker);
  // String willTopic = String("domoticz/st/") + config.device_idx;
  String strBuff = String(WiFi.macAddress());

  strBuff.replace(":", "_");
  // Serial.print("\tMAC: ");
  // Serial.println(strBuff);

  boolean status = mqtt->connect(strBuff.c_str(), config.mqttUser, config.mqttPass); //, willTopic.c_str(), willQoS, willRetain, willMessage);

  if (status == false)
  {
    SerialMon.println(" fail");
    return false;
  }

  SerialMon.println(" success");
  String sbuf;

  for (int i = 0; i < SL_CAM_BIEN; i++)
  {
    char tp[100];
    sprintf(tp, "readconfig/%d", IdxArray[i]);
    mqtt->subscribe(tp);
  }
  
  sbuf = "readconfig";
  mqtt->subscribe(sbuf.c_str());

  mqttSendConfig();
  if (!sendBootReasonStatus)
  {
    mqttSendBootReason();
    sendBootReasonStatus = true;
  }
  sendLogOverMQTT(OfflineLogfilePath, "domoticz/in", true, true);
  sendLogOverMQTT(ErrorFilePath, "domoticz/in", true, false);
  return mqtt->connected();
}
bool callAlarm(const char *number)
{
  if (!checkDeviceMode("SMS_ONLY") & !checkDeviceMode("SMS_WIFI"))
  {
    return true;
  }

  if (strlen(number) <= CallSMSnumberLength)
    return true;

  SerialMon.println("Calling: " + String(number));
  oledInforDisplay("Alarm calling...", String(number) + " ..", "", 1000);
  unsigned long now = millis();
  bool res = false;
  while (true)
  {
    res = modem.callNumber(number);
    if (res)
      break;
    delayWithWatchdog(1000);
    if (millis() - now > 20000L)
      break;
  };
  SerialMon.printf("Call: %s\r\n", res ? "OK" : "fail");
  String sbuf = res ? "OK" : "fail";
  oledInforDisplay("Alarm calling...", String(number) + " .." + res, "", 1000);
  if (res)
  {
    modem.sendAT(GF(""));
    int8_t status = modem.waitResponse(30000L, GF("BUSY"),
                                       GF("NO ANSWER"), GF("NO CARRIER"), GF("VOICE CALL: BEGIN"));
    if (status == 4)
    {
      int res = modem.callHangup();
      SerialMon.printf("Hang up: %s\r\n", res ? "OK" : "fail");
    }
    SerialMon.println(status);
  }
  else
  {
    SerialMon.println("Call failed");
  }
  addToLog("Đang gọi tới số: " + String(number) + "..." + String(res ? "Thành công" : "Thất bại"));
  delayWithWatchdog(1000);
  return res;
}
bool smsAlarm(const char *number, const char *noiDungTinNhan)
{
  // Kiểm tra chế độ thiết bị và độ dài số điện thoại
  if (!checkDeviceMode("SMS_ONLY") && !checkDeviceMode("SMS_WIFI"))
    return false;
  if (strlen(number) < CallSMSnumberLength)
    return false;

  // Hiển thị thông tin trên Serial và OLED (nếu có)
  SerialMon.println("SMS to: " + String(number));
  oledInforDisplay("SMS sending...", String(number) + " ..", "", 1000);

  // Gửi tin nhắn
  bool res = modem.sendSMS(number, noiDungTinNhan);

  // Xử lý kết quả và cập nhật trên OLED và nhật ký
  oledInforDisplay("SMS sending...", String(number) + " .." + res, "", 1000);
  addToLog("Đang SMS tới số : " + String(number) + "..." + (res ? "Thành công" : "Thất bại"));

  // Đợi một khoảng thời gian ngắn
  delayWithWatchdog(1000);
  return res;
}
void checkNewUnreadSms(int n)
{
  if (!checkDeviceMode("SMS_ONLY") & !checkDeviceMode("SMS_WIFI"))
  {
    return;
  }
  SerialMon.println("Check and delete all SMS in SIM");
  oledInforDisplay("SMS delete", "del.. SMS", "", 200);
  for (int i = 1; i <= n; i++)
  {
    esp_task_wdt_reset();
    modem.sendAT(GF("+CMGR="), i);
    oledInforDisplay("SMS delete", "del.. SMS " + String(i) + "/" + String(n), "", 0);
    String result = modem.stream.readString();
    if (result.indexOf("+CMGR:") >= 0)
    {
      // result.replace("\n", "@");
      // result.replace("\r", "@");
      // String smsHeader = result.substring(result.indexOf("@@@+CMGR:") + 9, result.indexOf("\"@@") + 1);
      // String smsContent = result.substring(result.indexOf("\"@@") + 3, result.indexOf("@@@@OK@@"));
      // smsContent.toUpperCase();
      // String sender = getValue(smsHeader, ',', 1);
      // sender.replace("\"", "");
      // SerialMon.print(sender);
      // SerialMon.printf("\t%s\t%d\r\n", smsContent, i);
      // if (smsContent.indexOf("NHIET DO") >= 0)
      // {
      //   oledInforDisplay("SMS temp from..", sender, "sending it back ...", 2000);
      //   char buffer[120];
      //   sprintf(buffer, "Thiet bi: %s\r\nNhiet do: %.1f do C\r\n(Min=%.1f*C Max=%.1f*C)", config.device_name, readTemp(0), 0, 0);
      //   bool res = modem.sendSMS(sender, String(buffer));
      //   SerialMon.printf("SMS: %s\r\n", res ? "OK" : "fail");
      //   String sbuf = res ? "OK" : "fail";
      //   oledInforDisplay("SMS temp from..", sender + " .." + res, "sent it back ...", 2000);

      //   if (res)
      //     modem.sendAT(GF("+CMGD="), i);
      //   modem.waitResponse(2000L, GF("OK"));
      // }

      modem.sendAT(GF("+CMGD="), i);
      modem.waitResponse(1000L, GF("OK"));
    }
  }
}
void update_prgs(size_t i, size_t total)
{
  oledInforDisplay("updating", String(((i * 100) / total)) + "%", "", 0);
  Serial.printf("upgrade %d/%d   %d%%\n", i, total, ((i * 100) / total));
}
void APCallback(WebServer *server)
{
  server->on("/styles.css", HTTPMethod::HTTP_GET, [server]()
             { configManager.streamFile(stylesCSS, mimeCSS); });
}
void APICallback(WebServer *server)
{
  server->on("/disconnect", HTTPMethod::HTTP_GET, [server]()
             { configManager.clearWifiSettings(false); });

  server->on("/caidat.html", HTTPMethod::HTTP_GET, [server]()
             { configManager.streamFile(settingsHTML, mimeHTML); });

  server->on("/styles.css", HTTPMethod::HTTP_GET, [server]()
             { configManager.streamFile(stylesCSS, mimeCSS); });

  server->on("/main.js", HTTPMethod::HTTP_GET, [server]()
             { configManager.streamFile(mainJS, mimeJS); });
}
void paraSetup(bool loadDataOnly)
{
  // SerialMon.printf("Func: paraSetup... \r\n");
  DEBUG_MODE = false;
  DebugPrintln(F(""));

  configManager.setAPName("CauhinhSMS");
  configManager.setAPFilename("/index.html");

  // Settings variables
  configManager.addParameter("device_name", config.device_name, 20);
  configManager.addParameter("device_mode", config.device_mode, 20);

  configManager.addParameter("device_idx", config.device_idx, 30);
  // ---Cai dat mang di dong
  configManager.addParameter("apn", config.apn, 20);
  configManager.addParameter("user", config.user, 20);
  configManager.addParameter("pwd", config.pwd, 20);
  configManager.addParameter("broker", config.broker, 32);
  configManager.addParameter("mqttUser", config.mqttUser, 20);
  configManager.addParameter("mqttPass", config.mqttPass, 20);
  configManager.addParameter("SERVER_TOPIC_RECEIVE_DATA", config.SERVER_TOPIC_RECEIVE_DATA, 32);
  configManager.addParameter("brokerPort", &config.brokerPort);
  configManager.addParameter("MQTT_update_interval", &config.MQTT_update_interval);

  configManager.addParameter("giaTriToiDa_config", config.giaTriToiDa_config, 20);
  configManager.addParameter("giaTriToiThieu_config", config.giaTriToiThieu_config, 20);

  configManager.addParameter("thoiGianChoCanhBao_config", config.thoiGianChoCanhBao_config, 20);
  configManager.addParameter("thoiGianChoLapLai_CanhBao_config", config.thoiGianChoLapLai_CanhBao_config, 20);
  configManager.addParameter("soTinNhanToiDa_config", config.soTinNhanToiDa_config, 20);

  configManager.addParameter("powerWaitingToSend", &config.powerWaitingToSend);
  configManager.addParameter("powerWaitingToRepeat", &config.powerWaitingToRepeat);
  configManager.addParameter("maximumOfSendingpower", &config.maximumOfSendingpower);

  configManager.addParameter("doorSensorEnable", &config.doorSensorEnable);
  configManager.addParameter("runMode", &config.runMode);
  configManager.addParameter("resetStatusAtBoot", &config.resetStatusAtBoot);
  configManager.addParameter("sendConfigMqtt", &config.sendConfigMqtt);
  configManager.addParameter("firstTime", &config.firstTime);
  configManager.addParameter("sendReportStatus", &config.sendReportStatus);

  configManager.addParameter("reportClock", &config.reportClock);
  configManager.addParameter("lastDay", &config.lastDay);

  configManager.addParameter("powerLostWarningEnable", &config.powerLostWarningEnable);
  configManager.addParameter("waitingTimetoSendPowerLost", &config.waitingTimetoSendPowerLost);
  configManager.addParameter("powerRecoverEnable", &config.powerRecoverEnable);

  configManager.addParameter("sensorErrorWarningEnable", &config.sensorErrorWarningEnable);
  configManager.addParameter("waitingTimetoSendSensorError", &config.waitingTimetoSendSensorError);

  configManager.addParameter("hieuChinhCamBien_config", config.hieuChinhCamBien_config, 20);

  configManager.addParameter("dailySMSEnable", &config.dailySMSEnable);
  configManager.addParameter("relayOutputEnable", &config.relayOutputEnable);

  // Cài đặtf wifi thứ 2
  configManager.addParameter("second_wifi_name", config.second_wifi_name, 100);
  configManager.addParameter("second_wifi_pass", config.second_wifi_pass, 20);

  configManager.addParameter("wifi_name", config.wifi_name, 100);
  configManager.addParameter("wifi_pass", config.wifi_pass, 20);

  configManager.addParameter("mainWifi", &config.mainWifi);

  configManager.addParameter("soGoiDien", config.soGoiDien, 150);
  configManager.addParameter("soNhanTin", config.soNhanTin, 150);
  configManager.addParameter("soNhanTinHangNgay", config.soNhanTinHangNgay, 75);
  configManager.addParameter("choPhepSensorCanhBao_config", config.choPhepSensorCanhBao_config, 20);
  configManager.addParameter("trangthaiNhantinTheoTuan", &config.trangthaiNhantinTheoTuan);
  configManager.addParameter("volteStatus", &config.volteStatus);
  configManager.addParameter("donVi_hienThi_config", config.donVi_hienThi_config, 20);
  // Init Callbacks
  configManager.setAPCallback(APCallback);
  configManager.setAPICallback(APICallback);
  configManager.begin(config, loadDataOnly);
}

void keepAlive()
{
beginning:
  if (checkDeviceMode("SMS_ONLY") | checkDeviceMode("SMS_WIFI"))
  {
    SIMOFF();
  }
  // else if (checkDeviceMode("WIFI_ONLY"))
  // {
  //   SerialMon.printf("-------------------WIFI_ONLY-----------------\r\n");
  //   oledInforDisplay("Mode", "WIFI_ONLY", "---", 500);
  // }
  // else
  // {
  //   SerialMon.printf("-------------------Not SMS mode-----------------\r\n");
  //   oledInforDisplay("Mode", "Not SMS mode", "---", 500);
  // }

  if (checkDeviceMode("SMS_WIFI") || checkDeviceMode("WIFI_ONLY"))
  {
    mqttSetupParameter();
  }
  unsigned long now1 = millis(), now2 = millis(), now3 = millis(), timerTemp = 0, timerSMS = 0, now4 = millis();
  unsigned long now8 = millis(), now9 = millis();

  float temp = 0;

  long timeToCheckWifi = 1000;
  long timeToCheckMQTTConnected = 5000;
  unsigned long nowPowerStateCheck = millis();
  unsigned long nowBatteryWakeUp = millis();
  unsigned long nowPrintLocalTime = millis();
  unsigned long now20 = millis(), now21 = millis();

  unsigned long checkErrorSensorInterval = millis();

  bool powerState = !digitalRead(powerDetectPin);
  bool lastPowerState = powerState;
  unsigned long startToCountPowerLost;
  bool sentPowerlostWaringStatus;
  bool recoverPowerLostStatus = false;

  unsigned long nowOfflineLog = millis();
  int numberOfStartToLog = 0;
  bool saveLogFlag = false;
  int numberOfStartToGetTimeFromModem = 0;

  if (digitalRead(powerDetectPin) == HIGH)
  {
    sentPowerlostWaringStatus = false;
    startToCountPowerLost = millis();
    SerialMon.println("Trạng thái nguồn: Mất điện");
  }
  else
  {
    SerialMon.println("Trạng thái nguồn: Có điện");
    sentPowerlostWaringStatus = true;
  }
  int timeOfSendSmsReportDaily = 0;
  bool m0 = false, m1 = false;

  int numberOfWifiLost = 0;

  // Lấy giá trị từ configManager và lưu vào biến toàn cục
  const char *mainSsid = config.wifi_name;
  const char *mainPassword = config.wifi_pass;

  int SWITCH_WIFI_INTERVAL = 300; // Thời gian chuyển WiFi sau 300 giây
  if (testEsp32)
    SWITCH_WIFI_INTERVAL = 10;

  const int REBOOT_INTERVAL = 3600;

  bool trangThaiSensorLoi_sub = false;
  bool FlagTinhGioSensorLoi = false;
  unsigned long Moc_thoigian_SSLoi = 0;
  bool Flag_daCanhBao_Cambien_loi = false;

  while (true)
  {
    unsigned long overFlowMillisHandle = millis();
    esp_task_wdt_reset();
  loopAgain:

    if (version == 1 || version == 2)
    {
      unsigned long timeOfHoldButoon = millis();
      unsigned long delta = 0;
      while (digitalRead(enable_button) == LOW)
      {
        delta = millis() - timeOfHoldButoon;
      }

      if (delta <= 2000 && delta > 1000)
      {
        SerialMon.println("Bạn vừa mới bấm để kích hoạt lệnh Restart ESP32");
        delayWithWatchdog(1000);
        ESP.restart();
      }

      if (delta > 2000)
      {
        timeOfHoldButoon = millis();

        config.runMode = !config.runMode;
        configManager.save();
        SerialMon.printf("Đổi trạng thái dừng / chạy, bây giờ trạng thái là: %d\n", config.runMode);
      }
    }
    if (version == 3)
    {
      if (lastSwitchState != digitalRead(switch_record))
      {
        lastSwitchState = digitalRead(switch_record);
        if (digitalRead(switch_record) == LOW)
        {
          config.runMode = true;
          digitalWrite(led, LOW);
        }
        else
        {
          config.runMode = false;
          digitalWrite(led, HIGH);
        }
        configManager.save();
        SerialMon.printf("Đổi trạng thái công tắc gạt, chế độ chạy bây giờ là %d\n", config.runMode);
      }
    }
    // if (millis() - now20 > 3600000L)
    // {
    //   now20 = millis();
    //   appendError("ESP32 is running .. OK");
    // }

    // Set time for SIM800L
    if (setSim800lTimeFlag)
    {
      if (!testEsp32)
      {
        SerialMon.println("Đặt giờ cho SIM800L");
        setSim800lTimeFlag = false;
        setSim800lTime();
      }
    }
    // Check offlineLog
    if (millis() - nowOfflineLog > 1000)
    {
      nowOfflineLog = millis();

      // Check to settime from Modem
      if (++numberOfStartToGetTimeFromModem >= 2 && !getLocalTime(&timeinfo))
      {
        if (!m1)
        {
          m1 = true;
          setTimeFromModem();
        }
      }
      // ghi dữ liệu offline
      if (!mqtt->connected() || WiFi.status() != WL_CONNECTED)
      {

        if (numberOfStartToLog++ > 30)
        {
          if (getLocalTime(&timeinfo))
          {
            // Chuyển đổi timeinfo thành chuỗi
            char buffer[80];
            int bien_phu = 0;
            if (testEsp32)
            {
              bien_phu = timeinfo.tm_sec;
              strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
            }
            else
            {
              strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:00", &timeinfo);
              bien_phu = timeinfo.tm_min;
            }

            if (bien_phu % 5 == 0)
            {
              if (!saveLogFlag)
              {
                SerialMon.println("Lưu 1 điểm vào offline Log");
                saveLogFlag = true;
                for (int i = 0; i < SL_CAM_BIEN; i++)
                {
                  char b[100];
                  sprintf(b, "{\"DeviceRowID\":\"%d\",\"Temperature\":\"%.1f\",\"Date\":\"%s\"}",
                          IdxArray[i], giaTriCamBien[i],
                          buffer);
                  SerialMon.println(b);
                  appendLog(b);
                }
              }
            }
            else
              saveLogFlag = false;
          }
          else
          {
            if (now21 - millis() > 300000L)
            {
              now21 = millis();

              for (int i = 0; i < SL_CAM_BIEN; i++)
              {
                char b[100];
                sprintf(b, "{\"DeviceRowID\":\"%d\",\"Temperature\":\"%.1f\",\"Date\":\"0\"}",
                        IdxArray[i], giaTriCamBien[i]);
                SerialMon.println(b);
                appendLog(b);
              }
            }

            if (!m0)
            {
              m0 = true;
              appendError("Check time to add to offlineLog .. NG");
            }
          }
        }
      }
    }
    // Check edge of powerState
    if (lastPowerState != powerState)
    {
      lastPowerState = powerState;
      if (powerState)
      {
        addToLog("POWER AC");
        SerialMon.println("Đã đổi trạng thái nguồn thành nguồn AC");
        if (recoverPowerLostStatus && config.powerRecoverEnable)
        {
          SerialMon.printf("Power recover \n");
          tempToSendSMS = readTemp(0);
          addToLog("POWER AC BACK ALARM");
          char noiDungTinNhan_nhap[300];
          snprintf(noiDungTinNhan_nhap, sizeof(noiDungTinNhan_nhap), "DA CO DIEN TRO LAI \nTen: %s idx:%s \n%s",
                   config.device_name,
                   config.device_idx,
                   generateRandomString().c_str());
          AlarmNew(noiDungTinNhan_nhap);
        }
        //     WifiBluetoothOn();
      }
      else
      {
        addToLog("POWER BAT");
        SerialMon.printf("Đã đổi trạng thái nguồn thành nguồn PIN\n");
        startToCountPowerLost = millis();
        SerialMon.printf("Power lost, start to count the time: %d\n", startToCountPowerLost);
        // WifiBluetoothOff();
      }
    }

    // Overflow Millis() handling
    if (millis() < overFlowMillisHandle)
      ESP.restart();

    if (!powerState)
    {
      int rd = random(0, 100);
      if (config.powerLostWarningEnable && (millis() - startToCountPowerLost > (config.waitingTimetoSendPowerLost + rd) * 1000L) && !sentPowerlostWaringStatus)
      {
        SerialMon.printf("Gui tin nhanh canh bao mat dien \n");
        // WifiBluetoothOff();
        tempToSendSMS = readTemp(0);
        addToLog("POWER LOST ALARM");
        char noiDungTinNhan_nhap[300];
        snprintf(noiDungTinNhan_nhap, sizeof(noiDungTinNhan_nhap), "CANH BAO MAT DIEN \nTen: %s idx:%s\n %s",
                 config.device_name,
                 config.device_idx,
                 generateRandomString().c_str());

        AlarmNew(noiDungTinNhan_nhap);
        SIMOFF();
        //  WifiBluetoothOn();
        recoverPowerLostStatus = true;
        sentPowerlostWaringStatus = true;
      }
    }
    else
    {
      startToCountPowerLost = millis();
      sentPowerlostWaringStatus = false;
    }
    // Check powerState
    if (millis() - nowPowerStateCheck > 500)
    {
      nowPowerStateCheck = millis();
      // SerialMon.println("__3");
      powerState = !digitalRead(powerDetectPin);
      if (powerState)
      {
        display.setBrightness(255);
      }
      else
      {
        display.setBrightness(20);
      }
    }
    // Read sensor Display to OLED
    if (millis() - now8 > 1000)
    {
      now8 = millis();

      docGiaTriCamBien();

      if (checkDeviceMode("SMS_WIFI") || checkDeviceMode("WIFI_ONLY"))
      {

        if (mqtt->connected())
        {

          oledTempDisplay(String("(M) ") + String((digitalRead(powerDetectPin) ? " IDX:" : "(+) IDX:") + String(config.device_idx)),
                          giaTriCamBien, "", config.runMode, true);
        }
        else
        {

          oledTempDisplay(String("( ) ") + String((digitalRead(powerDetectPin) ? " IDX:" : "(+) IDX:") + String(config.device_idx)),
                          giaTriCamBien, "", config.runMode, true);
        }
      }
      else
      {
        // SerialMon.println("__45");
        oledTempDisplay(String("( ) ") + String((digitalRead(powerDetectPin) ? " IDX:" : "(+) IDX:") + String(config.device_idx)),
                        giaTriCamBien, "", config.runMode, true);
        // SerialMon.println("__46");
      }
    }

    // Check sensor Error
    if (millis() - checkErrorSensorInterval > 1000L)
    {

      checkErrorSensorInterval = millis();
      trangThaiSensorLoi_sub = false;
      for (int i = 0; i < SL_CAM_BIEN; i++)
      {
        if (giaTriCamBien[i] <= -900)
        {
          trangThaiSensorLoi_sub = true;
        }
      }
      if (trangThaiSensorLoi_sub && config.runMode && config.sensorErrorWarningEnable)
      {
        if (!FlagTinhGioSensorLoi)
        {
          FlagTinhGioSensorLoi = true;
          Moc_thoigian_SSLoi = millis();
        }
        else
        {
          if (millis() - Moc_thoigian_SSLoi > config.waitingTimetoSendSensorError * 1000L && !Flag_daCanhBao_Cambien_loi)
          {
            SerialMon.println("Cảm biến bị lỗi !");
            Flag_daCanhBao_Cambien_loi = true;
            char noiDungTinNhan_nhap[300];
            snprintf(noiDungTinNhan_nhap, sizeof(noiDungTinNhan_nhap), "LOI CAM BIEN \nTen: %s idx:%s\n%s",
                     config.device_name,
                     config.device_idx,
                     generateRandomString().c_str());
            AlarmNew(noiDungTinNhan_nhap);
          }
        }
      }
      else
      {
        FlagTinhGioSensorLoi = false;
        Flag_daCanhBao_Cambien_loi = false;
      }
    }

    // Đoạn chương trình test cảnh báo cuộc gọi và tin nhắn
    if (millis() - now3 > 2000L)
    {
      now3 = millis();

      bool flag_tam[SL_CAM_BIEN];
      for (int i = 0; i < SL_CAM_BIEN; i++)
      {
        flag_tam[i] = true;
      }

      for (int i = 0; i < SL_CAM_BIEN; i++)
      {
        if (giaTriCamBien[i] > -900 && (giaTriCamBien[i] > giaTriToiDa[i] || giaTriCamBien[i] < giaTriToiThieu[i]))
        {
          trangThaiCanhBao[i] = true;
          if (trangThaiCanhBao[i] != trangThaiCanhBaoLanTruoc[i])
          {
            trangThaiCanhBaoLanTruoc[i] = trangThaiCanhBao[i];
            currentMillis[i] = millis();
          }
          T[i] = millis() - currentMillis[i];
        }
        else
        {
          trangThaiCanhBao[i] = false;
          if (trangThaiCanhBao[i] != trangThaiCanhBaoLanTruoc[i])
          {
            trangThaiCanhBaoLanTruoc[i] = trangThaiCanhBao[i];
          }
          T[i] = 0;
          soTinNhanDaGuiHienTai[i] = 0;
          flag_tam[i] = false;
        }

        SerialMon.printf("%.1f(%.1f~%.1f) - %d    ", giaTriCamBien[i], giaTriToiThieu[i], giaTriToiDa[i], T[i] / 1000);

        if (soTinNhanDaGuiHienTai[i] == 0 && T[i] >= thoiGianChoCanhBao[i] * 1000L)
        {
          SerialMon.printf("Cảm biến (%d) gửi cảnh báo lần 0\n", i);

          activeRelayOutput(true);
          char noiDungTinNhan_nhap[300];
          snprintf(noiDungTinNhan_nhap, sizeof(noiDungTinNhan_nhap), "%s\nCANH BAO!\nTen:%s idx:%d:%d\nGia tri: %.1f \nMinmax:%.1f den %.1f\nNguon: %d",
                   generateRandomString().c_str(),
                   config.device_name,
                   IdxArray[i],
                   i,
                   giaTriCamBien[i],
                   giaTriToiThieu[i],
                   giaTriToiDa[i],
                   !digitalRead(powerDetectPin));
          if (config.runMode && choPhepSensorCanhBao[i] == 1)
            AlarmNew(noiDungTinNhan_nhap);
          soTinNhanDaGuiHienTai[i]++;
          saving_T[i] = T[i];
        }
        if (soTinNhanDaGuiHienTai[i] >= 1 &&
            soTinNhanDaGuiHienTai[i] < soTinNhanToiDa[i] &&
            T[i] > saving_T[i] + soTinNhanDaGuiHienTai[i] * thoiGianChoLapLai_CanhBao[i] * 1000L)
        {
          saving_T[i] = T[i];

          SerialMon.printf("Cảm biến (%d) báo lần (%d)\n", i, soTinNhanDaGuiHienTai[i]++);

          activeRelayOutput(true);
          char noiDungTinNhan_nhap[300];
          snprintf(noiDungTinNhan_nhap, sizeof(noiDungTinNhan_nhap), "%s\nCANH BAO\nTen:%s idx:%d : %d\nGia tri: %.1f \n Minmax: %.1f~%.1f \nNguon: %d",
                   generateRandomString().c_str(),
                   config.device_name,
                   IdxArray[i],
                   i,
                   giaTriCamBien[i],
                   giaTriToiThieu[i],
                   giaTriToiDa[i],
                   !digitalRead(powerDetectPin));
          if (choPhepSensorCanhBao[i] == 1)
            AlarmNew(noiDungTinNhan_nhap);
        }
      }
      bool KiemTraCac_Flag = false;
      for (int i = 0; i < SL_CAM_BIEN; i++)
      {
        if (flag_tam[i] == true)
        {
          KiemTraCac_Flag = true;
          break;
        }
      }
      if (!KiemTraCac_Flag)
      {
        activeRelayOutput(false);
      };

      SerialMon.println();
    }

    // tick RTC - Send report daily
    if (millis() - nowPrintLocalTime > 60000L)
    {
      nowPrintLocalTime = millis();

      if (config.runMode)
        nhanSMS_hangTuan();

      if (!getLocalTime(&timeinfo))
      {
        timeIsSet = false;
      }
      else
      {
        timeIsSet = true;
        // SerialMon.printf("Today: %d Lastday: %d Clock: %d, ClockReport: %d, Trạng thái nhắn tin chưa: %d \n",
        //                  timeinfo.tm_mday, config.lastDay, timeinfo.tm_hour,
        //                  config.reportClock, config.sendReportStatus);
        if (timeinfo.tm_mday != config.lastDay)
        {
          config.lastDay = timeinfo.tm_mday;
          config.sendReportStatus = false;
          configManager.save();
          timeOfSendSmsReportDaily = 0;
          SerialMon.println("Đã bước sang ngày mới !");
        }
      }

      if (!config.sendReportStatus && timeinfo.tm_hour == config.reportClock && timeIsSet)
      {
        if (config.runMode && config.dailySMSEnable)
        {
          if (timeOfSendSmsReportDaily <= 3 && dailyReportSMS(config.soNhanTinHangNgay))
          {
            SerialMon.println("Bắt đầu gửi Daily SMS");
            config.sendReportStatus = true;
            LogMQTTContent = "Daily SMS ..OK ";
            appendError("Daily SMS .. OK");
            addToLog("Daily SMS .. OK");
            configManager.save();
          }
          else
          {
            timeOfSendSmsReportDaily++;
            if (timeOfSendSmsReportDaily <= 3)
            {
              addToLog("Daily SMS .. NG: " + String(timeOfSendSmsReportDaily));
            }
            if (timeOfSendSmsReportDaily == 3)
            {
              LogMQTTContent = "Daily SMS ..NG ";
              appendError("Daily SMS .. NG: ");
            }
          }
        }
      }
    }

    //-----DICH VU MANG -----------------
    if (powerState)
    {
      if ((checkDeviceMode("SMS_WIFI") || checkDeviceMode("WIFI_ONLY")))
      {
        mqtt->loop();
        //-- kiểm tra có kết nối wifi không, với trường hợp dùng wifi
        if (millis() - now9 > timeToCheckWifi && wifi_using)
        {
          now9 = millis();
          //-- Check wifi connected
          if (WiFi.status() != WL_CONNECTED)
          {
            SerialMon.print("Số lần mất wifi: ");
            SerialMon.println(numberOfWifiLost++);
            if (numberOfWifiLost % SWITCH_WIFI_INTERVAL == 0)
            { // Mỗi 10 giây
              if (config.mainWifi && strlen(config.second_wifi_name) > 0)
              {
                config.mainWifi = false;
                // configManager.save();
                // appendError("Mất kết nối, chuyển wifi PHỤ bây giờ");
                SerialMon.printf("Mất kết nối quá %d giây, chuyển wifi phụ bây giờ\r\n", numberOfWifiLost);
                SerialMon.println(config.second_wifi_name);
                SerialMon.println(config.second_wifi_pass);
                WiFi.begin(config.second_wifi_name, config.second_wifi_pass);
              }
              else if (!config.mainWifi && strlen(mainSsid) > 0)
              {
                config.mainWifi = true;
                configManager.save();
                appendError("Mất kết nối, chuyển wifi CHÍNH bây giờ");
                SerialMon.printf("Mất kết nối quá %d giây, chuyển wifi chính bây giờ\r\n", numberOfWifiLost);
                SerialMon.println(mainSsid);
                SerialMon.println(mainPassword);
                WiFi.begin(mainSsid, mainPassword);
              }
            }

            if (numberOfWifiLost >= REBOOT_INTERVAL)
            { // Quá 3600 giây
              appendError("Không thể kết nối WiFi trong 3600 giây, khởi động lại!");
              SerialMon.println("Không thể kết nối WiFi trong 3600 giây, khởi động lại!");
              ESP.restart();
            }
          }
          else
          {
            numberOfWifiLost = 0;
          }
        }

        // Check MQTT connect or not
        if (millis() - now2 > timeToCheckMQTTConnected)
        {
          now2 = millis();
          if (WiFi.status() != WL_CONNECTED && wifi_using)
          {
            SerialMon.println("------Mất kết nối WiFi");
            goto loopAgain;
          }

          if (!mqtt->connected())
          {
            SerialMon.println("------Mất kết nối MQTT");
            oledInforDisplay("status", "MQTT.." + String(mqttConnect() ? "OK" : "fail"), "", 500);
          }
        }
        // Send mqttLogCOntent
        if (LogMQTTContent.length() >= 1)
        {
          if (mqttLogUpdate(logIdx, LogMQTTContent))
            LogMQTTContent = "";
        }

        if (LogMQTTContent1.length() >= 1)
        {
          if (mqttLogUpdate("91", LogMQTTContent1))
            LogMQTTContent1 = "";
        }

        // Send data to server
        if ((millis() - now1 > config.MQTT_update_interval * 1000L))
        {
          if (config.MQTT_update_interval < 5)
          {
            config.MQTT_update_interval = 5;
            configManager.save();
          }

          now1 = millis();
          if (mqtt->connected())
          {
            if (config.sendConfigMqtt)
            {
              mqttSendConfig();
              config.sendConfigMqtt = false;
              configManager.save();
            }

            oledTempDisplay(String("(M) ") + String(" ") + String(35) + String(digitalRead(powerDetectPin) ? "" : "(+) IDX:" + String(config.device_idx)),
                            giaTriCamBien, "->", config.runMode, true);

            for (int i = 0; i < SL_CAM_BIEN;)
            {

              char message[256];

              if (i < SL_CAM_BIEN - 1 && IdxArray[i] == IdxArray[i + 1])
              {
                // Cảm biến nhiệt độ và độ ẩm
                sprintf(message, "{ \"idx\" : %d, \"name\" : \"%s_%d\", \"nvalue\" : 0, \"svalue\" : \"%.1f;%.1f;1\", \"Battery\" : %d, \"RSSI\" : %d}",
                        IdxArray[i],
                        config.device_name,
                        IdxArray[i],
                        giaTriCamBien[i],
                        giaTriCamBien[i + 1],
                        digitalRead(powerDetectPin) ? 0 : 100,
                        map(WiFi.RSSI(), RSSI_MIN, RSSI_MAX, 1, 11));

                i += 2; // Tăng i để bỏ qua cảm biến độ ẩm đã xử lý
              }
              else
              {
                // Cảm biến nhiệt độ thông thường
                sprintf(message, "{ \"idx\" : %d, \"name\" : \"%s_%d\", \"nvalue\" : 0, \"svalue\" : \"%.1f\", \"Battery\" : %d, \"RSSI\" : %d}",
                        IdxArray[i],
                        config.device_name,
                        IdxArray[i],
                        giaTriCamBien[i],
                        digitalRead(powerDetectPin) ? 0 : 100,
                        map(WiFi.RSSI(), RSSI_MIN, RSSI_MAX, 1, 11));

                i++; // Chỉ tăng i khi xử lý cảm biến thông thường
              }

              if (mqtt->publish(config.SERVER_TOPIC_RECEIVE_DATA, message))
              {
                SerialMon.print("->OK  ");
              }
              else
              {
                SerialMon.print("->fail  ");
              }

              delayWithWatchdog(1000);
            }

            SerialMon.println();

            oledTempDisplay(String("(M) ") + String(digitalRead(powerDetectPin) ? "" : "(+) IDX:" + String(config.device_idx)),
                            giaTriCamBien, ">ok", config.runMode, true);
          }
          else
          {

            goto loopAgain;
          }
        }
      }
    }
  }
}
void setup()
{
  // Chọn MQTT Client dựa trên wifi_using
  mqtt = wifi_using ? &mqttWifi : &mqtt4G;

  SerialAT.begin(AT_BAUDRATE);
  SerialMon.begin(115200);
  delay(1000);
  SIMOFF();
  Serial.print("Đang khởi tạo SPIFFS...");
  if (!SPIFFS.begin())
  {
    Serial.println("Khởi tạo không thành công bộ nhớ SPIFFS, đang định dạng...");
    if (SPIFFS.format())
    {
      Serial.println("Đã định dạng hệ thống tệp SPIFFS");
      ESP.restart();
    }
    else
    {
      Serial.println("Không thể định dạng hệ thống tệp SPIFFS");
    }
  }
  Serial.println("OK ");
  Wire.begin();
  esp_wifi_set_max_tx_power(84);

  String macString = WiFi.macAddress();
  String targetMacString = ""; // "B0:B2:1C:97:86:8C";
  testEsp32 = (macString == targetMacString);
  SerialMon.print("Kiểm tra kiểu module test ...");
  SerialMon.println(testEsp32 ? "OK" : "NG");

  Serial.print("Đang cấu hình watchDog...");
  esp_task_wdt_init(300, true); // enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);       // add current thread to WDT watch
  SerialMon.println("OK");

  // Clock
  sntp_set_time_sync_notification_cb(timeavailable);
  sntp_servermode_dhcp(1);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);

  version = hardwareVersion();

  if (version == 1)
  {
    powerDetectPin = 27;
    enable_button = 26;
    relayOutputPin = 15;
  }
  if (version == 2)
  {
    powerDetectPin = 39;
    enable_button = 19;
    relayOutputPin = 23;
  }
  if (version == 3)
  {
    powerDetectPin = 39;
    enable_button = 2;
    led = 4;
    switch_record = 19;
    pinMode(switch_record, INPUT_PULLUP);
    lastSwitchState = digitalRead(switch_record);
    relayOutputPin = 23;
  }

  pinMode(boot_pin, INPUT_PULLUP);
  pinMode(enable_button, INPUT_PULLUP);

  pinMode(DoorSensor, INPUT_PULLUP);
  pinMode(powerDetectPin, INPUT_PULLUP);
  pinMode(relayOutputPin, OUTPUT);
  oledSetup();
  paraSetup();

  // SerialMon.println(readAndPrintSms(10));

  strncpy(config.broker, "theodoi.setcom.com.vn", 30);
  config.brokerPort = 1883;
  strncpy(config.mqttUser, "", 10);
  strncpy(config.mqttPass, "", 10);
  strncpy(config.SERVER_TOPIC_RECEIVE_DATA, "domoticz/in", 20);

  // strncpy(config.device_idx, "152;155;155", 30);
  // strncpy(config.wifi_name, "Mangcambiennhiet", 100);
  // strncpy(config.wifi_pass, "cnttpshn", 20);

  configManager.save();

  //******************************
  // WiFi.mode(WIFI_STA);

  //******************************

  oledInforDisplay("--", PROJECT_NAME, "", 1000);
  oledInforDisplay(__DATE__, __TIME__, config.device_idx, 1000);
  SerialMon.printf("SMART CONFIG... \r\n");
  oledInforDisplay("Menu cai dat", ">>SMART CONFIG..", "", 0);
  if (checkKeyPressed())
  {
    config.mainWifi = true;
    configManager.save();
    smartConfig();
  }

  SL_CAM_BIEN = demPhanTu(config.device_idx);
  SerialMon.print("Số phần tử IDX: ");
  SerialMon.println(SL_CAM_BIEN);

  SerialMon.println(config.device_idx);
  // Cấp phát bộ nhớ cho các mảng dựa trên giá trị của SL_CAM_BIEN
  hieuChinhCamBien = new float[SL_CAM_BIEN];
  thoiGianChoCanhBao = new int[SL_CAM_BIEN];
  IdxArray = new int[SL_CAM_BIEN];
  thoiGianChoLapLai_CanhBao = new int[SL_CAM_BIEN];
  soTinNhanToiDa = new int[SL_CAM_BIEN];
  T = new unsigned long[SL_CAM_BIEN];
  saving_T = new unsigned long[SL_CAM_BIEN];
  giaTriToiThieu = new float[SL_CAM_BIEN];
  giaTriToiDa = new float[SL_CAM_BIEN];
  trangThaiCanhBao = new bool[SL_CAM_BIEN];
  trangThaiCanhBaoLanTruoc = new bool[SL_CAM_BIEN];
  currentMillis = new unsigned long[SL_CAM_BIEN];
  giaTriCamBien = new float[SL_CAM_BIEN];
  soTinNhanDaGuiHienTai = new int[SL_CAM_BIEN];
  choPhepSensorCanhBao = new int[SL_CAM_BIEN];
  donVi_hienThi = new char *[SL_CAM_BIEN];
  soGoiDienArray = new char *[SL_SDT_GOI];
  for (int i = 0; i < SL_SDT_GOI; ++i)
  {
    soGoiDienArray[i] = new char[doDaiToiDa_SDT];
  }

  soNhanTinArray = new char *[SL_SDT_NhanTin];
  for (int i = 0; i < SL_SDT_NhanTin; ++i)
  {
    soNhanTinArray[i] = new char[doDaiToiDa_SDT];
  }

  soNhanTinHangNgayArray = new char *[SL_SDT_tinNhanHangNgay];
  for (int i = 0; i < SL_SDT_tinNhanHangNgay; ++i)
  {
    soNhanTinHangNgayArray[i] = new char[doDaiToiDa_SDT];
  }
  // Điền giá trị
  fillArrayFromString(giaTriToiThieu, config.giaTriToiThieu_config, 0);
  fillArrayFromString(giaTriToiDa, config.giaTriToiDa_config, 30);
  fillArrayFromString_INT(thoiGianChoCanhBao, config.thoiGianChoCanhBao_config, 300);
  fillArrayFromString_INT(thoiGianChoLapLai_CanhBao, config.thoiGianChoLapLai_CanhBao_config, 300);
  fillArrayFromString_INT(soTinNhanToiDa, config.soTinNhanToiDa_config, 300);
  fillArrayFromString(hieuChinhCamBien, config.hieuChinhCamBien_config, 0);
  fillArrayFromString_INT(IdxArray, config.device_idx, 1);
  fillArrayFromString_INT(choPhepSensorCanhBao, config.choPhepSensorCanhBao_config, 1);
  for (int i = 0; i < SL_CAM_BIEN; i++)
  {
    T[i] = 0;
    saving_T[i] = 0;
    trangThaiCanhBao[i] = false;
    trangThaiCanhBaoLanTruoc[i] = false;
    currentMillis[i] = 0;
    soTinNhanDaGuiHienTai[i] = 0;
  }

  tachSoDienThoai(config.soGoiDien, soGoiDienArray);
  tachSoDienThoai(config.soNhanTin, soNhanTinArray);
  tachSoDienThoai(config.soNhanTinHangNgay, soNhanTinHangNgayArray);
  tachChuoiCon(config.donVi_hienThi_config, donVi_hienThi);

  // Kết nối đến wifi chính phụ
  if (wifi_using && !connectToWiFi(config.mainWifi) )
  {
    delay(1000);
    config.mainWifi = !config.mainWifi;
    configManager.save();

    if (!connectToWiFi(config.mainWifi))
    {
      SerialMon.println("Không kết nối được đến wifi nào ...");
    }
  }

  if (version == 3)
  {
    pinMode(led, OUTPUT);
    if (config.runMode)
      digitalWrite(led, LOW);
    else
      digitalWrite(led, HIGH);
  }

  if (!config.firstTime)
  {
    loadParaDefault();
  }

  SerialMon.println(WiFi.localIP().toString().c_str());
  oledInforDisplay("wifi: " + String(WiFi.SSID()), WiFi.localIP().toString().c_str(), "setcom.com.vn", 2000);

  if (WiFi.status() == WL_CONNECTED)
  {
    SerialMon.print("Kiểm tra xem đã có file SPIFSS trong bộ nhớ chưa ...");
    downloadAllDataFile();
  }

  if (config.runMode > 80)
  {
    config.runMode = true;
    configManager.save();
  }

  if (atoi(config.device_idx) == 1)
  {
    SerialMon.println("IDX là 1");
    config.runMode = true;
    config.MQTT_update_interval = 5;
    strncpy(config.broker, "theodoi.setcom.com.vn", 30);

    configManager.save();
  }

  SerialMon.printf("WIFI... \r\n");
  oledInforDisplay("Menu cai dat", ">>KET NOI WIFI", "", 0);

  if (checkKeyPressed())
  {
    config.mainWifi = true;
    configManager.save();
    SerialMon.println("Wifi: CauHinhSMS, http://192.168.1.1/");
    oledInforDisplay("Ket noi den wifi", "CauHinhSMS", "http://192.168.1.1/", 2000);
    runWifiWebServer();
    // configManager.startAP();
    // configManager.startAPApi();
    // while (true)
    // {
    //   esp_task_wdt_reset();
    //   configManager.loop();
    // }
  }

  SerialMon.printf("CAI DAT...\r\n");
  oledInforDisplay("Menu cai dat", ">>CAI DAT", "", 0);
  if (checkKeyPressed())
  {
    String sbuf = WiFi.localIP().toString().c_str();
    if (sbuf.indexOf("0.0.0.0") >= 0)
    {
      oledInforDisplay("Da co loi (error)", "Chua co WIFI, khoi dong lai", "", 2000);
      ESP.restart();
    }
    else
    {
      SerialMon.println("http://" + sbuf + "/caidat.html");
      oledInforDisplay("IP: " + sbuf, sbuf + "/caidat.html", "", 2000);
    }
    configManager.startApi();
    while (true)
    {
      esp_task_wdt_reset();
      configManager.loop();
    }
  }

  SerialMon.printf("CHECK SIM...\n");
  oledInforDisplay("Menu cai dat", ">>CHECK SIM", "", 0);
  if (checkKeyPressed())
  {
    if (!checkATSimNet())
    {
      ESP.restart();
    }
  }

  SerialMon.printf("DEFAULT...\r\n");
  oledInforDisplay("Menu cai dat", ">>DEFAULT", "", 0);
  if (checkKeyPressed(1000))
  {
    loadParaDefault();
  }

  SerialMon.printf("UPDATE ...\r\n");
  oledInforDisplay("Menu cai dat", ">>UPDATE...", "", 0);
  if (checkKeyPressed(1000))
  {
    if (!wifi_using)
    {
      SerialMon.println("Huỷ bỏ cập nhật do đang kết nối 4G");
      oledInforDisplay("Cap nhat phan mem", "update(4G) cancelled)", "setcom.com.vn", 1000);
      return;
    }
    else
    {
      long long now100 = millis();
      downloadAllDataFile(true);
      while (true)
      {
        esp_task_wdt_reset();
        httpUpdateFirmware();
        if (millis() - now100 > 600000L)
          break;
      }
    }
  }

  oledInforDisplay("Menu cai dat", "Ver.." + String(version), "", 1000);

  btStop();

  keepAlive();
}
void loop()
{
}
