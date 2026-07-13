#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// --- Konfigurasi WiFi & IP Static ---
const char* ssid = "NAMA_WIFI_ANDA";
const char* password = "PASSWORD_WIFI_ANDA";

// --- Konfigurasi Port Server (TCP/IP) ---
WiFiServer server(8080); 
WiFiClient vbClient;     

// --- Konfigurasi Pin ---
#define VIB_PIN 15       
#define DHTPIN 4         
#define DHTTYPE DHT22    

// --- Konfigurasi OLED ---
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 32 
#define OLED_RESET    -1 

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHT dht(DHTPIN, DHTTYPE);

// --- Variabel Logika ---
int hitungAnomali = 0;        
int hitungAman = 0;           
bool statusAnomali = false;   
float suhu = 0.0;
unsigned long waktuKirimTerakhir = 0;
unsigned long waktuKedipTerakhir = 0;
bool statusKedipLayar = false;

// ==========================================
// ASET IKON (BITMAP 8x8 PIXEL)
// ==========================================
const unsigned char icon_wifi_ok[] PROGMEM = { 0x00, 0x3c, 0x42, 0x81, 0x00, 0x24, 0x00, 0x18 };
const unsigned char icon_wifi_off[] PROGMEM = { 0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81 };
const unsigned char icon_vb_ok[] PROGMEM = { 0x00, 0xac, 0xaa, 0xac, 0x4a, 0x4c, 0x00, 0x00 };
const unsigned char icon_vb_off[] PROGMEM = { 0x81, 0x6c, 0x3a, 0x1c, 0x0a, 0x4c, 0x80, 0x00 };

// Ikon Api (Panas)
const unsigned char icon_api[] PROGMEM = { 0x08, 0x1c, 0x2a, 0x5d, 0x55, 0x49, 0x22, 0x0c };
// Ikon Daun (Segar)
const unsigned char icon_daun[] PROGMEM = { 0x02, 0x06, 0x0e, 0x1c, 0x38, 0x70, 0x20, 0x00 };

void setup() {
  Serial.begin(115200);
  
  pinMode(VIB_PIN, INPUT);
  dht.begin();
  
  // Menggunakan jalur I2C Default ESP32: D21 (SDA) dan D22 (SCL)
  Wire.begin();

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Gagal inisialisasi OLED!"));
    for(;;); 
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.print("Koneksi WiFi...");
  display.display();

  // 1. Mulai koneksi WiFi (DHCP)
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // 2. Ambil data Gateway otomatis dari Router
  IPAddress autoGateway = WiFi.gatewayIP();
  IPAddress autoSubnet = WiFi.subnetMask();
  IPAddress autoDNS = WiFi.dnsIP();

  // 3. Buat IP Static dinamis berakhiran .200
  IPAddress local_IP(autoGateway[0], autoGateway[1], autoGateway[2], 200);
  WiFi.config(local_IP, autoGateway, autoSubnet, autoDNS);

  // Mulai Server TCP
  server.begin(); 
  Serial.println("\nIP Address (Static): " + WiFi.localIP().toString());
}

void loop() {
  // 1. Cek Koneksi WiFi & VB
  bool wifiTersambung = (WiFi.status() == WL_CONNECTED);

  if (server.hasClient()) {
    if (!vbClient || !vbClient.connected()) {
      if (vbClient) vbClient.stop(); 
      vbClient = server.available(); 
    } else {
      WiFiClient tolakKlien = server.available();
      tolakKlien.stop();
    }
  }
  bool vbTersambung = (vbClient && vbClient.connected());

  // 2. Membaca Sensor
  float bacaSuhu = dht.readTemperature();
  if (!isnan(bacaSuhu)) suhu = bacaSuhu; 
  
  bool bahayaSuhu = (suhu > 40.0); // True jika suhu lebih dari 40

  // Logika Filter Getaran (Debounce)
  int statusGetaran = digitalRead(VIB_PIN);
  if (statusGetaran == HIGH) {
    hitungAnomali++;     
    hitungAman = 0;      
    if (hitungAnomali >= 5) { statusAnomali = true; hitungAnomali = 5; }
  } else { 
    hitungAman++;        
    hitungAnomali = 0;   
    if (hitungAman >= 7) { statusAnomali = false; hitungAman = 7; }
  }

  // 3. Kondisi Kritis (Keduanya Bahaya) -> Layar Berkedip 1000ms
  bool kondisiKritis = (bahayaSuhu && statusAnomali);
  
  if (kondisiKritis) {
    if (millis() - waktuKedipTerakhir >= 1000) {
      waktuKedipTerakhir = millis();
      statusKedipLayar = !statusKedipLayar;
      display.invertDisplay(statusKedipLayar); // Membalik warna layar (Hitam jadi putih)
    }
  } else {
    // Kembalikan layar ke normal jika tidak kritis
    display.invertDisplay(false); 
  }

  // 4. Kirim Data ke Visual Basic (Format: SUHU,STATUS_SUHU,STATUS_GETARAN)
  if (millis() - waktuKirimTerakhir >= 1000) {
    waktuKirimTerakhir = millis();
    
    if (vbTersambung) {
      String dataSuhu = bahayaSuhu ? "PANAS BERBAHAYA" : "UDARA SEGAR";
      String dataGetaran = statusAnomali ? "GETARAN BERANOMALI" : "GETARAN AMAN";
      
      String dataPaket = String(suhu, 1) + "," + dataSuhu + "," + dataGetaran;
      vbClient.println(dataPaket); 
    }
  }

  // 5. Menggambar Layar OLED (Layout 3 Baris)
  display.clearDisplay();
  
  // Baris 1 (Suhu & Ikon Sinyal) - Posisi Y: 0
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Suhu: ");
  display.print(suhu, 1); 
  display.print("C");

  if (vbTersambung) display.drawBitmap(106, 0, icon_vb_ok, 8, 8, SSD1306_WHITE);
  else display.drawBitmap(106, 0, icon_vb_off, 8, 8, SSD1306_WHITE);

  if (wifiTersambung) display.drawBitmap(118, 0, icon_wifi_ok, 8, 8, SSD1306_WHITE);
  else display.drawBitmap(118, 0, icon_wifi_off, 8, 8, SSD1306_WHITE);

  // Baris 2 (Indikator Teks Suhu & Emoji) - Posisi Y: 12
  if (bahayaSuhu) {
    display.drawBitmap(0, 12, icon_api, 8, 8, SSD1306_WHITE);
    display.setCursor(12, 12);
    display.print("Panas Berbahaya");
  } else {
    display.drawBitmap(0, 12, icon_daun, 8, 8, SSD1306_WHITE);
    display.setCursor(12, 12);
    display.print("Udara Segar");
  }

  // Baris 3 (Indikator Teks Getaran) - Posisi Y: 24
  display.setCursor(0, 24);
  if (statusAnomali) {
    display.print("Getaran Beranomali");
  } else {
    display.print("Getaran Aman");
  }

  display.display(); 
  delay(100); // Waktu respons sensor 0.1 detik
}