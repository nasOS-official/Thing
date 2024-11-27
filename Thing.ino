/*
  #############################################################################
  #  _________    ___   ___      __________      ___   ___       _______      #
  # /________/\  /__/\ /__/\    /_________/\    /__/\ /__/\     /______/\     # 
  # \__    __\/  \  \ \\  \ \   \___    __\/    \  \_\\  \ \    \    __\/__   # 
  #    \  \ \     \  \/_\  \ \      \  \ \       \   `-\  \ \    \ \ /____/\  # 
  #     \  \ \     \   ___  \ \     _\  \ \__     \   _    \ \    \ \\_  _\/  # 
  #      \  \ \     \  \ \\  \ \   /__\  \__/\     \  \`-\  \ \    \ \_\ \ \  # 
  #       \__\/      \__\/ \__\/   \________\/      \__\/ \__\/     \_____\/  # 
  #                                                                           #
  #                   https://github.com/ZernovTechno/Thing                   #
  #                                                                           #
  #############################################################################
                                                    ___     ___    ___    _  _   
                                                   |__ \   / _ \  |__ \  | || |  
  ____   ___   _ __   _ __     ___   __   __          ) | | | | |    ) | | || |_ 
 |_  /  / _ \ | '__| | '_ \   / _ \  \ \ / /         / /  | | | |   / /  |__   _|
  / /  |  __/ | |    | | | | | (_) |  \ V /   _     / /_  | |_| |  / /_     | |  
 /___|  \___| |_|    |_| |_|  \___/    \_/   (_)   |____|  \___/  |____|    |_|  
     https://www.youtube.com/@zernovtech

  Мультифункциональная прошивка для ESP32. Включает основные дистанционные протоколы обмена данными. 
  RFID, NFC, WiFi, 433Mhz, ИК.

  Прошивка начата 30.10.2024 в 14:42:57.

*/

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRutils.h>
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "FS.h"
#include <LittleFS.h>
#define FORMAT_LITTLEFS_IF_FAILED true
#include "foxy1.h"
const uint16_t * Foxy[] = {foxy1,foxy2,foxy3}; // Массив ссылок на лису клубочком

#include "FoxyOnStart1.h"
const uint16_t * FoxyOnStart[] = {FoxyOnStart1,FoxyOnStart2,FoxyOnStart3,FoxyOnStart4,FoxyOnStart5}; // Массив ссылок на лису в шляпе

//Таймер для Millis (Анимация лисички в баре)
uint32_t timer = 0;
//Счетчик для лисы
int FoxyCounter = 1;
//Частота связи по UART
String SerialFrq = "115200";
//Текущие дата и время
String DateTime = "Unknown datetime";
//Режим отладки (вывод инфо в UART)
bool DebugMode = true;
bool CoolDown;

const int IRReceiverPin = 27;
const int IRSenderPin = 16;

const char* ssid = "";
const char* password =  "";

// Объект дисплея 
TFT_eSPI tft = TFT_eSPI();

AsyncWebServer server(80);

// IR transmitter
IRsend irsend(IRSenderPin);
// The IR receiver.
IRrecv irrecv(IRReceiverPin, 1024, 100, false);

decode_results IRResult;
// Somewhere to store the captured message.

// Структура "кнопки". Содержит позицию, размер, текст на самой кнопке, а также ссылку на действие, воспроизводимое по нажатию.
struct Button {
  int location_X; // Положение X (левый верхний угол)
  int location_Y; // Положение Y (левый верхний угол)
  int Size_Width; // Ширина
  int Size_Height; // Длина
  String Label; // Надпись
  int Label_Font_Size; // Размер шрифта (Внимание: Выбирать подбором)

  void (*Action)(); // Ссылка на функцию

  uint16_t BackColor;
  uint16_t ForeColor;
  void Draw(uint16_t _BackColor, uint16_t _ForeColor, bool Fill = false) {
      BackColor = _BackColor;
      ForeColor = _ForeColor;
      tft.fillRect(
        location_X, 
        location_Y, 
        Size_Width, 
        Size_Height, 
        BackColor
      );
      tft.drawRect(
        location_X, 
        location_Y, 
        Size_Width, 
        Size_Height, 
        ForeColor
      );
      tft.setTextColor(ForeColor);
      tft.drawCentreString(
        Label,
        location_X + Size_Width / 2,
        location_Y + Size_Height / 4,
        Label_Font_Size
      );
      tft.setTextColor(TFT_WHITE);
  }

