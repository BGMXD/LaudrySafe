Imports System.Net.Sockets
Imports System.IO
Imports System.Data.SQLite

Public Class Form1
    ' Variabel untuk Koneksi TCP
    Private client As TcpClient
    Private reader As StreamReader
    Private isConnected As Boolean = False

    ' Variabel Database SQLite
    Private dbConnection As String = "Data Source=history_anomali.db;Version=3;"
    
    ' Variabel Pencegah Spam Database
    Private statusSedangAnomali As Boolean = False

    Private Sub Form1_Load(sender As Object, e As EventArgs) Handles MyBase.Load
        ' Saat aplikasi dibuka, buat database jika belum ada, lalu tampilkan datanya
        BuatDatabaseDanTabel()
        TampilkanHistory()
    End Sub

    ' ==========================================
    ' BAGIAN 1: DATABASE SQLITE
    ' ==========================================
    Private Sub BuatDatabaseDanTabel()
        If Not File.Exists("history_anomali.db") Then
            SQLiteConnection.CreateFile("history_anomali.db")
        End If

        Using conn As New SQLiteConnection(dbConnection)
            conn.Open()
            ' Membuat tabel dengan kolom: ID, Tanggal, Waktu, Suhu, Keterangan
            Dim query As String = "CREATE TABLE IF NOT EXISTS TabelAnomali (ID INTEGER PRIMARY KEY AUTOINCREMENT, Tanggal TEXT, Waktu TEXT, Suhu TEXT, Keterangan TEXT)"
            Using cmd As New SQLiteCommand(query, conn)
                cmd.ExecuteNonQuery()
            End Using
        End Using
    End Sub

    Private Sub SimpanAnomali(suhuTercatat As String, keteranganBahaya As String)
        Dim tglSekarang As String = DateTime.Now.ToString("dd/MM/yyyy")
        Dim waktuSekarang As String = DateTime.Now.ToString("HH:mm:ss")

        Using conn As New SQLiteConnection(dbConnection)
            conn.Open()
            Dim query As String = "INSERT INTO TabelAnomali (Tanggal, Waktu, Suhu, Keterangan) VALUES (@tgl, @waktu, @suhu, @ket)"
            Using cmd As New SQLiteCommand(query, conn)
                cmd.Parameters.AddWithValue("@tgl", tglSekarang)
                cmd.Parameters.AddWithValue("@waktu", waktuSekarang)
                cmd.Parameters.AddWithValue("@suhu", suhuTercatat)
                cmd.Parameters.AddWithValue("@ket", keteranganBahaya)
                cmd.ExecuteNonQuery()
            End Using
        End Using

        ' Refresh tabel di layar agar langsung terlihat
        TampilkanHistory()
    End Sub

    Private Sub TampilkanHistory()
        Using conn As New SQLiteConnection(dbConnection)
            conn.Open()
            Dim query As String = "SELECT Tanggal, Waktu, Suhu, Keterangan FROM TabelAnomali ORDER BY ID DESC"
            Dim adapter As New SQLiteDataAdapter(query, conn)
            Dim tabelData As New DataTable()
            adapter.Fill(tabelData)

            ' Masukkan data ke DataGridView
            dgvHistory.DataSource = tabelData
            
            ' Merapikan lebar kolom otomatis
            dgvHistory.AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill
        End Using
    End Sub

    ' ==========================================
    ' BAGIAN 2: KONEKSI TCP KE ESP32
    ' ==========================================
    Private Async Sub btnConnect_Click(sender As Object, e As EventArgs) Handles btnConnect.Click
        If isConnected Then
            ' Jika sedang terhubung, maka tombol ini berfungsi untuk Putus Koneksi
            isConnected = False
            If client IsNot Nothing Then client.Close()
            lblStatusKoneksi.Text = "Status: Terputus"
            btnConnect.Text = "Hubungkan"
            Return
        End If

        Try
            lblStatusKoneksi.Text = "Status: Mencari ESP32..."
            btnConnect.Enabled = False

            ' Memulai koneksi ke ESP32 pada Port 8080
            client = New TcpClient()
            Await client.ConnectAsync(txtIP.Text, 8080)
            
            reader = New StreamReader(client.GetStream())
            isConnected = True
            
            lblStatusKoneksi.Text = "Status: TERHUBUNG"
            lblStatusKoneksi.ForeColor = Color.Green
            btnConnect.Text = "Putuskan Koneksi"
            btnConnect.Enabled = True

            ' Mulai membaca data di latar belakang
            MulaiBacaData()

        Catch ex As Exception
            MessageBox.Show("Gagal terhubung! Pastikan ESP32 menyala dan IP benar." & vbCrLf & ex.Message, "Error Koneksi", MessageBoxButtons.OK, MessageBoxIcon.Error)
            lblStatusKoneksi.Text = "Status: Terputus"
            btnConnect.Enabled = True
        End Try
    End Sub

    Private Async Sub MulaiBacaData()
        Try
            While isConnected AndAlso client.Connected
                ' Membaca 1 baris teks yang dikirim ESP32 (misal: "45.2,PANAS BERBAHAYA,GETARAN BERANOMALI")
                Dim dataMasuk As String = Await reader.ReadLineAsync()
                
                If dataMasuk IsNot Nothing Then
                    ProsesDataSensor(dataMasuk)
                End If
            End While
        Catch ex As Exception
            ' Jika koneksi terputus di tengah jalan
            isConnected = False
            lblStatusKoneksi.Text = "Status: Terputus (Koneksi Hilang)"
            lblStatusKoneksi.ForeColor = Color.Red
            btnConnect.Text = "Hubungkan"
        End Try
    End Sub

    ' ==========================================
    ' BAGIAN 3: LOGIKA PEMROSESAN DATA & ALARM
    ' ==========================================
    Private Sub ProsesDataSensor(dataKasar As String)
        ' Memecah data berdasarkan koma (,)
        Dim paket() As String = dataKasar.Split(","c)
        
        ' Pastikan data tidak rusak (harus ada 3 bagian: Suhu, Status Suhu, Status Getaran)
        If paket.Length = 3 Then
            Dim nilaiSuhu As String = paket(0)
            Dim statusSuhu As String = paket(1)
            Dim statusGetaran As String = paket(2)

            ' Tampilkan ke layar Real-Time
            lblSuhuAngka.Text = nilaiSuhu & " °C"
            lblSuhuStatus.Text = statusSuhu
            lblGetaranStatus.Text = statusGetaran

            ' Memberi warna merah jika bahaya, hijau jika aman
            lblSuhuStatus.ForeColor = If(statusSuhu.Contains("BAHAYA"), Color.Red, Color.Green)
            lblGetaranStatus.ForeColor = If(statusGetaran.Contains("ANOMALI"), Color.Red, Color.Green)

            ' --- LOGIKA DATABASE ---
            Dim adaBahaya As Boolean = statusSuhu.Contains("BAHAYA") OrElse statusGetaran.Contains("ANOMALI")

            If adaBahaya = True Then
                ' Jika ada bahaya, cek apakah ini bahaya baru atau bahaya lama yang belum selesai
                If statusSedangAnomali = False Then
                    ' Ini adalah BAHAYA BARU. Tentukan sumber bahayanya!
                    Dim keteranganBahaya As String = ""
                    
                    If statusSuhu.Contains("BAHAYA") AndAlso statusGetaran.Contains("ANOMALI") Then
                        keteranganBahaya = "Suhu & Getaran Kritis"
                    ElseIf statusSuhu.Contains("BAHAYA") Then
                        keteranganBahaya = "Overheating (Suhu Tinggi)"
                    ElseIf statusGetaran.Contains("ANOMALI") Then
                        keteranganBahaya = "Getaran Kasar (Beban Unbalance)"
                    End If

                    ' Simpan ke SQLite
                    SimpanAnomali(nilaiSuhu, keteranganBahaya)
                    
                    ' Kunci status agar tidak tersimpan berulang kali ke database detik berikutnya
                    statusSedangAnomali = True 
                End If
            Else
                ' Jika mesin kembali aman ("UDARA SEGAR" & "GETARAN AMAN")
                ' Buka kuncian status, sehingga jika nanti bahaya lagi, bisa tercatat di database lagi
                statusSedangAnomali = False
            End If

        End If
    End Sub
End Class