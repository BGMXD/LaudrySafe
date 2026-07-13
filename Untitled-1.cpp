#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <WebServer.h>
#include <Adafruit_VL53L0X.h>

// --- Konfigurasi WiFi ---
const char* ssid = "Radot.Net";
const char* password = "radothome";

// --- Batas Bahaya ---
const float BATAS_SUHU_KRITIS = 28.1; 

// --- Konfigurasi Server ---
WiFiServer serverTCP(8080); 
WiFiClient vbClient;     
WebServer serverWeb(80);    

// --- Konfigurasi Pin Sensor Lainnya ---
#define VIB_PIN 15       
#define DHTPIN 4         
#define DHTTYPE DHT22    

// --- KONFIGURASI DUAL I2C (OLED vs VL53L0X) ---
#define SENSOR_SDA 16    // Pin SDA khusus untuk VL53L0X
#define SENSOR_SCL 17    // Pin SCL khusus untuk VL53L0X

// Membuat jalur I2C kedua (I2C1) khusus untuk sensor jarak
TwoWire I2C_VL53L0X = TwoWire(1); 

// --- Konfigurasi OLED ---
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 32 
#define OLED_RESET    -1 

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHT dht(DHTPIN, DHTTYPE);
Adafruit_VL53L0X lox = Adafruit_VL53L0X(); 

// --- Variabel Logika Sensor ---
int hitungAnomali = 0;        
int hitungAman = 0;           
bool statusAnomali = false;   
float suhu = 0.0;
unsigned long waktuKedipTerakhir = 0;
bool statusKedipLayar = false;

// --- Variabel Hasil Akhir Jarak ---
int jarakRataRata = 0; 

// ==========================================
// ASET IKON OLED (BITMAP 8x8 PIXEL)
// ==========================================
const unsigned char icon_wifi_ok[] PROGMEM = { 0x00, 0x3c, 0x42, 0x81, 0x00, 0x24, 0x00, 0x18 };
const unsigned char icon_wifi_off[] PROGMEM = { 0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81 };
const unsigned char icon_vb_ok[] PROGMEM = { 0x00, 0xac, 0xaa, 0xac, 0x4a, 0x4c, 0x00, 0x00 };
const unsigned char icon_vb_off[] PROGMEM = { 0x81, 0x6c, 0x3a, 0x1c, 0x0a, 0x4c, 0x80, 0x00 };
const unsigned char icon_api[] PROGMEM = { 0x08, 0x1c, 0x2a, 0x5d, 0x55, 0x49, 0x22, 0x0c };
const unsigned char icon_daun[] PROGMEM = { 0x02, 0x06, 0x0e, 0x1c, 0x38, 0x70, 0x20, 0x00 };