  String DrawHTML(int i) {
    if (Label == "<-")
          return "<a href=\"/back\" class=\"button simple-button\" style=\"top: "+String(location_Y)+"; left: "+String(location_X)+"px; width: "+String(Size_Width)+"px; height: "+String(Size_Height)+"px; background-color: black; color: white;\">"+Label+"</a>\n";
    else
          return "<a href=\"/"+String(i)+"\" class=\"simple-button\" style=\"top: "+String(location_Y)+"; left: "+String(location_X)+"px; width: "+String(Size_Width)+"px; height: "+String(Size_Height)+"px; background-color: black; color: white;\">"+Label+"</a>\n";
  }
};

// База - родительский класс для всех меню. 
class Menu {
  public: // Модификатор доступа
  virtual String Title() { return "Thing.";}; // Текст слева вверху
  Button buttons[1]; // Массив с кнопками
  virtual Button* getButtons() { return buttons; } // Виртуальная функция для получения значения
  virtual int getButtonsLength() { return 1; } // Количество кнопок
  virtual void CustomDraw() { } // Количество кнопок
  virtual void MenuLoop() {}; // Цикл, вызываемый для актуального (загруженного) меню.
 
  virtual String HTML() {
    String HTMLSTR = "<html>\n\
    <head><meta charset=\"UTF-8\">\n\
    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n\
    <title>Buttons</title>\n\
    <style>\n\
        body {\n\
            background-color: black;\n\
            position: relative;\n\
			      margin: 0; \n\
			      padding: 0; \n\
			      border: 0;\n\
        }\n\
        .header {\n\
            background-color: white;\n\
            position: absolute;\n\
			      color: white;\n\
        }\n\
        .simple-button {\n\
            border: 3px solid white;\n\
            text-decoration: none;\n\
            display: flex;\n\
            justify-content: center;\n\
            cursor: pointer;\n\
            position: absolute;\n\
			      align-items: center;\n\
        }\n\
		    .simple-text {\n\
            position: absolute;\n\
        }\n\
		    .base64-image {\n\
            position: absolute;\n\
            left: 270px;\n\
        }	\n\
    </style>\n\
  </head>\n\
  <body>\n\
	<div class=\"header\" style=\"top: 40px; left: 10px; background-color: white; width: 250px; height: 3px;\"></div>\n\
	<div class=\"simple-text\" style=\"top: 10px; left: 10px; width: 200px; height: 40px; font-size: 25px; color: white;\">"+Title()+"</div>\n\
  <img class=\"base64-image\" src=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEAAAAAtCAYAAAAeA21aAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAAMeSURBVGhD7Zk/ctUwEMb9khYqLuDchMxQMvSUNOEOOQBUHIA0lFyAoeUCXIGxD0CKTOgf+qRdv5Us648lPRfPv5kdyYktW6vdb2W/bmcbemVHsk25onZHMCj7ZbpNwPgwjoDW98uGH6oVPHFpLe8XhFPAzcnaOWqN179UnTtjW3PxGjBzwPBeJeRbOjCU5qh1vWf8TZk5AOEJEyB8YWuxrveMvykHavGAWojcvDw8UKcATBgrP/FK3e6T0b3DQT/CqOwGnXNz8Rowj4C/qnlUC/LlFofd+Gzs9oc+tFcygynsaeV1FCg8EUAnJFMUOXMHHFWlggPuT+PCATffTb+4dH21K6vHAbmll+ewCr8DJB+Lxvcz14CJFAfLBSFWa8iuAdTOU4A0QPdbwBrwedT6wHuD1BKJKAAiEvhBsyJhOQIw8VaTB874mHjq5IHnfHjUeDWDeQREctC3L8CDhKqDm7PyfIyXe32ELNHaNYDaYARgBXgfwLnHDMPQjePYfXhnNMNdSV45vg7nI/T7B3M+RxSHM66X9wP9i6779poOIqj7Ia9gJGJhkiMAD+VOHvR9r23p/+7f+XwX9zw+1vbPOCjFFFla4MuXIx5QrxRw9gG1NQDE/p+LM15QE3YNoFaiQ0ih3+GH36pBuRLvBj4o/BZxr+PzsVK+HI+NF4PvF9OEpfA4iaLn3aAmKWWwBEpZOMA7gWB+ELYmgIBDfBoRovjlKkLMAbsGUBvC1gQOVbGNlSD3YLKOS9xQL831GDVSAJw0Yd3r6kTrkHepogHUJtNS1HLZNSBCqAyanB8G9NOh6tA6t1NZmwL2PiCHFp/QCljjgHndz6XhxikFR4SDK3LxGsAO4JAvWHYBf/Xl3wDOCFb+zc8p5KNhKCMATsgTvBBwAn34PCcI/z9PpksWpL4IujQWRRI5SdYN930AtT50/KqKYCVydnVARYDx7wwVkO8aqu/meTTsJSnhYuXAqpSoXBZzylyMNReX7xMkCRrhTBhglat4dNcAanOANkzfB4pZ+K4gub7qRlXaXBHJyvXawAkQg3NZox1V1/0HQtRnmAgP6AoAAAAASUVORK5CYII=\" />\n";
  for (int i = 1; i <= getButtonsLength(); i++) {
      HTMLSTR += buttons[i].DrawHTML(i);
  }
  HTMLSTR += "</body>\n\
  </html>"; // HTML заканчивается
  return HTMLSTR;
  };

