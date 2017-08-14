#include "CEC_Device.h"
#include <MsTimer2.h>

#define IN_LINE 2
#define OUT_LINE 3
//#define HPD_LINE 10
#define LEFT_KEY 4
#define DOWN_KEY 5
#define ENTER_KEY 6
#define UP_KEY 7
#define RIGHT_KEY 8
#define MENU_KEY 9
#define DEL_KEY 10
#define C1_KEY 11

// ugly macro to do debug printing in the OnReceive method
#define report(X) do { DbgPrint("report " #X "\n"); report ## X (); } while (0)

#define phy1 ((_physicalAddress >> 8) & 0xFF)
#define phy2 ((_physicalAddress >> 0) & 0xFF)

class MyCEC: public CEC_Device {
  public:
    MyCEC(int physAddr): CEC_Device(physAddr,IN_LINE,OUT_LINE) { }
    
    void reportPhysAddr()    { unsigned char frame[4] = { 0x84, phy1, phy2, 0x04 }; TransmitFrame(0x0F,frame,sizeof(frame)); } // report physical address
    void reportStreamState() { unsigned char frame[3] = { 0x82, phy1, phy2 };       TransmitFrame(0x0F,frame,sizeof(frame)); } // report stream state (playing)
    
    void reportPowerState()  { unsigned char frame[2] = { 0x90, 0x00 };             TransmitFrame(0x00,frame,sizeof(frame)); } // report power state (on)
    void reportCECVersion()  { unsigned char frame[2] = { 0x9E, 0x04 };             TransmitFrame(0x00,frame,sizeof(frame)); } // report CEC version (v1.3a)
    
    void reportOSDName()     { unsigned char frame[5] = { 0x47, 'H','T','P','C' };  TransmitFrame(0x00,frame,sizeof(frame)); } // FIXME: name hardcoded
    void reportVendorID()    { unsigned char frame[4] = { 0x87, 0x00, 0xF1, 0x0E }; TransmitFrame(0x00,frame,sizeof(frame)); } // report fake vendor ID
    // TODO: implement menu status query (0x8D) and report (0x8E,0x00)
    

    /*
    void handleKey(unsigned char key) {
      switch (key) {
        case 0x00: Keyboard.press(KEY_RETURN); break;
        case 0x01: Keyboard.press(KEY_UP_ARROW); break;
        case 0x02: Keyboard.press(KEY_DOWN_ARROW); break;
        case 0x03: Keyboard.press(KEY_LEFT_ARROW); break;
        case 0x04: Keyboard.press(KEY_RIGHT_ARROW); break;
        case 0x0D: Keyboard.press(KEY_ESC); break;
        case 0x4B: Keyboard.press(KEY_PAGE_DOWN); break;
        case 0x4C: Keyboard.press(KEY_PAGE_UP); break;
        case 0x53: Keyboard.press(KEY_HOME); break;
      }
    }
    */
        
    void OnReceive(int source, int dest, unsigned char* buffer, int count) {
      if (count == 0) return;
      switch (buffer[0]) {
        
        case 0x36: DbgPrint("standby\n"); break;
        
        case 0x83: report(PhysAddr); break;
        case 0x86: if (buffer[1] == phy1 && buffer[2] == phy2)
                   report(StreamState); break;
        
        case 0x8F: report(PowerState); break;
        case 0x9F: report(CECVersion); break;  
        
        case 0x46: report(OSDName);    break;
        case 0x8C: report(VendorID);   break;
        
       // case 0x44: handleKey(buffer[1]); break;
       // case 0x45: Keyboard.releaseAll(); break;
        
        default: CEC_Device::OnReceive(source,dest,buffer,count); break;
      }
    }
};

// TODO: set physical address via serial (or even DDC?)

// Note: this does not need to correspond to the physical address (i.e. port number)
// where the Arduino is connected - in fact, it _should_ be a different port, namely
// the one where the PC to be controlled is connected. Basically, it is the address
// of the port where the CEC-less source device is plugged in.
MyCEC device(0x0008);
unsigned char CommandSeq[8]={0x00, 0x02, 0x02, 0x00, 0x02, 0x00, 0x09, 0x09};
int CommandSeqFlag = 0;
char sw_stat[2][8] = {0};

///////  Key操作関連の関数　///////////
void Get_SW_Status(){
  // get swich status
  sw_stat[1][0] = digitalRead(ENTER_KEY);
  sw_stat[1][1] = digitalRead(UP_KEY);
  sw_stat[1][2] = digitalRead(DOWN_KEY);
  sw_stat[1][3] = digitalRead(LEFT_KEY);
  sw_stat[1][4] = digitalRead(RIGHT_KEY);
  sw_stat[1][5] = digitalRead(MENU_KEY);
  sw_stat[1][6] = digitalRead(C1_KEY);
  sw_stat[1][7] = digitalRead(DEL_KEY);
}

int Select_SW_Res(){
  int sw_res_pattern = 1;
  int sw_dir = 0;
  // priority ENTER > UP ... > C1 > DEL
  for(int i=0; i<8; i++, sw_res_pattern++){
    sw_dir = sw_stat[0][i] - sw_stat[1][i];
    if(sw_dir){
      break;
    }
  }
  return sw_res_pattern * sw_dir;
}