// ==========================================
// DESAIN HALAMAN WEB SMARTPHONE
// ==========================================
const char html_halaman_web[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>LaundrySafe IoT</title>
  <style>
    body { background-color: #1e1e1e; color: #ffffff; font-family: 'Segoe UI', Tahoma, sans-serif; text-align: center; margin: 0; padding: 20px; }
    .card { background-color: #2d2d30; border-radius: 12px; padding: 30px 20px; margin: 5vh auto; max-width: 400px; box-shadow: 0 8px 16px rgba(0,0,0,0.5); }
    h1 { font-size: 22px; color: #007acc; margin-top: 0; margin-bottom: 20px; letter-spacing: 1px; }
    .temp { font-size: 55px; font-weight: bold; margin: 10px 0; color: #ffc107; }
    .dist { font-size: 28px; font-weight: bold; margin: 10px 0; color: #17a2b8; }
    .status { font-size: 18px; font-weight: bold; margin: 12px 0; padding: 10px; border-radius: 6px; border: 1px solid transparent; }
    .safe { background-color: rgba(40, 167, 69, 0.1); color: #28a745; border-color: #28a745; }
    .danger { background-color: rgba(220, 53, 69, 0.1); color: #dc3545; border-color: #dc3545; }
    .footer { font-size: 12px; color: #666; margin-top: 20px; }
  </style>
  <script>
    setInterval(function() {
      fetch('/data').then(response => response.json()).then(data => {
        document.getElementById('suhu-angka').innerText = data.suhu + " °C";
        document.getElementById('jarak-angka').innerText = "Jarak Lantai: " + data.jarak + " mm";
        
        var elSuhu = document.getElementById('status-suhu');
        elSuhu.innerText = data.statusSuhu;
        elSuhu.className = "status " + (data.statusSuhu.includes("BAHAYA") ? "danger" : "safe");
        
        var elGetaran = document.getElementById('status-getaran');
        elGetaran.innerText = data.statusGetar;
        elGetaran.className = "status " + (data.statusGetar.includes("ANOMALI") ? "danger" : "safe");
      });
    }, 1000);
  </script>
</head>
<body>
  <div class="card">
    <h1>PANEL MESIN CUCI</h1>
    <div id="suhu-angka" class="temp">--.- °C</div>
    <div id="jarak-angka" class="dist">Jarak: -- mm</div>
    <div id="status-suhu" class="status safe">Memuat Data...</div>
    <div id="status-getaran" class="status safe">Memuat Data...</div>
    <div class="footer">Real-Time IoT Monitor</div>
  </div>
</body>
</html>
)=====";

// Routing Web Server
void handleRoot() { serverWeb.send(200, "text/html", html_halaman_web); }
void handleData() {
  bool bahayaSuhu = (suhu > BATAS_SUHU_KRITIS);
  String json = "{";
  json += "\"suhu\":" + String(suhu, 1) + ",";
  json += "\"jarak\":" + String(jarakRataRata) + ",";
  json += "\"statusSuhu\":\"" + String(bahayaSuhu ? "PANAS BERBAHAYA" : "UDARA SEGAR") + "\",";
  json += "\"statusGetar\":\"" + String(statusAnomali ? "GETARAN BERANOMALI" : "GETARAN AMAN") + "\"";
  json += "}";
  serverWeb.send(200, "application/json", json);
}

// ==========================================
// PROGRAM UTAMA SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  
  pinMode(VIB_PIN, INPUT);
  dht.begin();
  
  // 1. Memulai I2C Pertama (Default Wire) untuk OLED -> D21 (SDA), D22 (SCL)
  Wire.begin(); 
  
  // 2. Memulai I2C Kedua (Wire1) untuk VL53L0X -> D16 (SDA), D17 (SCL)
  I2C_VL53L0X.begin(SENSOR_SDA, SENSOR_SCL); 

  // Inisialisasi OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Gagal inisialisasi OLED!"));
    for(;;); 
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Inisialisasi...");
  display.display();

  // Inisialisasi VL53L0X menggunakan jalur I2C Kedua (&I2C_VL53L0X)
  if (!lox.begin(0x29, false, &I2C_VL53L0X)) {
    Serial.println(F("Gagal mendeteksi VL53L0X! Cek kabel di pin 16 dan 17."));
    display.setCursor(0, 12);
    display.print("Error VL53L0X!");
    display.display();
    while(1);
  }

  // Koneksi WiFi
  display.setCursor(0, 12);
  display.print("Koneksi WiFi...");
  display.display();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }

  IPAddress autoGateway = WiFi.gatewayIP();
  IPAddress local_IP(autoGateway[0], autoGateway[1], autoGateway[2], 200);
  WiFi.config(local_IP, autoGateway, WiFi.subnetMask(), WiFi.dnsIP());

  serverTCP.begin(); 
  serverWeb.on("/", handleRoot);
  serverWeb.on("/data", handleData);
  serverWeb.begin();
  
  Serial.println("\nIP Static ESP32: " + WiFi.localIP().toString());
}

// ==========================================
// PROGRAM UTAMA LOOP (Siklus Super Cepat)
// ==========================================
void loop() {
  serverWeb.handleClient(); 
  bool wifiTersambung = (WiFi.status() == WL_CONNECTED);

  // Kelola Koneksi TCP Visual Basic
  if (serverTCP.hasClient()) {
    if (!vbClient || !vbClient.connected()) {
      if (vbClient) vbClient.stop(); 
      vbClient = serverTCP.available(); 
    } else {
      WiFiClient tolakKlien = serverTCP.available();
      tolakKlien.stop();
    }
  }
  bool vbTersambung = (vbClient && vbClient.connected());

  // 1. Membaca Sensor Suhu & Getaran
  float bacaSuhu = dht.readTemperature();
  if (!isnan(bacaSuhu)) suhu = bacaSuhu; 
  bool bahayaSuhu = (suhu > BATAS_SUHU_KRITIS);

  int statusGetaran = digitalRead(VIB_PIN);
  if (statusGetaran == HIGH) {
    hitungAnomali++; hitungAman = 0;      
    if (hitungAnomali >= 3) { statusAnomali = true; hitungAnomali = 3; }
  } else { 
    hitungAman++; hitungAnomali = 0;   
    if (hitungAman >= 3) { statusAnomali = false; hitungAman = 3; }
  }

  // 2. BURST SAMPLING VL53L0X: Ambil 10 data secepat mungkin saat ini juga!
  long totalJarak = 0;
  for (int i = 0; i < 10; i++) {
    VL53L0X_RangingMeasurementData_t measure;
    lox.rangingTest(&measure, false);
    if (measure.RangeStatus != 4) { 
      totalJarak += measure.RangeMilliMeter;
    } else {
      totalJarak += jarakRataRata; // Gunakan nilai sebelumnya jika pembacaan gagal/out of range
    }
  }
  jarakRataRata = totalJarak / 10; // Hitung rata-rata langsung dari 10 tembakan cepat

  // 3. Kirim Data ke VB via TCP INSTAN (Tanpa menunggu 1 detik!)
  if (vbTersambung) {
    String strSuhu = bahayaSuhu ? "PANAS BERBAHAYA" : "UDARA SEGAR";
    String strGetar = statusAnomali ? "GETARAN BERANOMALI" : "GETARAN AMAN";
    vbClient.println(String(suhu, 1) + "," + strSuhu + "," + strGetar + "," + String(jarakRataRata)); 
  }

  // 4. Kondisi Kritis (Layar Berkedip Invert)
  bool kondisiKritis = (bahayaSuhu && statusAnomali);
  if (kondisiKritis) {
    if (millis() - waktuKedipTerakhir >= 1000) {
      waktuKedipTerakhir = millis();
      statusKedipLayar = !statusKedipLayar;
      display.invertDisplay(statusKedipLayar); 
    }
  } else {
    display.invertDisplay(false); 
  }

  // 5. Menggambar Layar OLED (Layout Padat dengan Jarak)
  display.clearDisplay();
  
  // Baris 1: Suhu & Jarak Lantai
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Suhu:"); display.print(suhu, 1); display.print("C ");
  display.print("D:"); display.print(jarakRataRata); display.print("mm");
  
  // Ikon Status Kanan Atas
  display.drawBitmap(106, 0, vbTersambung ? icon_vb_ok : icon_vb_off, 8, 8, SSD1306_WHITE);
  display.drawBitmap(118, 0, wifiTersambung ? icon_wifi_ok : icon_wifi_off, 8, 8, SSD1306_WHITE);

  // Baris 2: Indikator Suhu & Emoji
  if (bahayaSuhu) {
    display.drawBitmap(0, 12, icon_api, 8, 8, SSD1306_WHITE);
    display.setCursor(12, 12); display.print("Panas Berbahaya");
  } else {
    display.drawBitmap(0, 12, icon_daun, 8, 8, SSD1306_WHITE);
    display.setCursor(12, 12); display.print("Udara Segar");
  }

  // Baris 3: Indikator Getaran
  display.setCursor(0, 24);
  display.print(statusAnomali ? "Getaran Beranomali" : "Getaran Aman");

  display.display(); 
  
  // Delay sangat singkat (10ms) hanya untuk menjaga stabilitas memori jaringan WiFi ESP32
  delay(10); 
}