  // Функция рисования меню. По умолчанию отрисовывает все кнопки в нём.
  void Draw() {
    tft.fillRect(10, 10, 200, 39, TFT_BLACK);
    tft.drawString(Title(), 10, 10, 4);
    tft.drawLine(10, 40, 240, 40, TFT_WHITE);
    tft.fillRect(0, 50, 320, 190, TFT_BLACK);
    for (int i = 0; i <= getButtonsLength(); i++) {
      buttons[i].Draw(TFT_BLACK, TFT_WHITE);
    }

    CustomDraw();
  }

};
Menu* Actual_Menu;

void DoLog(String text) {
  String LogText = "[DEBUG] ";
  LogText += "[" + DateTime + "] ";
  LogText += text;
  if (DebugMode) {
    Serial.println(LogText);
  }
}

// Анимация лисички в шляпе на старте.
void FoxAnimation(String Text, String UpperText = "", uint16_t UpperTextColor = TFT_BLACK) {
  DoLog("Running splash-screen with text \"" + Text + "\" and UpperText - \"" + UpperText + "\"" );
  tft.fillScreen(TFT_WHITE);
  for (int i = 0; i < 5; i++) {
    tft.pushImage(30, 50, 191, 144, FoxyOnStart[i]);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.drawString(Text, 110, 70, 4);
    tft.setTextColor(UpperTextColor, TFT_WHITE);
    tft.drawCentreString(UpperText, 160, 20, 4);
    delay(700);
  }
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.fillScreen(TFT_BLACK);
}

class IR_Menu_Resources_Type {
  public:
  Button* Active_Button;
  int IndexOfActive_Button;
  Menu* _IRMenu;

