Imports System.Net.Sockets
Imports System.IO
Imports System.Data.SQLite
Imports System.Runtime.InteropServices ' Digunakan untuk pemutar MP3

Public Class Form1
    ' ==========================================
    ' DEKLARASI VARIABEL GLOBAL
    ' ==========================================
    ' Variabel untuk Koneksi TCP
    Private client As TcpClient
    Private reader As StreamReader
    Private isConnected As Boolean = False

    ' Variabel Database SQLite
    Private dbConnection As String = "Data Source=history_anomali.db;Version=3;"
    
    ' Variabel Logika (Mencegah Spam DB & Audio)
    Private statusSedangAnomali As Boolean = False
    Private isLaguDiputar As Boolean = False

    ' ==========================================
    ' FUNGSI PEMUTAR MP3 (WINDOWS API)
    ' ==========================================
    <DllImport("winmm.dll")>
    Private Shared Function mciSendString(ByVal command As String, ByVal buffer As String, ByVal bufferSize As Integer, ByVal hwndCallback As IntPtr) As Integer
    End Function

    Private Sub PutarMP3(pathLagu As String)
        mciSendString("close laguAlarm", Nothing, 0, IntPtr.Zero) ' Tutup lagu sebelumnya
        mciSendString("open """ & pathLagu & """ type mpegvideo alias laguAlarm", Nothing, 0, IntPtr.Zero)
        mciSendString("play laguAlarm", Nothing, 0, IntPtr.Zero)
    End Sub

    Private Sub HentikanMP3()
        mciSendString("close laguAlarm", Nothing, 0, IntPtr.Zero)
    End Sub

    ' ==========================================
    ' EVENT FORM LOAD (Awal Aplikasi Dibuka)
    ' ==========================================
    Private Sub Form1_Load(sender As Object, e As EventArgs) Handles MyBase.Load
        TerapkanTemaModern()      ' 1. Ubah desain ke Dark Mode
        BuatDatabaseDanTabel()    ' 2. Buat database SQLite jika belum ada
        TampilkanHistory()        ' 3. Tampilkan riwayat lama ke layar
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

        TampilkanHistory() ' Refresh tabel di layar
    End Sub

    Private Sub TampilkanHistory()
        Using conn As New SQLiteConnection(dbConnection)
            conn.Open()
            Dim query As String = "SELECT Tanggal, Waktu, Suhu, Keterangan FROM TabelAnomali ORDER BY ID DESC"
            Dim adapter As New SQLiteDataAdapter(query, conn)
            Dim tabelData As New DataTable()
            adapter.Fill(tabelData)

            dgvHistory.DataSource = tabelData
            dgvHistory.AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill
        End Using
    End Sub

    Private Sub btnHapusRiwayat_Click(sender As Object, e As EventArgs) Handles btnHapusRiwayat.Click
        Dim konfirmasi As DialogResult = MessageBox.Show(
            "Apakah Anda yakin ingin menghapus SEMUA data riwayat anomali mesin cuci?" & vbCrLf & "Data yang sudah dihapus tidak dapat dikembalikan!", 
            "Peringatan Hapus Data", MessageBoxButtons.YesNo, MessageBoxIcon.Warning)

        If konfirmasi = DialogResult.Yes Then
            Try
                Using conn As New SQLiteConnection(dbConnection)
                    conn.Open()
                    ' Hapus isi tabel
                    Using cmd As New SQLiteCommand("DELETE FROM TabelAnomali;", conn)
                        cmd.ExecuteNonQuery()
                    End Using
                    ' Reset nomor urut (ID) kembali ke 1
                    Using cmdReset As New SQLiteCommand("DELETE FROM sqlite_sequence WHERE name='TabelAnomali';", conn)
                        cmdReset.ExecuteNonQuery()
                    End Using
                End Using

                MessageBox.Show("Seluruh riwayat anomali berhasil dibersihkan.", "Sukses", MessageBoxButtons.OK, MessageBoxIcon.Information)
                TampilkanHistory()

            Catch ex As Exception
                MessageBox.Show("Terjadi kesalahan saat menghapus data: " & ex.Message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error)
            End Try
        End If
    End Sub


    ' ==========================================
    ' BAGIAN 2: KONEKSI TCP KE ESP32
    ' ==========================================
    Private Async Sub btnConnect_Click(sender As Object, e As EventArgs) Handles btnConnect.Click
        If isConnected Then
            isConnected = False
            If client IsNot Nothing Then client.Close()
            lblStatusKoneksi.Text = "Status: Terputus"
            lblStatusKoneksi.ForeColor = Color.White
            btnConnect.Text = "Hubungkan"
            Return
        End If

        Try
            lblStatusKoneksi.Text = "Status: Mencari ESP32..."
            btnConnect.Enabled = False

            client = New TcpClient()
            Await client.ConnectAsync(txtIP.Text, 8080) ' Hubungkan ke Port 8080
            
            reader = New StreamReader(client.GetStream())
            isConnected = True
            
            lblStatusKoneksi.Text = "Status: TERHUBUNG"
            lblStatusKoneksi.ForeColor = Color.FromArgb(40, 167, 69) ' Hijau
            btnConnect.Text = "Putuskan Koneksi"
            btnConnect.Enabled = True

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
                Dim dataMasuk As String = Await reader.ReadLineAsync()
                
                If dataMasuk IsNot Nothing Then
                    ProsesDataSensor(dataMasuk)
                End If
            End While
        Catch ex As Exception
            isConnected = False
            lblStatusKoneksi.Text = "Status: Terputus (Koneksi Hilang)"
            lblStatusKoneksi.ForeColor = Color.FromArgb(220, 53, 69) ' Merah
            btnConnect.Text = "Hubungkan"
            HentikanMP3() ' Matikan lagu jika ESP32 tiba-tiba mati
        End Try
    End Sub


    ' ==========================================
    ' BAGIAN 3: LOGIKA PEMROSESAN DATA & ALARM
    ' ==========================================
    Private Sub ProsesDataSensor(dataKasar As String)
        ' Pecah data berformat: "45.2,PANAS BERBAHAYA,GETARAN BERANOMALI"
        Dim paket() As String = dataKasar.Split(","c)
        
        If paket.Length = 3 Then
            Dim nilaiSuhu As String = paket(0)
            Dim statusSuhu As String = paket(1)
            Dim statusGetaran As String = paket(2)

            ' Update UI Label
            lblSuhuAngka.Text = nilaiSuhu & " °C"
            lblSuhuStatus.Text = statusSuhu
            lblGetaranStatus.Text = statusGetaran

            lblSuhuStatus.ForeColor = If(statusSuhu.Contains("BAHAYA"), Color.FromArgb(220, 53, 69), Color.FromArgb(40, 167, 69))
            lblGetaranStatus.ForeColor = If(statusGetaran.Contains("ANOMALI"), Color.FromArgb(220, 53, 69), Color.FromArgb(40, 167, 69))

            ' Cek Status Kritis
            Dim adaBahaya As Boolean = statusSuhu.Contains("BAHAYA") OrElse statusGetaran.Contains("ANOMALI")
            Dim kritisKeduanya As Boolean = statusSuhu.Contains("BAHAYA") AndAlso statusGetaran.Contains("ANOMALI")

            ' 1. Logika Audio (Rickroll)
            If kritisKeduanya = True AndAlso isLaguDiputar = False Then
                PutarMP3("E:\Visual Studio\repos\Bahaya_MesinCuci_Project_IoT\Rickroll Meme Template.mp3")
                isLaguDiputar = True
            ElseIf kritisKeduanya = False AndAlso isLaguDiputar = True Then
                HentikanMP3()
                isLaguDiputar = False
            End If

            ' 2. Logika Database
            If adaBahaya = True Then
                If statusSedangAnomali = False Then
                    Dim keteranganBahaya As String = ""
                    If statusSuhu.Contains("BAHAYA") AndAlso statusGetaran.Contains("ANOMALI") Then
                        keteranganBahaya = "Suhu & Getaran Kritis"
                    ElseIf statusSuhu.Contains("BAHAYA") Then
                        keteranganBahaya = "Overheating (Suhu Tinggi)"
                    ElseIf statusGetaran.Contains("ANOMALI") Then
                        keteranganBahaya = "Getaran Kasar (Beban Unbalance)"
                    End If

                    SimpanAnomali(nilaiSuhu, keteranganBahaya)
                    statusSedangAnomali = True 
                End If
            Else
                ' Jika aman, reset status pencatatan DB
                statusSedangAnomali = False
            End If
        End If
    End Sub


    ' ==========================================
    ' BAGIAN 4: SUNTIKAN TEMA MODERN DARK MODE
    ' ==========================================
    Private Sub TerapkanTemaModern()
        Me.BackColor = Color.FromArgb(30, 30, 30)
        Me.ForeColor = Color.White
        Me.Font = New Font("Segoe UI", 10, FontStyle.Regular)
        Me.Text = "LaundrySafe Dashboard - Monitor Mesin Cuci"

        btnConnect.FlatStyle = FlatStyle.Flat
        btnConnect.FlatAppearance.BorderSize = 0
        btnConnect.BackColor = Color.FromArgb(0, 122, 204)
        btnConnect.ForeColor = Color.White
        btnConnect.Font = New Font("Segoe UI", 10, FontStyle.Bold)
        btnConnect.Cursor = Cursors.Hand

        btnHapusRiwayat.FlatStyle = FlatStyle.Flat
        btnHapusRiwayat.FlatAppearance.BorderSize = 0
        btnHapusRiwayat.BackColor = Color.FromArgb(220, 53, 69)
        btnHapusRiwayat.ForeColor = Color.White
        btnHapusRiwayat.Font = New Font("Segoe UI", 10, FontStyle.Bold)
        btnHapusRiwayat.Cursor = Cursors.Hand

        txtIP.BackColor = Color.FromArgb(45, 45, 48)
        txtIP.ForeColor = Color.White
        txtIP.BorderStyle = BorderStyle.FixedSingle
        txtIP.Font = New Font("Segoe UI", 11, FontStyle.Regular)

        With dgvHistory
            .BackgroundColor = Color.FromArgb(45, 45, 48) 
            .BorderStyle = BorderStyle.None
            .EnableHeadersVisualStyles = False 
            
            .ColumnHeadersBorderStyle = DataGridViewHeaderBorderStyle.None
            .ColumnHeadersDefaultCellStyle.BackColor = Color.FromArgb(37, 37, 38)
            .ColumnHeadersDefaultCellStyle.ForeColor = Color.FromArgb(0, 122, 204)
            .ColumnHeadersDefaultCellStyle.Font = New Font("Segoe UI", 10, FontStyle.Bold)
            .ColumnHeadersHeight = 35
            
            .DefaultCellStyle.BackColor = Color.FromArgb(45, 45, 48)
            .DefaultCellStyle.ForeColor = Color.White
            .DefaultCellStyle.SelectionBackColor = Color.FromArgb(0, 122, 204) 
            .DefaultCellStyle.SelectionForeColor = Color.White
            
            .RowHeadersVisible = False 
            .AllowUserToAddRows = False 
            .GridColor = Color.FromArgb(60, 60, 60) 
            .AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill 
            .SelectionMode = DataGridViewSelectionMode.FullRowSelect 
        End With

        lblSuhuAngka.Font = New Font("Segoe UI", 36, FontStyle.Bold)
        lblSuhuStatus.Font = New Font("Segoe UI", 14, FontStyle.Bold)
        lblGetaranStatus.Font = New Font("Segoe UI", 14, FontStyle.Bold)
    End Sub

End Class