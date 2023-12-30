#pragma GCC optimize ("Ofast")
#include <M5Core2.h>
#include <AXP192.h>
#include <ESP32Encoder.h>
#include <esp_now.h>
#include <WiFi.h>
#include <PatternMath.h>
#include "OneButton.h"          //For Button Debounce and Longpress
#include "config.h"
#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>
#include <SPI.h>
#include "ui/ui.h"
#include <EEPROM.h>
#include "main.h"

int screenWidth  = 320;
int screenHeight = 240;

///////////////////////////////////////////
////
////  To Debug or not to Debug
////
///////////////////////////////////////////

// Uncomment the following line if you wish to print DEBUG info
#define DEBUG 

#ifdef DEBUG
#define LogDebug(...) Serial.println(__VA_ARGS__)
#define LogDebugFormatted(...) Serial.printf(__VA_ARGS__)
#else
#define LogDebug(...) ((void)0)
#define LogDebugFormatted(...) ((void)0)
#endif

#define OFF 0.0
#define ON 1.0

// Screens 

#define ST_UI_START 0
#define ST_UI_HOME 1

#define ST_UI_MENUE 10
#define ST_UI_PATTERN 11
#define ST_UI_Torqe 12
#define ST_UI_EJECTSETTINGS 13

#define ST_UI_SETTINGS 20

int st_screens = ST_UI_START;



// Menü States

#define CONNECT 0
#define HOME 1
#define MENUE 2
#define MENUE2 3
#define TORQE 4
#define PATTERN_MENUE 5
#define PATTERN_MENUE2 6
#define PATTERN_MENUE3 7
#define CUM_MENUE 20

int menuestatus = CONNECT;

// EEPROM Area:

#define EJECT 0
#define DARKMODE 1
#define VIBRATE 2
#define LEFTY 6
#define CUM_SIZE 10
#define CUM_TIME 14
#define CUM_ACCEL 18
#define CUM_REVERSE 22

bool eject_status = false;
bool dark_mode = false;
bool vibrate_mode = true;
bool touch_home = false;
bool touch_disabled = false;

// Command States
#define CONN 0
#define SPEED 1
#define DEPTH 2
#define STROKE 3
#define SENSATION 4
#define PATTERN 5
#define TORQE_F 6
#define TORQE_R 7
#define OFF 10
#define ON  11
#define SETUP_D_I 12
#define SETUP_D_I_F 13
#define REBOOT 14

#define CUMCONN 20
#define CUMTOGGLE 21
#define CUMTIME 22
#define CUMSIZE   23
#define CUMACCEL  24
#define CUMREVERSE 25

#define CONNECT 88
#define HEARTBEAT 99

int displaywidth;
int displayheight;
int progheight = 30;
int distheight = 10;
int S1Pos;
int S2Pos;
int S3Pos;
int S4Pos;
bool rstate = false;
int pattern = 2;
char patternstr[20];
bool onoff = false;


long speedenc = 0;
long depthenc = 0;
long strokeenc = 0;
long sensationenc = 0;
long torqe_f_enc = 0;
long torqe_r_enc = 0;
long cum_time_enc = 0;
long cum_size_enc =0;
long cum_accel_enc = 0;
long encoder4_enc = 0;

extern float maxdepthinmm = 180.0;
extern float speedlimit = 600;
int speedscale = 0;

extern float cum_maxtime = 300;
extern float cum_maxsize = 800;
extern float cum_maxaccel = 500;

float speed = 0.0;
float depth = 0.0;
float stroke = 0.0;
float sensation = 0.0;
float torqe_f = 100.0;
float torqe_r = -180.0;
float cum_time = 1.0;
float cum_size = 1.0;
float cum_accel = 1.0;
bool cum_reverse = false;

ESP32Encoder encoder1;
ESP32Encoder encoder2;
ESP32Encoder encoder3;
ESP32Encoder encoder4;

// Variable to store if sending data was successful
String success;

float out_esp_speed;
float out_esp_depth;
float out_esp_stroke;
float out_esp_sensation;
float out_esp_pattern;
bool out_esp_rstate;
bool out_esp_connected;
int out_esp_command;
float out_esp_value;
int out_esp_target;

float incoming_esp_speed;
float incoming_esp_depth;
float incoming_esp_stroke;
float incoming_esp_sensation;
float incoming_esp_pattern;
bool incoming_esp_rstate;
bool incoming_esp_connected;
bool incoming_esp_heartbeat;
int incoming_esp_target;

typedef struct struct_message {
  float esp_speed;
  float esp_depth;
  float esp_stroke;
  float esp_sensation;
  float esp_pattern;
  bool esp_rstate;
  bool esp_connected;
  bool esp_heartbeat;
  int esp_command;
  float esp_value;
  int esp_target;
} struct_message;

bool Ossm_paired = false;
bool Eject_paired = false;

struct_message outgoingcontrol;
struct_message incomingcontrol;