  struct IRData_struct {
    uint16_t* raw_array;
    uint16_t length;
    String RawString;
    String Address;
    String Command;
    String Protocol;
    void Fill(decode_results * results) {
      raw_array = resultToRawArray(results);
      length = getCorrectedRawLength(results);
      RawString = String(results->value, HEX);
      Address = String(results->address, HEX);
      Command = String(results->command, HEX);
      Protocol = typeToString(results->decode_type).c_str();
    }
  };
  void saveData(fs::FS &fs,  IRData_struct data) {
    if (DebugMode) Serial.printf("Writing file: %s\r\n", "IRData");

    fs::File file = fs.open("/IRData", FILE_WRITE);
    if (!file) {
      if (DebugMode) DoLog("IR FS - failed to open file for writing");
      return;
    }
    if(file.write((uint8_t*)&data, sizeof(data.raw_array) + sizeof(data.length))) { // Записываем uint16_t)
      if (DebugMode) DoLog("IR FS - file written");
    }
    else {
      if (DebugMode) DoLog("IR FS - write failed");
    }
    file.println(data.RawString);  // Записываем строку
    file.println(data.Address);  // Записываем строку
    file.println(data.Command);  // Записываем строку
    file.println(data.Protocol);  // Записываем строку
    file.close();
  }
  IRData_struct loadData(fs::FS &fs) {
    IRData_struct data;
    fs::File file = fs.open("/IRData");
    if (!file || file.isDirectory()) {
      DoLog("IR FS- failed to open file for reading");
      return data;
    }

    file.read((uint8_t *)&data, sizeof(data.raw_array) + sizeof(data.length));  // Читаем uint16_t
    data.RawString = file.readStringUntil('\n');                                  // Читаем строку
    data.RawString.remove(data.RawString.length() -1);
    data.Address = file.readStringUntil('\n');
    data.Address.remove(data.Address.length() -1);
    data.Command = file.readStringUntil('\n');
    data.Command.remove(data.Command.length() -1);
    data.Protocol = file.readStringUntil('\n');
    data.Protocol.remove(data.Protocol.length() -1);
    file.close();
    return data;
  }
  String HexToStr(uint16_t hexcode) {
    String str = String(hexcode, HEX);
    str.toUpperCase();
    return str;
  }

  IRData_struct IRData[6];
  
  void ResourcesLoop () {
    if (irrecv.decode(&IRResult)) {
      Serial.println(String(IRResult.value, HEX));
      if (String(IRResult.value, HEX).length() > 2) {
        for (int i = 1; i < 5; i++) {
          IRData[i] = IRData[i+1];
          _IRMenu->getButtons()[i].Label = _IRMenu->getButtons()[i+1].Label;
          _IRMenu->getButtons()[i].Draw(TFT_BLACK, TFT_WHITE);
          Active_Button = NULL;
        }
        IRData[5].Fill(&IRResult);
        _IRMenu->getButtons()[5].Label = IRData[5].RawString;
        _IRMenu->getButtons()[5].Draw(TFT_BLACK, TFT_WHITE);
        }
      irrecv.resume();
    }
  }
  void SendRawData() {
   // if (DebugMode) Serial.println(IRData[IndexOfActive_Button].raw_array);
    if (DebugMode) Serial.println(IRData[IndexOfActive_Button].length);
    irsend.sendRaw(IRData[IndexOfActive_Button].raw_array, IRData[IndexOfActive_Button].length, 38000);
  }
  void setIRMenu(Menu* _Menu) {
    _IRMenu = _Menu;
  }
  void setActiveButton (int Index) {
    if (IRData[Index].RawString != "") {
      if (Active_Button) Active_Button->Draw(TFT_BLACK, TFT_WHITE); // Чистим
      Active_Button = &_IRMenu->getButtons()[Index];
      IndexOfActive_Button = Index;
      Active_Button->Draw(TFT_WHITE, TFT_BLACK, true);
      tft.fillRect(110, 50, 110, 120, TFT_BLACK);
      tft.drawCentreString(IRData[Index].RawString, 165, 50, 4);
      tft.drawCentreString(IRData[Index].Address + " " + IRData[Index].Command, 165, 70, 2);
      tft.drawCentreString(IRData[Index].Protocol, 165, 83, 2);
      tft.drawCentreString(DateTime, 165, 93, 2);
    }
  }
  void SaveIRData() {
    saveData(LittleFS, IRData[IndexOfActive_Button]);
  }
  void LoadIRData() {
    for (int i = 1; i < 5; i++) {
          IRData[i] = IRData[i+1];
          _IRMenu->getButtons()[i].Label = _IRMenu->getButtons()[i+1].Label;
          _IRMenu->getButtons()[i].Draw(TFT_BLACK, TFT_WHITE);
          Active_Button = NULL;
        }
        IRData[5] = loadData(LittleFS);
        _IRMenu->getButtons()[5].Label = IRData[5].RawString;
        _IRMenu->getButtons()[5].Draw(TFT_BLACK, TFT_WHITE);
  }
};
IR_Menu_Resources_Type IR_Menu_Resources;


