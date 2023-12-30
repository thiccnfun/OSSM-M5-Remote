
#define OSSM 1

#define CUM 2


#define OSSM_ID  1 //OSSM_ID Default can be changed with M5 Remote in the Future will be Saved in EPROOM
#define M5_ID 99 //M5_ID Default can be changed with M5 Remote in the Future will be Saved in EPROOM
#define EJECT_ID 69
#define BATTERY_CHARGE_CURRENT kCHG_100mA  // Charge current, must be one of AXP192::CHGCurrent



#define V2  //Define V1 or V2 PCB Version

//PIn Definitons Encoders V1:
#ifdef V1
#define ENC_1_CLK 25
#define ENC_1_DT 26

#define ENC_2_CLK 13
#define ENC_2_DT 14

#define ENC_3_CLK 33
#define ENC_3_DT 32

#define ENC_4_CLK 19
#define ENC_4_DT 27
OneButton Button1(35, false);
OneButton Button2(18, false);
OneButton Button3(38, false);
#endif

//PIn Definitons Encoders V2:

#ifdef V2
#define ENC_1_CLK 25
#define ENC_1_DT 26

#define ENC_2_CLK 13
#define ENC_2_DT 14

#define ENC_3_CLK 33
#define ENC_3_DT 32

#define ENC_4_CLK 19
#define ENC_4_DT 27
OneButton Button1(35, false); //MX Button
OneButton Button2(36, false); //Encoder Left
OneButton Button3(34, false, true); // Encoder Right
#endif

#define Encoder_MAP 144

#define LV_HOR_RES_MAX 320
#define LV_VER_RES_MAX 240

// Color 
#define TextColor  TFT_PURPLE
#define BgColor    TFT_WHITE
#define FrontColor TFT_PURPLE
#define HighlightColor TFT_BLACK