esp_now_peer_info_t ossmPeerInfo;
esp_now_peer_info_t ejectPeerInfo;
uint8_t OSSM_Address[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Broadcast to all ESP32s, upon connection gets updated to the actual address
uint8_t EJECT_Address[] = {0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
// uint8_t EJECT_Address[] = {0x30, 0xC6, 0xF7, 0x23, 0x9D, 0x8C};

#define HEARTBEAT_INTERVAL 5000/portTICK_PERIOD_MS	// 5 seconds

// Bool

bool EJECT_On = false;
bool OSSM_On = false;

#define EEPROM_SIZE 200

// Tasks:

TaskHandle_t eRemote_t  = nullptr;  // Esp Now Remote Task

void espNowRemoteTask(void *pvParameters); // Handels the EspNow Remote
bool connectbtn(); //Handels Connectbtn
int64_t touchmenue();
void vibrate();
void mxclick();
bool mxclick_short_waspressed = false;
void mxlong();
bool mxclick_long_waspressed = false;
void click2();
bool click2_short_waspressed = false;
void click3();
bool click3_short_waspressed = false;

// init the tft espi
static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t disp_drv;  // Descriptor of a display driver
static lv_indev_drv_t indev_drv; // Descriptor of a touch driver

M5Display *tft;
static lv_obj_t * kb;

void tft_lv_initialization() {
  M5.begin();
  lv_init();
  static lv_color_t buf1[(LV_HOR_RES_MAX * LV_VER_RES_MAX) / 10];  // Declare a buffer for 1/10 screen siz
  static lv_color_t buf2[(LV_HOR_RES_MAX * LV_VER_RES_MAX) / 10];  // second buffer is optionnal

  // Initialize `disp_buf` display buffer with the buffer(s).
  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, (LV_HOR_RES_MAX * LV_VER_RES_MAX) / 10);

  tft = &M5.Lcd;
}

// Display flushing
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft->startWrite();
  tft->setAddrWindow(area->x1, area->y1, w, h);
  tft->pushColors((uint16_t *)&color_p->full, w * h, true);
  tft->endWrite();

  lv_disp_flush_ready(disp);
}

void init_disp_driver() {
  lv_disp_drv_init(&disp_drv);  // Basic initialization

  disp_drv.flush_cb = my_disp_flush;  // Set your driver function
  disp_drv.draw_buf = &draw_buf;      // Assign the buffer to the display
  disp_drv.hor_res = LV_HOR_RES_MAX;  // Set the horizontal resolution of the display
  disp_drv.ver_res = LV_VER_RES_MAX;  // Set the vertical resolution of the display

  lv_disp_drv_register(&disp_drv);                   // Finally register the driver
  lv_disp_set_bg_color(NULL, lv_color_hex3(0x000));  // Set default background color to black
}

void my_touchpad_read(lv_indev_drv_t * drv, lv_indev_data_t * data)
{
  if(touch_disabled == false){
  TouchPoint_t pos = M5.Touch.getPressPoint();
  bool touched = ( pos.x == -1 ) ? false : true;  

  if(!touched) {
      data->state = LV_INDEV_STATE_RELEASED;
  } else {
    if (M5.BtnA.wasPressed()){  // tab 1 : A Button
      LogDebug("ButtonA");
      data->point.x = 80; data->point.y = 220; // mouse position x,y
      data->state =LV_INDEV_STATE_PR; M5.update();
      } else if (M5.BtnB.wasPressed()){  // tab 2 : B Button
      LogDebug("ButtonB");
      data->point.x = 160; data->point.y = 220;
      data->state =LV_INDEV_STATE_PR; M5.update();
      } else if (M5.BtnC.wasPressed()){  // tab 3 : C Button
      LogDebug("ButtonC");
      data->point.x = 270; data->point.y = 220;
      data->state =LV_INDEV_STATE_PR; M5.update();
      } else {
    data->state = LV_INDEV_STATE_PRESSED; 
    data->point.x = pos.x;
    data->point.y = pos.y;
  }
  } 
}
}

void init_touch_driver() {
  lv_disp_drv_register(&disp_drv);

  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_t * my_indev = lv_indev_drv_register(&indev_drv);  // register
}