class IR_Menu_Type : public Menu {
  public:
  String Title() override { return "IR.";}
  Button buttons[9] = {
        {10, 200, 40, 30, "<-", 2, []() { irrecv.disableIRIn();}}, //Пример кнопок. Есть параметры и лямбда-функция.
        {10, 50, 100, 20, "", 1, []() { IR_Menu_Resources.setActiveButton(1); }},
        {10, 80, 100, 20, "", 1, []() { IR_Menu_Resources.setActiveButton(2); }},
        {10, 110, 100, 20, "", 1, []() { IR_Menu_Resources.setActiveButton(3); }},
        {10, 140, 100, 20, "", 1, []() { IR_Menu_Resources.setActiveButton(4); }},
        {10, 170, 100, 20, "", 1, []() { IR_Menu_Resources.setActiveButton(5); }},

        {220, 80, 100, 30, "SAVE", 2, []() { IR_Menu_Resources.SaveIRData();}},
        {220, 120, 100, 30, "EMULATE", 2, []() { IR_Menu_Resources.SendRawData();}},
        {220, 160, 100, 30, "Memory", 2, []() { IR_Menu_Resources.LoadIRData(); }}
    };

  Button* getButtons() override { return buttons; } // 2^16 способов отстрелить себе конечность
  void MenuLoop() override {
    IR_Menu_Resources.ResourcesLoop();
  }
  void CustomDraw() override {
    tft.drawLine(10, 200, 310, 200, TFT_WHITE);
    irrecv.enableIRIn();  // Start up the IR receiver.
  }
  int getButtonsLength() override { return 9; } // 
};
IR_Menu_Type IR_Menu;



class Serial_Menu_Type2 : public Menu {
  public:
  String Title() override { return "Serial.";}
  Button buttons[1] = {
        {10, 200, 40, 30, "<-", 2, []() { if (!DebugMode) Serial.end(); }}, //Пример кнопок. Есть параметры и лямбда-функция.

    };
  Button* getButtons() override { return buttons; } // 2^16 способов отстрелить себе конечность
  void MenuLoop() {
    if (Serial.available() > 0) {
      tft.setTextColor(TFT_GREEN);
      tft.println("> " + Serial.readString());
      tft.setCursor(10 + tft.getCursorX(), 5 + tft.getCursorY());
      tft.setTextColor(TFT_WHITE);
    }
  }
  int getButtonsLength() override { return 1; } // 
  void CustomDraw() override {
    tft.drawLine(10, 200, 310, 200, TFT_WHITE);
    delay(500);
    Serial.end();
    delay(500);
    Serial.begin(SerialFrq.toInt()); 
    tft.drawCentreString(SerialFrq, 160, 210, 2);
    tft.setCursor(10,50);
  }
};
Serial_Menu_Type2 Serial_Menu2;



