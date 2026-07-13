#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// --- Konfigurasi WiFi & IP Static ---
const char* ssid = "NAMA_WIFI_ANDA";
const char* password = "PASSWORD_WIFI_ANDA";

// Atur IP Static yang diinginkan (Sesuaikan dengan segmen IP Router Anda)
IPAddress local_IP(192, 168, 1, 200); 
IPAddress gateway(192, 168, 1, 1);    
IPAddress subnet(255, 255, 255, 0);   
IPAddress primaryDNS(8, 8, 8, 8);     

// --- Konfigurasi Port Server (TCP/IP) ---
WiFiServer server(8080); // Port 8080 sangat optimal untuk socket IoT
WiFiClient vbClient;     // Variabel penyimpan koneksi Visual Basic

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

// --- Variabel Debounce 2 Arah & Timer ---
int hitungAnomali = 0;        
int hitungAman = 0;           
bool statusAnomali = false;   
float suhu = 0.0;
unsigned long waktuKirimTerakhir = 0;

// ==========================================
// ASET IKON (BITMAP 8x8 PIXEL)
// ==========================================
// 1. Ikon WiFi Terhubung (Sinyal Gelombang)
const unsigned char icon_wifi_ok[] PROGMEM = {
  0x00, 0x3c, 0x42, 0x81, 0x00, 0x24, 0x00, 0x18
};
// 2. Ikon WiFi Terputus (Tanda Silang X)
const unsigned char icon_wifi_off[] PROGMEM = {
  0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81
};
// 3. Ikon VB Terhubung (Teks "VB" Kecil)
const unsigned char icon_vb_ok[] PROGMEM = {
  0x00, 0xac, 0xaa, 0xac, 0x4a, 0x4c, 0x00, 0x00
};
// 4. Ikon VB Terputus (Teks "VB" Dicoret Silang)
const unsigned char icon_vb_off[] PROGMEM = {
  0x81, 0x6c, 0x3a, 0x1c, 0x0a, 0x4c, 0x80, 0x00
};

void setup() {
  Serial.begin(115200);
  
  pinMode(VIB_PIN, INPUT);
  dht.begin();
  Wire.begin();

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Gagal inisialisasi OLED!"));
    for(;;); 
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.print("Setup IP Static...");
  display.display();

  // Konfigurasi IP Static
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS)) {
    Serial.println("Gagal mengonfigurasi IP Static");
  }

  WiFi.begin(ssid, password);

  // Mulai Server TCP
  server.begin(); 
  Serial.println("Server TCP dimulai pada port 8080");
}

void loop() {
  // 1. Cek Status Koneksi WiFi
  bool wifiTersambung = (WiFi.status() == WL_CONNECTED);

  // 2. Cek & Kelola Koneksi dari Visual Basic
  if (server.hasClient()) {
    // Jika ada klien baru yang mencoba masuk
    if (!vbClient || !vbClient.connected()) {
      if (vbClient) vbClient.stop(); // Tutup koneksi lama (jika ada *hang*)
      vbClient = server.available(); // Terima koneksi dari VB
      Serial.println("Visual Basic TERHUBUNG!");
    } else {
      // Tolak koneksi klien lain jika VB sudah terhubung (1 server 1 klien)
      WiFiClient tolakKlien = server.available();
      tolakKlien.stop();
    }
  }

  // 3. Membaca Data Sensor
  float bacaSuhu = dht.readTemperature();
  if (!isnan(bacaSuhu)) {
    suhu = bacaSuhu; 
  }
  
  int statusGetaran = digitalRead(VIB_PIN);

  // Logika Filter Getaran (Debounce)
  if (statusGetaran == HIGH) {
    hitungAnomali++;     
    hitungAman = 0;      
    if (hitungAnomali >= 5) { statusAnomali = true; hitungAnomali = 5; }
  } else { 
    hitungAman++;        
    hitungAnomali = 0;   
    if (hitungAman >= 7) { statusAnomali = false; hitungAman = 7; }
  }

  // 4. Kirim Data ke Visual Basic (Setiap 1 Detik)
  // Format data: "SUHU,STATUS_GETARAN" -> Contoh: "45.2,ANOMALI"
  if (millis() - waktuKirimTerakhir >= 1000) {
    waktuKirimTerakhir = millis();
    
    if (vbClient && vbClient.connected()) {
      String dataPaket = String(suhu, 1) + ",";
      dataPaket += (statusAnomali) ? "ANOMALI" : "AMAN";
      
      vbClient.println(dataPaket); // Kirim ke VB
    }
  }

  // 5. Menggambar Tampilan Layar OLED
  display.clearDisplay();
  
  // -- Bagian Teks (Suhu dan Getaran) --
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Suhu: ");
  display.print(suhu, 1); 
  display.print("C");

  display.setCursor(0, 16);
  display.print("Vib : ");
  if (statusAnomali == true) {
    display.print("ANOMALI!");
  } else {
    display.print("Aman");
  }

  // -- Bagian Ikon Status Kanan Atas --
  // Parameter drawBitmap: (X, Y, nama_bitmap, lebar, tinggi, warna)
  
  // Cek Status VB
  bool vbTersambung = (vbClient && vbClient.connected());

  // Gambar Ikon VB (Posisi X: 106, Y: 0)
  if (vbTersambung) {
    display.drawBitmap(106, 0, icon_vb_ok, 8, 8, SSD1306_WHITE);
  } else {
    display.drawBitmap(106, 0, icon_vb_off, 8, 8, SSD1306_WHITE);
  }

  // Gambar Ikon WiFi (Posisi X: 118, Y: 0) Paling ujung kanan
  if (wifiTersambung) {
    display.drawBitmap(118, 0, icon_wifi_ok, 8, 8, SSD1306_WHITE);
  } else {
    display.drawBitmap(118, 0, icon_wifi_off, 8, 8, SSD1306_WHITE);
  }

  display.display(); 
  delay(100); 
}