//Sends Commands and Value to Remote device returns ture or false if sended
bool SendCommand(int Command, float Value, int Target){  
  if(Ossm_paired && Target == OSSM_ID){
    outgoingcontrol.esp_connected = true;
    outgoingcontrol.esp_command = Command;
    outgoingcontrol.esp_value = Value;
    outgoingcontrol.esp_target = OSSM_ID;
    esp_err_t result = esp_now_send(OSSM_Address, (uint8_t *) &outgoingcontrol, sizeof(outgoingcontrol));
  
    if (result == ESP_OK) {
      return true;
    } 
    else {
      delay(20);
      esp_err_t result = esp_now_send(OSSM_Address, (uint8_t *) &outgoingcontrol, sizeof(outgoingcontrol));
      return false;
    }
  }

  if(Eject_paired && Target == EJECT_ID){
    outgoingcontrol.esp_connected = true;
    outgoingcontrol.esp_command = Command;
    outgoingcontrol.esp_value = Value;
    outgoingcontrol.esp_target = EJECT_ID;
    esp_err_t result = esp_now_send(EJECT_Address, (uint8_t *) &outgoingcontrol, sizeof(outgoingcontrol));
  
    if (result == ESP_OK) {
      return true;
    } 
    else {
      delay(20);
      esp_err_t result = esp_now_send(EJECT_Address, (uint8_t *) &outgoingcontrol, sizeof(outgoingcontrol));
      return false;
    }
  }
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS){
    if (memcmp(mac_addr, EJECT_Address, sizeof(EJECT_Address)) == 0){
      Eject_paired = false;
      EJECT_On = false;
    }
  }
}

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&incomingcontrol, incomingData, sizeof(incomingcontrol));

  if(incomingcontrol.esp_target == M5_ID){
    if (Ossm_paired == false && incomingcontrol.esp_value == 0) {

      // Remove the existing peer (0xFF:0xFF:0xFF:0xFF:0xFF:0xFF)
      esp_err_t result = esp_now_del_peer(ossmPeerInfo.peer_addr);

      if (result == ESP_OK) {

        memcpy(OSSM_Address, mac, 6); //get the mac address of the sender
        
        // Add the new peer
        memcpy(ossmPeerInfo.peer_addr, OSSM_Address, 6);
        if (esp_now_add_peer(&ossmPeerInfo) == ESP_OK) {
          LogDebugFormatted("New peer added successfully, OSSM addresss : %02X:%02X:%02X:%02X:%02X:%02X\n", OSSM_Address[0], OSSM_Address[1], OSSM_Address[2], OSSM_Address[3], OSSM_Address[4], OSSM_Address[5]);
          Ossm_paired = true;
        }
        else {
          LogDebug("Failed to add new peer");
        }
      }
      else {
        LogDebug("Failed to remove peer");
      }

      speedlimit = incomingcontrol.esp_speed;
      maxdepthinmm = incomingcontrol.esp_depth;
      pattern = incomingcontrol.esp_pattern;
      outgoingcontrol.esp_target = OSSM_ID;
      
      result = esp_now_send(OSSM_Address, (uint8_t *) &outgoingcontrol, sizeof(outgoingcontrol));      
      if (result == ESP_OK) {
        Ossm_paired = true;
        lv_label_set_text(ui_connect, "Connected");
        lv_scr_load_anim(ui_Home, LV_SCR_LOAD_ANIM_FADE_ON,20,0,false);
      }
    }

    if (incomingcontrol.esp_value == EJECT_ID) {
      if (Eject_paired == false) {
        esp_err_t result = esp_now_del_peer(ejectPeerInfo.peer_addr);

        if (result == ESP_OK) {

          memcpy(EJECT_Address, mac, 6); //get the mac address of the sender
          
          // Add the new peer
          memcpy(ejectPeerInfo.peer_addr, EJECT_Address, 6);
          if (esp_now_add_peer(&ejectPeerInfo) == ESP_OK) {
            LogDebugFormatted("New peer added successfully, Ejector addresss : %02X:%02X:%02X:%02X:%02X:%02X\n", EJECT_Address[0], EJECT_Address[1], EJECT_Address[2], EJECT_Address[3], EJECT_Address[4], EJECT_Address[5]);
            Eject_paired = true;
            SendCommand(CUMTIME, cum_time, EJECT_ID);
            SendCommand(CUMSIZE, cum_size, EJECT_ID);
            SendCommand(CUMACCEL, cum_accel, EJECT_ID);
            SendCommand(CUMREVERSE, cum_reverse ? 1 : 0, EJECT_ID);
          }
          else {
            LogDebug("Failed to add new ejector peer");
          }
        }
        else {
          LogDebug("Failed to remove ejector peer");
        }
      }

      switch(incomingcontrol.esp_command)
        {
        case CUMTOGGLE: 
        {
          EJECT_On = false;
        } 
        break;
      }
    }
  }

  switch(incomingcontrol.esp_command)
    {
    case OFF: 
    {
      lv_obj_clear_state(ui_HomeButtonM, LV_STATE_CHECKED);
      OSSM_On = false;
    }
    break;
    case ON:
    {
      lv_obj_add_state(ui_HomeButtonM, LV_STATE_CHECKED);
      OSSM_On = true;
    }
    break;
  }

}

void connectbutton(lv_event_t * e)
{
    if(!Ossm_paired){
      outgoingcontrol.esp_command = HEARTBEAT;
      outgoingcontrol.esp_heartbeat = true;
      outgoingcontrol.esp_target = OSSM_ID;
      esp_err_t result = esp_now_send(OSSM_Address, (uint8_t *) &outgoingcontrol, sizeof(outgoingcontrol));
    }
}

void savesettings(lv_event_t * e)
{
	if(lv_obj_get_state(ui_ejectaddon) == 1){
		EEPROM.writeBool(EJECT,true);
	} else if(lv_obj_get_state(ui_ejectaddon) == 0){
		EEPROM.writeBool(EJECT,false);
	}
  if(lv_obj_get_state(ui_darkmode) == 1){
		EEPROM.writeBool(DARKMODE,true);
	} else if(lv_obj_get_state(ui_darkmode) == 0){
		EEPROM.writeBool(DARKMODE,false);
	}
  if(lv_obj_get_state(ui_vibrate) == 1){
		EEPROM.writeBool(VIBRATE,true);
	} else if(lv_obj_get_state(ui_vibrate) == 0){
		EEPROM.writeBool(VIBRATE,false);
	}
  if(lv_obj_get_state(ui_lefty) == 1){
		EEPROM.writeBool(LEFTY,true);
	} else if(lv_obj_get_state(ui_lefty) == 0){
		EEPROM.writeBool(LEFTY,false);
	}
  EEPROM.commit();
  delay(100);
  ESP.restart();
}

void saveejectsettings(lv_event_t * e)
{
	EEPROM.put(CUM_TIME, cum_time);
  EEPROM.put(CUM_SIZE, cum_size);
  EEPROM.put(CUM_ACCEL, cum_accel);
	EEPROM.writeBool(CUM_REVERSE, lv_obj_has_state(ui_ejectreverse, LV_STATE_CHECKED));
  EEPROM.commit();
  delay(100);
}