class Serial_Menu_Type : public Menu {
  public:
  String Title() override { return "Serial.";}
  Button buttons[13] = {
        {10, 200, 40, 30, "<-", 2, []() { }}, //Пример кнопок. Есть параметры и лямбда-функция.

        {160, 140, 30, 30, "1", 2, []() {if (SerialFrq.length() < 8) SerialFrq += "1"; tft.fillRect(0, 90, 150, 90, TFT_BLACK); tft.drawCentreString(SerialFrq, 80, 100, 4);}},
        {200, 140, 30, 30, "2", 2, []() {if (SerialFrq.length() < 8) SerialFrq += "2"; tft.fillRect(0, 90, 150, 90, TFT_BLACK); tft.drawCentreString(SerialFrq, 80, 100, 4);}}, 
        {240, 140, 30, 30, "3", 2, []() {if (SerialFrq.length() < 8) SerialFrq += "3"; tft.fillRect(0, 90, 150, 90, TFT_BLACK); tft.drawCentreString(SerialFrq, 80, 100, 4);}}, 
        {160, 100, 30, 30, "4", 2, []() {if (SerialFrq.length() < 8) SerialFrq += "4"; tft.fillRect(0, 90, 150, 90, TFT_BLACK); tft.drawCentreString(SerialFrq, 80, 100, 4);}},
        {200, 100, 30, 30, "5", 2, []() {if (SerialFrq.length() < 8) SerialFrq += "5"; tft.fillRect(0, 90, 150, 90, TFT_BLACK); tft.drawCentreString(SerialFrq, 80, 100, 4);}},  
        {240, 100, 30, 30, "6", 2, []() {if (SerialFrq.length() < 8) SerialFrq += "6"; tft.fillRect(0, 90, 150, 90, TFT_BLACK); tft.drawCentreString(SerialFrq, 80, 100, 4);}}, 
        {160, 60, 30, 30, "7", 2, []() {if (SerialFrq.length() < 8) SerialFrq += "7"; tft.fillRect(0, 90, 150, 90, TFT_BLACK); tft.drawCentreString(SerialFrq, 80, 100, 4);}},  
        {200, 60, 30, 30, "8", 2, []() {if (SerialFrq.length() < 8) SerialFrq += "8"; tft.fillRect(0, 90, 150, 90, TFT_BLACK); tft.drawCentreString(SerialFrq, 80, 100, 4);}},  
        {240, 60, 30, 30, "9", 2, []() {if (SerialFrq.length() < 8) SerialFrq += "9"; tft.fillRect(0, 90, 150, 90, TFT_BLACK); tft.drawCentreString(SerialFrq, 80, 100, 4);}},

        {280, 60, 30, 30, "<=", 2, []() {if (SerialFrq.length() > 0) SerialFrq.remove(SerialFrq.length() -1); tft.fillRect(0, 90, 150, 90, TFT_BLACK); tft.drawCentreString(SerialFrq, 80, 100, 4);}}, 
        {280, 100, 30, 70, "0", 2, []() {if (SerialFrq.length() < 8) SerialFrq += "0"; tft.fillRect(0, 90, 150, 90, TFT_BLACK); tft.drawCentreString(SerialFrq, 80, 100, 4);}},

        {10, 60, 140, 30, "Run Serial port", 2, []() { Serial_Menu2.Draw(); Actual_Menu = &Serial_Menu2;}}

    };
  Button* getButtons() override { return buttons; } // 2^16 способов отстрелить себе конечность
  int getButtonsLength() override { return 13; } // 
  void CustomDraw() override {
    tft.drawLine(10, 200, 310, 200, TFT_WHITE);
    tft.fillRect(0, 90, 150, 90, TFT_BLACK);
    tft.drawCentreString(SerialFrq, 80, 100, 4);
  }
};
Serial_Menu_Type Serial_Menu;

class WIFI_Menu_Type : public Menu {
  public:
  String Title() override { return "Wi-Fi.";}
  Button buttons[1] = {
        {10, 200, 40, 30, "<-", 2, []() { }}, //Пример кнопок. Есть параметры и лямбда-функция.
    };

  Button* getButtons() override { return buttons; } // 2^16 способов отстрелить себе конечность
  void MenuLoop() override {
  }
  void CustomDraw() override {
    tft.drawLine(10, 200, 310, 200, TFT_WHITE);
    tft.setCursor(5,50);
    int n = WiFi.scanNetworks();
    if (n == 0) {
        Serial.println("No networks found");
    } else {
        tft.print(n);
        tft.println(" networks found");
        tft.println("");
        tft.println(" N| SSID                           |RSSI|CH|Encr");
        tft.println("");
        for (int i = 0; i < n; ++i) {
            // Print SSID and RSSI for each network found
            tft.printf("%2d",i + 1);
            tft.print("|");
            tft.printf("%-32.32s", WiFi.SSID(i).c_str());
            tft.print("|");
            tft.printf("%4d", WiFi.RSSI(i));
            tft.print("|");
            tft.printf("%2d", WiFi.channel(i));
            tft.print("|");
            switch (WiFi.encryptionType(i))
            {
            case WIFI_AUTH_OPEN:
                tft.print("open");
                break;
            case WIFI_AUTH_WEP:
                tft.print("WEP");
                break;
            case WIFI_AUTH_WPA_PSK:
                tft.print("WPA");
                break;
            case WIFI_AUTH_WPA2_PSK:
                tft.print("WPA2");
                break;
            case WIFI_AUTH_WPA_WPA2_PSK:
                tft.print("WPA+WPA2");
                break;
            case WIFI_AUTH_WPA2_ENTERPRISE:
                tft.print("WPA2-EAP");
                break;
            case WIFI_AUTH_WPA3_PSK:
                tft.print("WPA3");
                break;
            case WIFI_AUTH_WPA2_WPA3_PSK:
                tft.print("WPA2+WPA3");
                break;
            case WIFI_AUTH_WAPI_PSK:
                tft.print("WAPI");
                break;
            default:
                tft.print("unknown");
            }
            tft.println();
            delay(10);
        }
    }
    tft.println("");

    // Delete the scan result to free memory for code below.
    WiFi.scanDelete();
  }
  int getButtonsLength() override { return 1; } // 
};
WIFI_Menu_Type WIFI_Menu;

