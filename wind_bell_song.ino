/*
「揺れると曲を奏でる風鈴」
・揺れると風鈴調の曲が数秒流れるよ
・短冊を付け替えると曲が変わるよ

○揺れると曲が流れる仕組み
①加速度センサKXR94-2050で定期的にxyzの加速度を取得
②(xyzの直近20回分の差分の絶対値/20 > 閾値)だったらDFRobotDFPlayerMiniで5秒曲を流す

○短冊を付け替えると曲が変わる仕組み
①RFIDリーダRFID-RC522で短冊についているNFCタグを読み取り
②NFCタグの値によって流す曲を切り替える
*/

#include "SPI.h"
#include "MFRC522.h"
#include "Arduino.h";
#include "DFRobotDFPlayerMini.h";

// 直近20回分の値を格納する場所
float preAccelValues[20];
// 直近20回分の値を格納するためのインデックス
int accelItr = 0;
// 「揺れた」と判定するための差分の閾値
int ACCELL_DIFF_THRESHOLD = 130;

// RFIDリーダのピンアサイン・MFRCインスタンス生成
#define RST_PIN         9 
#define SS_PIN          10
MFRC522 mfrc522(SS_PIN, RST_PIN);
// NFCのUIDと曲順をリンクさせるための構造体
typedef struct {
  String uid;
  uint8_t numberOfPlaylist;
} dataDictionary;
// {NFCのUID, 曲番号}を格納した辞書型
const dataDictionary songDictionaryArr[]{
  {"04 84 4B CA 1E 18 90", 7},  //夏祭り
  {"04 6E 4B CA 1E 18 90", 5},  //打上花火
  {"04 52 4B CA 1E 18 90", 3},  //夏色
  {"04 6D 4B CA 1E 18 90", 1},  // dashi yogi badagga
  {"04 83 4B CA 1E 18 90", 2}  // hebyone yoin
};
int sizeOfSongDictionaryArr = 5;
// 今の曲番号
int currentNumberOfPlaylist = 7;

// DFRobotDFPlayerMiniの定義
DFRobotDFPlayerMini myDFPlayer;

void setup() {
  // シリアルの準備
  Serial.begin(115200);
  while (!Serial);  // シリアル準備できるまで何もしない

  // DFRobotDFPlayerMini用のシリアルの準備
  Serial1.begin(9600);

  // DFRobotDFPlayerMiniの準備
  Serial.println(F("Initializing DFPlayer ... (May take 3~5 seconds)"));
  if (!myDFPlayer.begin(Serial1)) {
    // 2秒以内に初期化できなかった場合はエラーメッセージを表示
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));

    delay(3000);

    if(myDFPlayer.available()){
      Serial.println(F("available"));
      printDetail(myDFPlayer.readType(), myDFPlayer.read());
    }else{
      Serial.println(F("not available"));
    }
    while (true) {
      delay(0); // Code to compatible with ESP8266 watch dog.
    }
  }
  Serial.println(F("DFPlayer Mini online."));
  myDFPlayer.volume(20);  //ボリューム設定 0-30

  // NFCの読み取り準備
  SPI.begin();
  mfrc522.PCD_Init();
  delay(4);
  mfrc522.PCD_DumpVersionToSerial();  // MFRC522 Card Readerの詳細を表示(サンプルコードDumpInfoより)
  Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks...")); // (サンプルコードDumpInfoより)
}