void screenmachine(lv_event_t * e)
{
  if (lv_scr_act() == ui_Start){
    st_screens = ST_UI_START;
  } else if (lv_scr_act() == ui_Home){
    st_screens = ST_UI_HOME;
    speed = lv_slider_get_value(ui_homespeedslider);
    speedenc =  fscale(0.5, speedlimit, 0, Encoder_MAP, speed, speedscale);
    encoder1.setCount(speedenc); 

    depth = lv_slider_get_value(ui_homedepthslider);       
    depthenc =  fscale(0, maxdepthinmm, 0, Encoder_MAP, depth, 0);
    encoder2.setCount(depthenc);
            
    stroke = lv_slider_get_value(ui_homestrokeslider);    
    strokeenc =  fscale(0, maxdepthinmm, 0, Encoder_MAP, stroke, 0);
    encoder3.setCount(strokeenc);

    sensation = lv_slider_get_value(ui_homesensationslider);
    sensationenc =  fscale(-100, 100, (Encoder_MAP/2*-1), (Encoder_MAP/2), sensation, 0);
    encoder4.setCount(sensationenc);        
            
  } else if (lv_scr_act() == ui_Menue){
    st_screens = ST_UI_MENUE;
  } else if (lv_scr_act() == ui_Pattern){
    st_screens = ST_UI_PATTERN;
  } else if (lv_scr_act() == ui_Torqe){
    st_screens = ST_UI_Torqe;
    torqe_f = lv_slider_get_value(ui_outtroqeslider);
    torqe_f_enc = fscale(50, 200, 0, Encoder_MAP, torqe_f, 0);
    encoder1.setCount(torqe_f_enc);

    torqe_r = lv_slider_get_value(ui_introqeslider);
    torqe_r_enc = fscale(20, 200, 0, Encoder_MAP, torqe_r, 0);
    encoder4.setCount(torqe_r_enc);

  } else if (lv_scr_act() == ui_EJECTSettings){
    st_screens = ST_UI_EJECTSETTINGS;

    cum_time = lv_slider_get_value(ui_ejecttimeslider);
    cum_time_enc = fscale(1, cum_maxtime, 0, Encoder_MAP, cum_time, 0);
    encoder1.setCount(cum_time_enc); 

    cum_size = lv_slider_get_value(ui_ejectsizeslider);
    cum_size_enc = fscale(1, cum_maxsize, 0, Encoder_MAP, cum_size, 0);
    encoder2.setCount(cum_size_enc);
            
    cum_accel = lv_slider_get_value(ui_ejectaccelslider); 
    cum_accel_enc = fscale(1, cum_maxaccel, 0, Encoder_MAP, cum_accel, 0);
    encoder3.setCount(cum_accel_enc);

  } else if (lv_scr_act() == ui_Settings){
    st_screens = ST_UI_SETTINGS;
  }
}

void ejectcreampie(lv_event_t * e){
  if(Eject_paired){
    LogDebug("Ejecing");

    if(EJECT_On == true){
      EJECT_On = false;
      SendCommand(CUMTOGGLE, 0, EJECT_ID);
    } else if(EJECT_On == false){
      EJECT_On = true;
      SendCommand(CUMTOGGLE, 1, EJECT_ID);
    } 
  }
}

void savepattern(lv_event_t * e){
  pattern = lv_roller_get_selected(ui_PatternS);
  lv_roller_get_selected_str(ui_PatternS,patternstr,0);
  lv_label_set_text(ui_HomePatternLabel,patternstr);
  LogDebug(pattern);
  float patterns = pattern;
  SendCommand(PATTERN, patterns, OSSM_ID);
}

void homebuttonmevent(lv_event_t * e){
  LogDebug("HomeButton");
  if(OSSM_On == false){
    SendCommand(ON, 0.0, OSSM_ID);
  } else if(OSSM_On == true){
    SendCommand(OFF, 0.0, OSSM_ID);
  }
}

void setupDepthInter(lv_event_t * e){
    SendCommand(SETUP_D_I, 0.0, OSSM_ID);
}

void setupdepthF(lv_event_t * e){
    SendCommand(SETUP_D_I_F, 0.0, OSSM_ID);
}

void setup(){
  M5.begin(true, false, true, true); //Init M5Core2.
  EEPROM.begin(EEPROM_SIZE);

  M5.Axp.SetCHGCurrent(AXP192::BATTERY_CHARGE_CURRENT);
  M5.Axp.SetLcdVoltage(3000);
  LogDebug("\n Starting");      // Start LogDebug 

  WiFi.mode(WIFI_STA);
  LogDebug(WiFi.macAddress());

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
  }
  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);

  // register ossm peer
  ossmPeerInfo.channel = 0;  
  ossmPeerInfo.encrypt = false;
  memcpy(ossmPeerInfo.peer_addr, OSSM_Address, 6);
  if (esp_now_add_peer(&ossmPeerInfo) != ESP_OK){
    Serial.println("Failed to add OSSM peer");
  }  
  
  // register eject peer
  ejectPeerInfo.channel = 0;  
  ejectPeerInfo.encrypt = false;
  memcpy(ejectPeerInfo.peer_addr, EJECT_Address, 6);
  if (esp_now_add_peer(&ejectPeerInfo) != ESP_OK){
    Serial.println("Failed to add Ejector peer");
  }


  // Register for a callback function that will be called when data is received
  esp_now_register_recv_cb(OnDataRecv);

  xTaskCreatePinnedToCore(espNowRemoteTask,      /* Task function. */
                            "espNowRemoteTask",  /* name of task. */
                            4096,               /* Stack size of task */
                            NULL,               /* parameter of the task */
                            5,                  /* priority of the task */
                            &eRemote_t,         /* Task handle to keep track of created task */
                            0);                 /* pin task to core 0 */
  delay(200);
  
  encoder1.attachHalfQuad(ENC_1_CLK, ENC_1_DT);
  encoder2.attachHalfQuad(ENC_2_CLK, ENC_2_DT);
  encoder3.attachHalfQuad(ENC_3_CLK, ENC_3_DT);
  encoder4.attachHalfQuad(ENC_4_CLK, ENC_4_DT);
  Button1.attachClick(mxclick);
  Button1.attachLongPressStop(mxlong);
  Button2.attachClick(click2);
  Button3.attachClick(click3);

  tft_lv_initialization();
  init_disp_driver();
  init_touch_driver();

  //****Load EEPROOM:
  eject_status = EEPROM.readBool(EJECT);
  dark_mode = EEPROM.readBool(DARKMODE);
  vibrate_mode = EEPROM.readBool(VIBRATE);
  touch_home = EEPROM.readBool(LEFTY);
  cum_time = EEPROM.get(CUM_TIME, cum_time);
  cum_size = EEPROM.get(CUM_SIZE, cum_size);
  cum_accel = EEPROM.get(CUM_ACCEL, cum_accel);
  cum_reverse = EEPROM.readBool(CUM_REVERSE);

  ui_init();
  
  if(eject_status == true){
  lv_obj_add_state(ui_ejectaddon, LV_STATE_CHECKED);
  lv_obj_clear_state(ui_EJECTSettingButton, LV_STATE_DISABLED);
  lv_obj_clear_state(ui_HomeButtonL, LV_STATE_DISABLED);
  }
  if(dark_mode == true){
  lv_obj_add_state(ui_darkmode, LV_STATE_CHECKED);
  }
  if(vibrate_mode == true){
  lv_obj_add_state(ui_vibrate, LV_STATE_CHECKED);
  }
  if(touch_home == true){
  lv_obj_add_state(ui_lefty, LV_STATE_CHECKED);
  }
  if(cum_reverse == true){
  lv_obj_add_state(ui_ejectreverse, LV_STATE_CHECKED);
  }

  lv_slider_set_value(ui_ejecttimeslider, cum_time, LV_ANIM_OFF);
  lv_slider_set_value(ui_ejectsizeslider, cum_size, LV_ANIM_OFF);
  lv_slider_set_value(ui_ejectaccelslider, cum_accel, LV_ANIM_OFF);
  lv_roller_set_selected(ui_PatternS,2,LV_ANIM_OFF);
  lv_roller_get_selected_str(ui_PatternS,patternstr,0);
  lv_label_set_text(ui_HomePatternLabel,patternstr);
}