// Пример наследуемого класса: класс главного меню. Имеет кучу кнопок, и ничего более.
class Main_Menu_Type : public Menu {
  public:
  Button buttons[9] = {
        {20, 60, 80, 40, "433Mhz", 2, []() { FoxAnimation("Be careful!", "Actions are limited by law!", TFT_RED); }}, //Пример кнопок. Есть параметры и лямбда-функция.
      	{120, 60, 80, 40, "IR", 2, []() { IR_Menu.Draw(); Actual_Menu = &IR_Menu; }},
        {220, 60, 80, 40, "NFC", 2, []() { Serial.println("Button1;3"); }},

        {20, 120, 80, 40, "WiFi", 2, []() { WIFI_Menu.Draw(); Actual_Menu = &WIFI_Menu;}},
      	{120, 120, 80, 40, "GPIO", 2, []() { Serial.println("Button2;2"); }},
        {220, 120, 80, 40, "Serial", 2, []() { Serial_Menu.Draw(); Actual_Menu = &Serial_Menu;}},

        {20, 180, 80, 40, "WiFi", 2, []() { Serial.println("Button2;1"); }},
      	{120, 180, 80, 40, "GPIO", 2, []() { Serial.println("Button2;2"); }},
        {220, 180, 80, 40, "Serial", 2, []() { Serial_Menu.Draw(); Actual_Menu = &Serial_Menu;}},
    };

  Button* getButtons() override { return buttons; } // 2^16 способов отстрелить себе конечность
  int getButtonsLength() override { return 9; } // 
};

Main_Menu_Type Main_Menu;

// Обработчик тач-скрина. Получает X и Y нажатия и актуальное меню. Сам ищет среди кнопок совпадения.
void TouchRenderer(Menu* _Menu, uint16_t TouchX, uint16_t TouchY) {
  for (int i = 0; i < _Menu->getButtonsLength(); i++) {
    if (_Menu->getButtons()[i].location_X < TouchX && // Левый край
       _Menu->getButtons()[i].Size_Width + _Menu->getButtons()[i].location_X > TouchX &&  // Правый край
       _Menu->getButtons()[i].location_Y < TouchY && // Верх
       _Menu->getButtons()[i].Size_Height + _Menu->getButtons()[i].location_Y > TouchY) // Низ
      { 
        _Menu->getButtons()[i].Action(); // Если в кнопке - выполняем действие этой кнопки
        if (_Menu->getButtons()[i].Label == "<-") { Main_Menu.Draw(); Actual_Menu = &Main_Menu; }
        DoLog("Do something on button " + _Menu->getButtons()[i].Label + " by clicking at X:" + TouchX + " Y:" + TouchY);
      }
  }
  CoolDown = true;
}
void CreateHTMLFromActual_Menu() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(200, "text/html", Actual_Menu->HTML());
  });
    server.on("/1", HTTP_GET, [](AsyncWebServerRequest *request){
      request->redirect("/");
      Actual_Menu->getButtons()[0].Action();
    });
    server.on("/2", HTTP_GET, [](AsyncWebServerRequest *request){
      request->redirect("/");
      Actual_Menu->getButtons()[1].Action();
    });
    server.on("/3", HTTP_GET, [](AsyncWebServerRequest *request){
      request->redirect("/");
      Actual_Menu->getButtons()[2].Action();
    });
    server.on("/4", HTTP_GET, [](AsyncWebServerRequest *request){
      request->redirect("/");
      Actual_Menu->getButtons()[3].Action();
    });
    server.on("/5", HTTP_GET, [](AsyncWebServerRequest *request){
      request->redirect("/");
      Actual_Menu->getButtons()[4].Action();
    });
    server.on("/6", HTTP_GET, [](AsyncWebServerRequest *request){
      request->redirect("/");
      Actual_Menu->getButtons()[5].Action();
    });
    server.on("/7", HTTP_GET, [](AsyncWebServerRequest *request){
      request->redirect("/");
      Actual_Menu->getButtons()[6].Action();
    });
    server.on("/8", HTTP_GET, [](AsyncWebServerRequest *request){
      request->redirect("/");
      Actual_Menu->getButtons()[7].Action();
    });
    server.on("/9", HTTP_GET, [](AsyncWebServerRequest *request){
      request->redirect("/");
      Actual_Menu->getButtons()[8].Action();
    });
    server.on("/10", HTTP_GET, [](AsyncWebServerRequest *request){
      request->redirect("/");
      Actual_Menu->getButtons()[9].Action();
    });
    server.on("/11", HTTP_GET, [](AsyncWebServerRequest *request){
      request->redirect("/");
      Actual_Menu->getButtons()[10].Action();
    });
    server.on("/12", HTTP_GET, [](AsyncWebServerRequest *request){
      request->redirect("/");
      Actual_Menu->getButtons()[11].Action();
    });
    server.on("/13", HTTP_GET, [](AsyncWebServerRequest *request){
      request->redirect("/");
      Actual_Menu->getButtons()[12].Action();
    });
    server.on("/14", HTTP_GET, [](AsyncWebServerRequest *request){
      request->redirect("/");
      Actual_Menu->getButtons()[13].Action();
    });
      server.on("/back", HTTP_GET, [](AsyncWebServerRequest *request){
        request->redirect("/");
        Main_Menu.Draw(); 
        Actual_Menu = &Main_Menu; 
      });
  DoLog("Paint menu "+Actual_Menu->Title()+" on a WEB");
}