void loop() {
  // NFC読み取り
  String currentUID = readUID();
  // 定義しているUIDと一致したら、対応する曲に変更する
  for(int i=0; i<sizeOfSongDictionaryArr; i++){
    if(currentUID.equalsIgnoreCase(songDictionaryArr[i].uid)){
      currentNumberOfPlaylist = songDictionaryArr[i].numberOfPlaylist;
      Serial.println(songDictionaryArr[i].numberOfPlaylist);
      break;
    }
  }

  // 加速度センサの値読み取り
  int x = analogRead(0);
  int y = analogRead(1);
  int z = analogRead(2);

  Serial.print(x);
  Serial.print(",");
  Serial.print(y);
  Serial.print(",");
  Serial.println(z);

  // x, y, zの数値の2乗を合算
  float currentValueSq = sq(x) + sq(y) + sq(z);
  // ↑の平方根を出してノルムにする
  float currentValueSqrt = sqrt(currentValueSq);

  // 初回ループ以外揺れ検知する
  if (accelItr > 0) {
    // 直近20回分の平均値
    float preAvg = sum(preAccelValues, accelItr) / accelItr;
    // 今回の値と直近20回分の平均値の差
    float diff = currentValueSqrt - preAvg;

    // Serial.print("current, preArg");
    // Serial.print(currentValueSqrt);
    // Serial.print(",");
    // Serial.println(preAvg);

    // (今回の値 - 直近20回分の平均値)の絶対値 > 閾値だったら揺れていると判断
    if ((abs(diff) > ACCELL_DIFF_THRESHOLD) && (currentValueSqrt != float(0.0))) {

      // しばらく検知しないように配列を0にする
      accelItr = 0;
      for(int i = 0; i < 20; i++){
        preAccelValues[i] = 0;
      }

      // 曲を流し、その分だけループを止める
      myDFPlayer.play(currentNumberOfPlaylist);
      Serial.println("sway!!!");
      delay(10000);
    }

    // // プレイヤーの状態を出力
    // if(myDFPlayer.available()){
    //   printDetail(myDFPlayer.readType(), myDFPlayer.read());
    // }

  }

  // 直近20回分を格納する配列に今回の値を格納
  if (accelItr < 20) {
    preAccelValues[accelItr] = currentValueSqrt;
    accelItr++;
  } else {
    preAccelValues[0] = preAccelValues[1];
    preAccelValues[1] = preAccelValues[2];
    preAccelValues[2] = preAccelValues[3];
    preAccelValues[3] = preAccelValues[4];
    preAccelValues[4] = preAccelValues[5];
    preAccelValues[5] = preAccelValues[6];
    preAccelValues[6] = preAccelValues[7];
    preAccelValues[7] = preAccelValues[8];
    preAccelValues[8] = preAccelValues[9];
    preAccelValues[9] = preAccelValues[10];
    preAccelValues[10] = preAccelValues[11];
    preAccelValues[11] = preAccelValues[12];
    preAccelValues[12] = preAccelValues[13];
    preAccelValues[13] = preAccelValues[14];
    preAccelValues[14] = preAccelValues[15];
    preAccelValues[15] = preAccelValues[16];
    preAccelValues[16] = preAccelValues[17];
    preAccelValues[17] = preAccelValues[18];
    preAccelValues[18] = preAccelValues[19];
    preAccelValues[19] = currentValueSqrt;
  }

  delay(200);
}

// 配列内の合計を出す関数
int sum(float list[], int size) {
  int sum = 0;

  for (int i = 0; i < size; i = i + 1) {
    sum += list[i];
  }
  return sum;
}

// RFIDリーダでNFCのUIDを読み取る
String readUID(){
  // 新しいNFCが読み取られなければreturn
	if (!mfrc522.PICC_IsNewCardPresent()) {
		return "";
	}

  // シリアル番号が読み取れなければreturn
  if (!mfrc522.PICC_ReadCardSerial()) {
		return "";
	}

  // UIDの読み取り
  String strBuf[mfrc522.uid.size];
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    strBuf[i] =  String(mfrc522.uid.uidByte[i], HEX);
    if(strBuf[i].length() == 1){  // 1桁の場合は先頭に0を追加
      strBuf[i] = "0" + strBuf[i];
    }
  }

  String strUID = strBuf[0] + " " + strBuf[1] + " " + strBuf[2] + " " + strBuf[3] + " " + strBuf[4] + " " + strBuf[5] + " " + strBuf[6];

  return strUID;
}

// Helper routine to dump a byte array as hex values to Serial.(サンプルコードReadUidMultiReaderより)
void dump_byte_array(byte *buffer, byte bufferSize) {
    for (byte i = 0; i < bufferSize; i++) {
        Serial.print(buffer[i] < 0x10 ? " 0" : " ");
        Serial.print(buffer[i], HEX);
    }
}

// DFRobotDFPlayerMiniの状態をシリアルに出すメソッド(サンプルコードFullFunctionより)
void printDetail(uint8_t type, int value) {
  switch (type) {
    case TimeOut:
      Serial.println(F("Time Out!"));
      break;
    case WrongStack:
      Serial.println(F("Stack Wrong!"));
      break;
    case DFPlayerCardInserted:
      Serial.println(F("Card Inserted!"));
      break;
    case DFPlayerCardRemoved:
      Serial.println(F("Card Removed!"));
      break;
    case DFPlayerCardOnline:
      Serial.println(F("Card Online!"));
      break;
    case DFPlayerUSBInserted:
      Serial.println("USB Inserted!");
      break;
    case DFPlayerUSBRemoved:
      Serial.println("USB Removed!");
      break;
    case DFPlayerPlayFinished:
      Serial.print(F("Number:"));
      Serial.print(value);
      Serial.println(F(" Play Finished!"));
      break;
    case DFPlayerError:
      Serial.print(F("DFPlayerError:"));
      switch (value) {
        case Busy:
          Serial.println(F("Card not found"));
          break;
        case Sleeping:
          Serial.println(F("Sleeping"));
          break;
        case SerialWrongStack:
          Serial.println(F("Get Wrong Stack"));
          break;
        case CheckSumNotMatch:
          Serial.println(F("Check Sum Not Match"));
          break;
        case FileIndexOut:
          Serial.println(F("File Index Out of Bound"));
          break;
        case FileMismatch:
          Serial.println(F("Cannot Find File"));
          break;
        case Advertise:
          Serial.println(F("In Advertise"));
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }

}