void loop()
{
     const int BatteryLevel = M5.Axp.GetBatteryLevel();
     String BatteryValue = (String(BatteryLevel, DEC) + "%");
     const char *battVal = BatteryValue.c_str();
     lv_bar_set_value(ui_Battery, BatteryLevel, LV_ANIM_OFF);
     lv_label_set_text(ui_BattValue, battVal);
     lv_bar_set_value(ui_Battery1, BatteryLevel, LV_ANIM_OFF);
     lv_label_set_text(ui_BattValue1, battVal);
     lv_bar_set_value(ui_Battery2, BatteryLevel, LV_ANIM_OFF);
     lv_label_set_text(ui_BattValue2, battVal);
     lv_bar_set_value(ui_Battery3, BatteryLevel, LV_ANIM_OFF);
     lv_label_set_text(ui_BattValue3, battVal);
     lv_bar_set_value(ui_Battery4, BatteryLevel, LV_ANIM_OFF);
     lv_label_set_text(ui_BattValue4, battVal);
     lv_bar_set_value(ui_Battery5, BatteryLevel, LV_ANIM_OFF);
     lv_label_set_text(ui_BattValue5, battVal);
     lv_bar_set_value(ui_Battery6, BatteryLevel, LV_ANIM_OFF);
     lv_label_set_text(ui_BattValue6, battVal);

     M5.update();
     lv_task_handler();
     Button1.tick();
     Button2.tick();
     Button3.tick();

     switch(st_screens){
      
     case ST_UI_START:
      {
        if(click2_short_waspressed == true){
         lv_event_send(ui_StartButtonL, LV_EVENT_CLICKED, NULL);
        } else if(mxclick_short_waspressed == true){
         lv_event_send(ui_StartButtonM, LV_EVENT_CLICKED, NULL);
        } else if(click3_short_waspressed == true){
         lv_event_send(ui_StartButtonR, LV_EVENT_CLICKED, NULL);
        }
      }
      break;

      case ST_UI_HOME:
      {
        if(touch_home == true){
          touch_disabled = true;
        }
        // Encoder 1 Speed 
        if(lv_slider_is_dragged(ui_homespeedslider) == false){
          if (encoder1.getCount() != speedenc){
            lv_slider_set_value(ui_homespeedslider, speed, LV_ANIM_OFF);
            if(encoder1.getCount() <= 0){
              encoder1.setCount(0);
            } else if (encoder1.getCount() >= Encoder_MAP){
              encoder1.setCount(Encoder_MAP);
            } 
            speedenc = encoder1.getCount();
            speed = fscale(0, Encoder_MAP, 0, speedlimit, speedenc, speedscale);
            SendCommand(SPEED, speed, OSSM_ID);
          }
        } else if(lv_slider_get_value(ui_homespeedslider) != speed){
            speedenc =  fscale(0.5, speedlimit, 0, Encoder_MAP, speed, speedscale);
            encoder1.setCount(speedenc);
            speed = lv_slider_get_value(ui_homespeedslider);
            SendCommand(SPEED, speed, OSSM_ID);
        }
        char speed_v[7];
        dtostrf(speed, 6, 0, speed_v);
        lv_label_set_text(ui_homespeedvalue, speed_v);


        // Encoder2 Depth
        if(lv_slider_is_dragged(ui_homedepthslider) == false){
          if (encoder2.getCount() != depthenc){
            lv_slider_set_value(ui_homedepthslider, depth, LV_ANIM_OFF);
            if(encoder2.getCount() <= 0){
              encoder2.setCount(0);
            } else if (encoder2.getCount() >= Encoder_MAP){
              encoder2.setCount(Encoder_MAP);
            } 
            depthenc = encoder2.getCount();
            depth = fscale(0, Encoder_MAP, 0, maxdepthinmm, depthenc, 0);
            SendCommand(DEPTH, depth, OSSM_ID);
          }
        } else if(lv_slider_get_value(ui_homedepthslider) != depth){
            depthenc =  fscale(0, maxdepthinmm, 0, Encoder_MAP, depth, 0);
            encoder2.setCount(depthenc);
            depth = lv_slider_get_value(ui_homedepthslider);
            SendCommand(DEPTH, depth, OSSM_ID);
        }
        char depth_v[7];
        dtostrf(depth, 6, 0, depth_v);
        lv_label_set_text(ui_homedepthvalue, depth_v);
        

        // Encoder3 Stroke
        if(lv_slider_is_dragged(ui_homestrokeslider) == false){
          if (encoder3.getCount() != strokeenc){
            lv_slider_set_value(ui_homestrokeslider, stroke, LV_ANIM_OFF);
            if(encoder3.getCount() <= 0){
              encoder3.setCount(0);
            } else if (encoder3.getCount() >= Encoder_MAP){
              encoder3.setCount(Encoder_MAP);
            } 
            strokeenc = encoder3.getCount();
            stroke = fscale(0, Encoder_MAP, 0, maxdepthinmm, strokeenc, 0);
            SendCommand(STROKE, stroke, OSSM_ID);
          }
        } else if(lv_slider_get_value(ui_homestrokeslider) != stroke){
            strokeenc =  fscale(0, maxdepthinmm, 0, Encoder_MAP, stroke, 0);
            encoder3.setCount(strokeenc);
            stroke = lv_slider_get_value(ui_homestrokeslider);
            SendCommand(STROKE, stroke, OSSM_ID);
        }
        char stroke_v[7];
        dtostrf(stroke, 6, 0, stroke_v);
        lv_label_set_text(ui_homestrokevalue, stroke_v);

        // Encoder4 Senstation
        if(lv_slider_is_dragged(ui_homesensationslider) == false){
          if (encoder4.getCount() != sensationenc){
            lv_slider_set_value(ui_homesensationslider, sensation, LV_ANIM_OFF);
            if(encoder4.getCount() <= (Encoder_MAP/2*-1)){
              encoder4.setCount((Encoder_MAP/2*-1));
            } else if (encoder4.getCount() >= (Encoder_MAP/2)){
              encoder4.setCount((Encoder_MAP/2));
            } 
            sensationenc = encoder4.getCount();
            sensation = fscale((Encoder_MAP/2*-1), (Encoder_MAP/2), -100, 100, sensationenc, 0);
            SendCommand(SENSATION, sensation, OSSM_ID);
          }
        } else if(lv_slider_get_value(ui_homesensationslider) != sensation){
            sensationenc =  fscale(-100, 100, (Encoder_MAP/2*-1), (Encoder_MAP/2), sensation, 0);
            encoder4.setCount(sensationenc);
            sensation = lv_slider_get_value(ui_homesensationslider);
            SendCommand(SENSATION, sensation, OSSM_ID);
        }

        // Eject button

        if (Eject_paired) {
          lv_label_set_text(ui_HomeButtonLText, "Creampie");
          lv_obj_clear_state(ui_HomeButtonL, LV_STATE_DISABLED);

          if(EJECT_On == true){
            lv_obj_add_state(ui_HomeButtonL, LV_STATE_CHECKED);
          } else {
            lv_obj_clear_state(ui_HomeButtonL, LV_STATE_CHECKED);
          }
        } else {
          lv_label_set_text(ui_HomeButtonLText, "-");
          lv_obj_add_state(ui_HomeButtonL, LV_STATE_DISABLED);
        }

        if(click2_short_waspressed == true){
         lv_event_send(ui_HomeButtonL, LV_EVENT_CLICKED, NULL);
        } else if(mxclick_short_waspressed == true){
         lv_event_send(ui_HomeButtonM, LV_EVENT_CLICKED, NULL);
        } else if(click3_short_waspressed == true){
         lv_event_send(ui_HomeButtonR, LV_EVENT_CLICKED, NULL);
        }
        

      }
      break;

      case ST_UI_MENUE:
      {
        if(touch_home == true){
          touch_disabled = true;
        }
        if(encoder4.getCount() > encoder4_enc + 2){
          LogDebug("next");
          lv_group_focus_next(ui_g_menue);
          encoder4_enc = encoder4.getCount();
        } else if(encoder4.getCount() < encoder4_enc -2){
          lv_group_focus_prev(ui_g_menue);
          LogDebug("Preview");
          encoder4_enc = encoder4.getCount();
        }

        if(click2_short_waspressed == true){
         lv_event_send(ui_MenueButtonL, LV_EVENT_CLICKED, NULL);
        } else if(mxclick_short_waspressed == true){
         lv_event_send(ui_MenueButtonM, LV_EVENT_CLICKED, NULL);
        } else if(click3_short_waspressed == true){
         lv_event_send(lv_group_get_focused(ui_g_menue), LV_EVENT_CLICKED, NULL);
        }
      }
      break;

      case ST_UI_PATTERN:
      {
        if(touch_home == true){
          touch_disabled = true;
        }
        if(encoder4.getCount() > encoder4_enc + 2){
          LogDebug("next");
          uint32_t t = LV_KEY_DOWN;
          lv_event_send(ui_PatternS, LV_EVENT_KEY, &t);
          encoder4_enc = encoder4.getCount();
        } else if(encoder4.getCount() < encoder4_enc -2){
          uint32_t t = LV_KEY_UP;
          lv_event_send(ui_PatternS, LV_EVENT_KEY, &t);
          LogDebug("Preview");
          encoder4_enc = encoder4.getCount();
        }
         if(click2_short_waspressed == true){
         lv_event_send(ui_PatternButtonL, LV_EVENT_CLICKED, NULL);
        } else if(mxclick_short_waspressed == true){
         lv_event_send(ui_PatternButtonM, LV_EVENT_CLICKED, NULL);
        } else if(click3_short_waspressed == true){
         lv_event_send(ui_PatternButtonR, LV_EVENT_CLICKED, NULL);
        }
      }
      break;

      case ST_UI_Torqe:
      {
        if(touch_home == true){
          touch_disabled = true;
        }
        // Encoder 1 Torqe Out
        if(lv_slider_is_dragged(ui_outtroqeslider) == false){
          if (encoder1.getCount() != torqe_f_enc){
            lv_slider_set_value(ui_outtroqeslider, torqe_f, LV_ANIM_OFF);
            if(encoder1.getCount() <= 0){
              encoder1.setCount(0);
            } else if (encoder1.getCount() >= Encoder_MAP){
              encoder1.setCount(Encoder_MAP);
            } 
            torqe_f_enc = encoder1.getCount();
            torqe_f = fscale(0, Encoder_MAP, 50, 200, torqe_f_enc, 0);
            SendCommand(TORQE_F, torqe_f, OSSM_ID);
          }
        } else if(lv_slider_get_value(ui_outtroqeslider) != torqe_f){
            torqe_f_enc = fscale(50, 200, 0, Encoder_MAP, torqe_f, 0);
            encoder1.setCount(torqe_f_enc);
            torqe_f = lv_slider_get_value(ui_outtroqeslider);
            SendCommand(TORQE_F, torqe_f, OSSM_ID);
        }
        char torqe_f_v[7];
        dtostrf((torqe_f*-1), 6, 0, torqe_f_v);
        lv_label_set_text(ui_outtroqevalue, torqe_f_v);

        // Encoder 4 Torqe IN
        if(lv_slider_is_dragged(ui_introqeslider) == false){
          if (encoder4.getCount() != torqe_r_enc){
            lv_slider_set_value(ui_introqeslider, torqe_r, LV_ANIM_OFF);
            if(encoder4.getCount() <= 0){
              encoder4.setCount(0);
            } else if (encoder4.getCount() >= Encoder_MAP){
              encoder4.setCount(Encoder_MAP);
            } 
            torqe_r_enc = encoder4.getCount();
            torqe_r = fscale(0, Encoder_MAP, 20, 200, torqe_r_enc, 0);
            SendCommand(TORQE_R, torqe_r, OSSM_ID);
          }
        } else if(lv_slider_get_value(ui_introqeslider) != torqe_r){
            torqe_r_enc = fscale(20, 200, 0, Encoder_MAP, torqe_r, 0);
            encoder4.setCount(torqe_r_enc);
            torqe_r = lv_slider_get_value(ui_introqeslider);
            SendCommand(TORQE_R, torqe_r, OSSM_ID);
        }
        char torqe_r_v[7];
        dtostrf(torqe_r, 6, 0, torqe_r_v);
        lv_label_set_text(ui_introqevalue, torqe_r_v);

         if(click2_short_waspressed == true){
         lv_event_send(ui_TorqeButtonL, LV_EVENT_CLICKED, NULL);
        } else if(mxclick_short_waspressed == true){
         lv_event_send(ui_TorqeButtonM, LV_EVENT_CLICKED, NULL);
        } else if(click3_short_waspressed == true){
         lv_event_send(ui_TorqeButtonR, LV_EVENT_CLICKED, NULL);
        }
      }
      break;

      case ST_UI_EJECTSETTINGS:
      {
        if(touch_home == true){
          touch_disabled = true;
        }

        if (Eject_paired) {
          lv_obj_clear_state(ui_EJECTButtonL, LV_STATE_DISABLED);
          lv_label_set_text(ui_EJECTButtonLText, "Test");

          if(EJECT_On == true){
            lv_obj_add_state(ui_EJECTButtonL, LV_STATE_CHECKED);
          } else {
            lv_obj_clear_state(ui_EJECTButtonL, LV_STATE_CHECKED);
          }

          bool reverse = lv_obj_has_state(ui_ejectreverse, LV_STATE_CHECKED);
          if (cum_reverse != reverse){
            cum_reverse = reverse;
            SendCommand(CUMREVERSE, cum_reverse ? 1 : 0, EJECT_ID);
          }
        } else {
          lv_label_set_text(ui_EJECTButtonLText, "-");
          lv_obj_add_state(ui_EJECTButtonL, LV_STATE_DISABLED);
        }

        // Encoder1 Cum Time
        if(lv_slider_is_dragged(ui_ejecttimeslider) == false){
          if (encoder1.getCount() != cum_time_enc){
            lv_slider_set_value(ui_ejecttimeslider, cum_time, LV_ANIM_OFF);
            if(encoder1.getCount() <= 0){
              encoder1.setCount(0);
            } else if (encoder1.getCount() >= Encoder_MAP){
              encoder1.setCount(Encoder_MAP);
            } 
            if (cum_time_enc > encoder1.getCount()) {
              cum_time = max(cum_time-1, (float) 1);
            } else if (cum_time_enc < encoder1.getCount()) {
              cum_time = min(cum_time+1, cum_maxtime);
            }
            cum_time_enc = encoder1.getCount();
            SendCommand(CUMTIME, cum_time, EJECT_ID);
          }
        } else if(lv_slider_get_value(ui_ejecttimeslider) != cum_time){
          cum_time_enc = fscale(1, cum_maxtime, 0, Encoder_MAP, cum_time, 0);
          encoder1.setCount(cum_time_enc);
          cum_time = lv_slider_get_value(ui_ejecttimeslider);
          SendCommand(CUMTIME, cum_time, EJECT_ID);
        }
        char cum_time_v[7];
        dtostrf(cum_time, 6, 0, cum_time_v);
        lv_label_set_text(ui_ejecttimevalue, cum_time_v);

        // Encoder2 Cum Size
        if(lv_slider_is_dragged(ui_ejectsizeslider) == false){
          if (encoder2.getCount() != cum_size_enc){
            lv_slider_set_value(ui_ejectsizeslider, cum_size, LV_ANIM_OFF);
            if(encoder2.getCount() <= 0){
              encoder2.setCount(0);
            } else if (encoder2.getCount() >= Encoder_MAP){
              encoder2.setCount(Encoder_MAP);
            } 
            cum_size_enc = encoder2.getCount();
            cum_size = fscale(0, Encoder_MAP, 1, cum_maxsize, cum_size_enc, 0);
            SendCommand(CUMSIZE, cum_size, EJECT_ID);
          }
        } else if(lv_slider_get_value(ui_ejectsizeslider) != cum_size){
          cum_size_enc = fscale(1, cum_maxsize, 0, Encoder_MAP, cum_size, 0);
          encoder2.setCount(cum_size_enc);
          cum_size = lv_slider_get_value(ui_ejectsizeslider);
          SendCommand(CUMSIZE, cum_size, EJECT_ID);
        }
        char cum_size_v[7];
        dtostrf(cum_size, 6, 0, cum_size_v);
        lv_label_set_text(ui_ejectsizevalue, cum_size_v);

        // Encoder3 Cum Force
        if(lv_slider_is_dragged(ui_ejectaccelslider) == false){
          if (encoder3.getCount() != cum_accel_enc){
            lv_slider_set_value(ui_ejectaccelslider, cum_accel, LV_ANIM_OFF);
            if(encoder3.getCount() <= (Encoder_MAP/2*-1)){
              encoder3.setCount((Encoder_MAP/2*-1));
            } else if (encoder3.getCount() >= (Encoder_MAP/2)){
              encoder3.setCount((Encoder_MAP/2));
            } 
            cum_accel_enc = encoder3.getCount();
            cum_accel = fscale(0, Encoder_MAP, 1, cum_maxaccel, cum_accel_enc, 0);
            SendCommand(CUMACCEL, cum_accel, EJECT_ID);
          }
        } else if(lv_slider_get_value(ui_ejectaccelslider) != cum_accel){
            cum_accel_enc = fscale(1, cum_maxaccel, 0, Encoder_MAP, cum_accel, 0);
            encoder3.setCount(cum_accel_enc);
            cum_accel = lv_slider_get_value(ui_ejectaccelslider);
            SendCommand(CUMACCEL, cum_accel, EJECT_ID);
        }
        char cum_accel_v[7];
        dtostrf(cum_accel, 6, 0, cum_accel_v);
        lv_label_set_text(ui_ejectaccelvalue, cum_accel_v);


        if(click2_short_waspressed == true){
         lv_event_send(ui_EJECTButtonL, LV_EVENT_CLICKED, NULL);
        } else if(mxclick_short_waspressed == true){
         lv_event_send(ui_EJECTButtonM, LV_EVENT_CLICKED, NULL);
        } else if(click3_short_waspressed == true){
         
        }
      }
      break;

      case ST_UI_SETTINGS:
      {
        touch_disabled = false;
        if(encoder4.getCount() > encoder4_enc + 2){
          LogDebug("next");
          lv_group_focus_next(ui_g_settings);
          encoder4_enc = encoder4.getCount();
        } else if(encoder4.getCount() < encoder4_enc -2){
          lv_group_focus_prev(ui_g_settings);
          LogDebug("Preview");
          encoder4_enc = encoder4.getCount();
        }

        if(click2_short_waspressed == true){
         lv_event_send(ui_MenueButtonL, LV_EVENT_CLICKED, NULL);
        } else if(mxclick_short_waspressed == true){
         lv_event_send(ui_MenueButtonM, LV_EVENT_CLICKED, NULL);
        } else if(click3_short_waspressed == true){
         lv_event_send(ui_EJECTButtonR, LV_EVENT_CLICKED, NULL);
        }
      }
      break;

     }
     mxclick_long_waspressed = false;
     mxclick_short_waspressed = false;
     click2_short_waspressed = false;
     click3_short_waspressed = false;

  delay(5);
}