void setup() {
  pinMode(IRReceiverPin, INPUT_PULLUP);

  pinMode(IRSenderPin, OUTPUT);
  irsend.begin();       // Start up the IR sender.
  Serial.setTimeout(50);

  if (DebugMode) Serial.begin(SerialFrq.toInt());

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    if (DebugMode) DoLog("Connecting to WiFi..");
  }
  if (DebugMode) Serial.println(WiFi.localIP());

  if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
    if (DebugMode) DoLog("LittleFS Mount Failed");
    return;
  } else {
    if (DebugMode) DoLog("Little FS Mounted Successfully");
  }

  tft.init();
  tft.setRotation(1);
  tft.setSwapBytes(true);

  FoxAnimation("Welcome!");
  
  uint16_t calData[5] = { 437, 3472, 286, 3526, 3 };
  tft.setTouch(calData);

  Actual_Menu = &Main_Menu;
  Main_Menu.Draw();
  IR_Menu_Resources.setIRMenu(&IR_Menu);

  DoLog(Main_Menu.HTML());
  CreateHTMLFromActual_Menu();
  server.begin();
}

void loop() {
  uint16_t x = 0, y = 0; // Координаты тача
  if (tft.getTouch(&x, &y) && !CoolDown) { // Если коснулись..
    TouchRenderer(Actual_Menu, x, y); // Обработать нажатие
    CreateHTMLFromActual_Menu();
  }

  if (millis() - timer >= 700) { // таймер на millis() для лисы сверху
    CoolDown = false;
    timer = millis(); // сброс таймера
    if (FoxyCounter == 1) tft.pushImage(250, -10, 64, 64, Foxy[0]);
    if (FoxyCounter == 2) tft.pushImage(250, -10, 64, 64, Foxy[1]);
    if (FoxyCounter == 3) tft.pushImage(250, -10, 64, 64, Foxy[2]);
    if (FoxyCounter == 4) tft.pushImage(250, -10, 64, 64, Foxy[1]);
    if (FoxyCounter == 5) tft.pushImage(250, -10, 64, 64, Foxy[2]);
    FoxyCounter++;
    if (FoxyCounter >= 6) FoxyCounter = 1;
  }
  Actual_Menu->MenuLoop();
}

// "Пусть кто-то говорит, что ты слаб и бездарен
//  Не слушай и играй на любимой гитаре"
//  - Екатерина Яшникова, "Это возможно".