//////////////////////////////////////

void setup()
{
  // setup Hotplug pin
  // pinMode(HPD_LINE,INPUT);

  // setup Key
  pinMode(UP_KEY, INPUT_PULLUP);
  pinMode(DOWN_KEY, INPUT_PULLUP);
  pinMode(LEFT_KEY, INPUT_PULLUP);
  pinMode(RIGHT_KEY, INPUT_PULLUP);
  pinMode(ENTER_KEY, INPUT_PULLUP);
  pinMode(MENU_KEY, INPUT_PULLUP);
  pinMode(C1_KEY, INPUT_PULLUP);
  pinMode(DEL_KEY, INPUT_PULLUP);
 
  Serial.begin(115200);
  //Keyboard.begin();
  
  //device.MonitorMode = true;
  //device.Promiscuous = true;
  device.Initialize(CEC_LogicalDevice::CDT_PLAYBACK_DEVICE);

  MsTimer2::set(600, commandSeq);
}


void commandSeq()
{
  unsigned char frame[2] = { 0x44, 0x00 };
  frame[1] = CommandSeq[CommandSeqFlag];
  bool retState = device.TransmitFrame(0x04, frame, sizeof(frame));
  //Serial.print("commandSeq:");Serial.println(CommandSeq[CommandSeqFlag]);
  device.Run();

  CommandSeqFlag ++;
  if(CommandSeqFlag == 8){
    MsTimer2::stop();
    CommandSeqFlag = 0;
  }
  
}


void loop()
{
  int sw_respons = 0;
  
  // get swich status
  Get_SW_Status();

  // Determin the reaction
  sw_respons = Select_SW_Res();

  // sw_stat update
  for(int i=0; i<8; i++){
    sw_stat[0][i] = sw_stat[1][i];
  }

  switch (sw_respons){
    case  1:
      Serial.println("ENTER");
      {
        unsigned char frame[2] = { 0x44, 0x00 };
        bool retState = device.TransmitFrame(0x04, frame, sizeof(frame));
      }
      break;
    case  2:
      Serial.println("UP\n");
       {
        unsigned char frame[2] = { 0x44, 0x01 };
        bool retState = device.TransmitFrame(0x04, frame, sizeof(frame));
      }
      break;
    case  3:
      Serial.println("DOWN\n");
       {
        unsigned char frame[2] = { 0x44, 0x02 };
        bool retState = device.TransmitFrame(0x04, frame, sizeof(frame));
      }
      break;
    case  4:
      Serial.println("LEFT\n");
      {
        unsigned char frame[2] = { 0x44, 0x03 };
        bool retState = device.TransmitFrame(0x04, frame, sizeof(frame));
      }
      break;
    case  5:
      Serial.println("RIGHT\n");
       {
        unsigned char frame[2] = { 0x44, 0x04 };
        bool retState = device.TransmitFrame(0x04, frame, sizeof(frame));
      }
      break;
    case  6:
      Serial.println("MENU\n");
       {
        unsigned char frame[2] = { 0x44, 0x09 };
        bool retState = device.TransmitFrame(0x04, frame, sizeof(frame));
      }
      break;
    case  7:
      Serial.println("C1\n");
      {
        //シーケンスの最初のコマンドはここで実行
        unsigned char frame[2] = { 0x44, 0x09 };
        bool retState = device.TransmitFrame(0x04, frame, sizeof(frame));
      }
      break;
    case  8:
      Serial.println("DEL\n");
      {
        unsigned char frame[2] = { 0x44, 0x0A };
        bool retState = device.TransmitFrame(0x04, frame, sizeof(frame));
      }
      break;
    case -1:
      Serial.println("ENTER release\n");
      {
        unsigned char frame[1] = { 0x45 };
        bool retState = device.TransmitFrame(0x04, frame, sizeof(frame));
      }
      break;
    case -2:
      Serial.println("UP release\n");
      {
        unsigned char frame[1] = { 0x45 };
        bool retState = device.TransmitFrame(0x04, frame, sizeof(frame));
      }
      break;
    case -3:
      Serial.println("DOWN release\n");
      {
        unsigned char frame[1] = { 0x45 };
        bool retState = device.TransmitFrame(0x04, frame, sizeof(frame));
      }
      break;
    case -4:
      Serial.println("LEFT release\n");
      {
        unsigned char frame[1] = { 0x45 };
        bool retState = device.TransmitFrame(0x04, frame, sizeof(frame));
      }
      break;
    case -5:
      Serial.println("RIGHT release\n");
      {
        unsigned char frame[1] = { 0x45 };
        bool retState = device.TransmitFrame(0x04, frame, sizeof(frame));
      }
      break;
    case -6:
      Serial.println("MENU release\n");
      {
        unsigned char frame[1] = { 0x45 };
        bool retState = device.TransmitFrame(0x04, frame, sizeof(frame));
      }
      break;
    case -7:
      Serial.println("C1 release\n");
      MsTimer2::start();
      break;
    case -8:
      Serial.println("DEL release\n");
      {
        unsigned char frame[1] = { 0x45 };
        bool retState = device.TransmitFrame(0x04, frame, sizeof(frame));
      }
      break;
  }
   
  device.Run();
}