void espNowRemoteTask(void *pvParameters)
{
  for(;;){
    if(Ossm_paired){
      outgoingcontrol.esp_command = HEARTBEAT;
      outgoingcontrol.esp_heartbeat = true;
      outgoingcontrol.esp_target = OSSM_ID;
      esp_err_t result = esp_now_send(OSSM_Address, (uint8_t *) &outgoingcontrol, sizeof(outgoingcontrol));
    }
    if(Eject_paired){
      outgoingcontrol.esp_command = HEARTBEAT;
      outgoingcontrol.esp_heartbeat = true;
      outgoingcontrol.esp_target = EJECT_ID;
      esp_err_t result = esp_now_send(EJECT_Address, (uint8_t *) &outgoingcontrol, sizeof(outgoingcontrol));
    }
    vTaskDelay(HEARTBEAT_INTERVAL);
  }
}

void vibrate(){
    if(vibrate_mode == true){
    M5.Axp.SetLDOEnable(3,true);
    vTaskDelay(300);
    M5.Axp.SetLDOEnable(3,false);
    }
}

void mxclick() {
  vibrate();
  mxclick_short_waspressed = true;
} 

void mxlong(){
  vibrate();
  mxclick_long_waspressed = true;
} 

void click2() {
  vibrate();
  click2_short_waspressed = true;
} // click1

void click3() {
  vibrate();
  click3_short_waspressed = true;
} // click1