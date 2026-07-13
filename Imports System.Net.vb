Imports System.Net.Sockets
Imports System.IO
Imports System.Data.SQLite
Imports System.Windows.Forms.DataVisualization.Charting
Imports System.Drawing

Public Class Form1
    ' ==========================================
    ' DEKLARASI KONTROL UI (Dibuat lewat kode)
    ' ==========================================
    Private txtIP As TextBox
    Private btnConnect As Button
    Private lblStatusKoneksi As Label
    Private lblSuhuAngka As Label
    Private lblSuhuStatus As Label
    Private lblGetaranStatus As Label
    Private lblJarakAngka As Label
    Private dgvHistory As DataGridView
    Private btnHapusRiwayat As Button

    ' Grafik 1: Analisis Analog (Suhu & Jarak)
    Private grafikSensor As Chart
    Private seriesSuhu As Series
    Private seriesJarak As Series

    ' Grafik 2: Analisis Digital (Getaran SW-420)
    Private grafikGetaran As Chart
    Private seriesGetaran As Series

    ' ==========================================
    ' DEKLARASI VARIABEL KONEKSI & DATABASE
    ' ==========================================
    Private client As TcpClient
    Private reader As StreamReader
    Private isConnected As Boolean = False

    Private dbConnection As String = "Data Source=iot_sensor.db;Version=3;"

    Private statusBahayaTerakhir As String = "AMAN"
    Private isLaguDiputar As Boolean = False
    Private waktuTerakhirUpdateGrafik As DateTime = DateTime.Now

    ' KONSTANTA DISESUAIKAN DENGAN KEBUTUHAN TERBARUMU (1 Detik & 10 Titik)
    Private Const INTERVAL_GRAFIK_DETIK As Integer = 1
    Private Const MAKSIMAL_TITIK_DATA As Integer = 10

    ' ==========================================
    ' EVENT FORM LOAD
    ' ==========================================
    Private Sub Form1_Load(sender As Object, e As EventArgs) Handles MyBase.Load
        BangunUIOtomatis()
        InisialisasiGrafikRealTime()
        BuatDatabaseDanTabel()
        TampilkanHistory()
    End Sub

    ' ==========================================
    ' BAGIAN 1: PEMBUATAN UI OTOMATIS (PERBAIKAN TATA LETAK)
    ' ==========================================
    Private Sub BangunUIOtomatis()
        Me.Size = New Size(1120, 670)
        Me.Text = "LaundrySafe Dashboard - Monitor Mesin Cuci IoT"
        Me.BackColor = Color.FromArgb(235, 243, 251)
        Me.Font = New Font("Segoe UI", 9.5F, FontStyle.Regular)
        Me.StartPosition = FormStartPosition.CenterScreen

        ' GroupBox Kontrol Utama (Koneksi)
        Dim gbKoneksi As New GroupBox With {
            .Text = "KONFIGURASI JARINGAN", .Location = New Point(15, 10), .Size = New Size(500, 75),
            .ForeColor = Color.FromArgb(0, 51, 153), .Font = New Font("Segoe UI", 9.0F, FontStyle.Bold)
        }
        Me.Controls.Add(gbKoneksi)

        Dim lblIP As New Label With {.Text = "IP ESP32 :", .Location = New Point(15, 33), .AutoSize = True, .ForeColor = Color.Black}
        gbKoneksi.Controls.Add(lblIP)

        txtIP = New TextBox With {.Text = "192.168.1.200", .Location = New Point(85, 30), .Width = 120, .BackColor = Color.White}
        gbKoneksi.Controls.Add(txtIP)

        btnConnect = New Button With {
            .Text = "🔌 Hubungkan", .Location = New Point(215, 26), .Size = New Size(120, 30),
            .BackColor = Color.White, .Cursor = Cursors.Hand, .FlatStyle = FlatStyle.Standard
        }
        AddHandler btnConnect.Click, AddressOf btnConnect_Click
        gbKoneksi.Controls.Add(btnConnect)

        lblStatusKoneksi = New Label With {.Text = "Status: Terputus", .Location = New Point(350, 33), .AutoSize = True, .ForeColor = Color.Black}
        gbKoneksi.Controls.Add(lblStatusKoneksi)

        ' GroupBox Indikator Sensor (Perbaikan Posisi & Font Size)
        Dim gbIndikator As New GroupBox With {
            .Text = "MONITOR SENSOR REAL-TIME", .Location = New Point(15, 95), .Size = New Size(500, 190),
            .ForeColor = Color.FromArgb(0, 51, 153), .Font = New Font("Segoe UI", 9.0F, FontStyle.Bold)
        }
        Me.Controls.Add(gbIndikator)

        ' Font size 28 agar tidak menabrak label di bawahnya
        lblSuhuAngka = New Label With {.Text = "--.- °C", .Location = New Point(18, 30), .AutoSize = True, .Font = New Font("Segoe UI", 28, FontStyle.Bold), .ForeColor = Color.FromArgb(0, 51, 153)}
        gbIndikator.Controls.Add(lblSuhuAngka)

        lblSuhuStatus = New Label With {.Text = "Status Suhu : -", .Location = New Point(20, 90), .AutoSize = True, .Font = New Font("Segoe UI", 11, FontStyle.Bold)}
        gbIndikator.Controls.Add(lblSuhuStatus)

        lblGetaranStatus = New Label With {.Text = "Status Getaran : -", .Location = New Point(20, 120), .AutoSize = True, .Font = New Font("Segoe UI", 11, FontStyle.Bold)}
        gbIndikator.Controls.Add(lblGetaranStatus)

        lblJarakAngka = New Label With {.Text = "Jarak Lantai : -- mm", .Location = New Point(20, 150), .AutoSize = True, .Font = New Font("Segoe UI", 11, FontStyle.Bold), .ForeColor = Color.FromArgb(0, 122, 204)}
        gbIndikator.Controls.Add(lblJarakAngka)

        ' GroupBox Tabel Riwayat
        Dim gbTabel As New GroupBox With {
            .Text = "LOG RIWAYAT ANOMALI MESIN", .Location = New Point(15, 295), .Size = New Size(500, 320),
            .ForeColor = Color.FromArgb(0, 51, 153), .Font = New Font("Segoe UI", 9.0F, FontStyle.Bold)
        }
        Me.Controls.Add(gbTabel)

        dgvHistory = New DataGridView With {
            .Location = New Point(15, 25), .Size = New Size(470, 240), .ReadOnly = True,
            .EnableHeadersVisualStyles = False, .BackgroundColor = Color.White, .BorderStyle = BorderStyle.Fixed3D,
            .GridColor = Color.FromArgb(210, 225, 240), .RowHeadersVisible = False, .AllowUserToAddRows = False,
            .SelectionMode = DataGridViewSelectionMode.FullRowSelect, .AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill
        }
        dgvHistory.ColumnHeadersDefaultCellStyle.BackColor = Color.FromArgb(180, 205, 230)
        dgvHistory.ColumnHeadersDefaultCellStyle.Font = New Font("Segoe UI", 9.0F, FontStyle.Bold)
        dgvHistory.DefaultCellStyle.SelectionBackColor = Color.FromArgb(51, 153, 255)
        dgvHistory.AlternatingRowsDefaultCellStyle.BackColor = Color.FromArgb(240, 248, 255)
        gbTabel.Controls.Add(dgvHistory)

        btnHapusRiwayat = New Button With {
            .Text = "❌ Hapus Semua Riwayat", .Location = New Point(15, 275), .Size = New Size(470, 32),
            .BackColor = Color.White, .Cursor = Cursors.Hand, .FlatStyle = FlatStyle.Standard
        }
        AddHandler btnHapusRiwayat.Click, AddressOf btnHapusRiwayat_Click
        gbTabel.Controls.Add(btnHapusRiwayat)

        ' GRAFIK 1: Suhu & Jarak (Atas - Tinggi 380px)
        grafikSensor = New Chart With {.Location = New Point(530, 20), .Size = New Size(550, 380)}
        Me.Controls.Add(grafikSensor)

        ' GRAFIK 2: Getaran SW-420 (Bawah - Tinggi 205px)
        grafikGetaran = New Chart With {.Location = New Point(530, 410), .Size = New Size(550, 205)}
        Me.Controls.Add(grafikGetaran)
    End Sub

    ' ==========================================
    ' BAGIAN 2: KONFIGURASI DUAL CHART (REAL-TIME)
    ' ==========================================
    Private Sub InisialisasiGrafikRealTime()
        ' --- 1. SETUP GRAFIK SUHU & JARAK ---
        grafikSensor.Series.Clear()
        grafikSensor.ChartAreas.Clear()
        grafikSensor.Legends.Clear()
        grafikSensor.Titles.Clear()

        Dim legendaUtama As New Legend("LegendaUtama") With {
            .Docking = Docking.Top, .Alignment = StringAlignment.Center,
            .Font = New Font("Segoe UI", 9.0F, FontStyle.Bold), .BackColor = Color.Transparent
        }
        grafikSensor.Legends.Add(legendaUtama)

        Dim areaUtama As New ChartArea("AreaUtama") With {.BackColor = Color.White}
        areaUtama.AxisX.MajorGrid.LineColor = Color.LightGray
        areaUtama.AxisX.MajorGrid.LineDashStyle = ChartDashStyle.Dot
        areaUtama.AxisX.LabelStyle.Format = "HH:mm:ss"
        areaUtama.AxisX.LabelStyle.Angle = -45
        areaUtama.AxisX.IntervalType = DateTimeIntervalType.Seconds
        areaUtama.AxisX.Interval = 1

        areaUtama.AxisY.Title = "Suhu (°C)"
        areaUtama.AxisY.TitleFont = New Font("Segoe UI", 9.0F, FontStyle.Bold)
        areaUtama.AxisY.TitleForeColor = Color.FromArgb(220, 53, 69)
        areaUtama.AxisY.MajorGrid.LineColor = Color.LightGray
        areaUtama.AxisY.MajorGrid.LineDashStyle = ChartDashStyle.Dot
        areaUtama.AxisY.Minimum = 15
        areaUtama.AxisY.Maximum = 50

        areaUtama.AxisY2.Title = "Jarak Lantai (mm)"
        areaUtama.AxisY2.TitleFont = New Font("Segoe UI", 9.0F, FontStyle.Bold)
        areaUtama.AxisY2.TitleForeColor = Color.FromArgb(0, 122, 204)
        areaUtama.AxisY2.MajorGrid.Enabled = False
        areaUtama.AxisY2.Minimum = 0

        grafikSensor.ChartAreas.Add(areaUtama)

        seriesSuhu = New Series("Suhu (°C)") With {
            .ChartArea = "AreaUtama", .Legend = "LegendaUtama",
            .ChartType = SeriesChartType.Line, .BorderWidth = 3,
            .Color = Color.FromArgb(220, 53, 69), .MarkerStyle = MarkerStyle.Circle, .MarkerSize = 6,
            .XValueType = ChartValueType.DateTime, .YAxisType = AxisType.Primary
        }

        seriesJarak = New Series("Jarak Lantai (mm)") With {
            .ChartArea = "AreaUtama", .Legend = "LegendaUtama",
            .ChartType = SeriesChartType.Line, .BorderWidth = 3,
            .Color = Color.FromArgb(0, 122, 204), .MarkerStyle = MarkerStyle.Square, .MarkerSize = 6,
            .XValueType = ChartValueType.DateTime, .YAxisType = AxisType.Secondary
        }

        grafikSensor.Series.Add(seriesSuhu)
        grafikSensor.Series.Add(seriesJarak)
        grafikSensor.Titles.Add("Grafik Analisis Suhu & Jarak (Analog)")
        grafikSensor.Titles(0).Font = New Font("Segoe UI", 10.5F, FontStyle.Bold)


        ' --- 2. SETUP GRAFIK GETARAN SW-420 (OSILOSKOP DIGITAL) ---
        grafikGetaran.Series.Clear()
        grafikGetaran.ChartAreas.Clear()
        grafikGetaran.Legends.Clear()
        grafikGetaran.Titles.Clear()

        Dim areaGetaran As New ChartArea("AreaGetaran") With {.BackColor = Color.FromArgb(250, 250, 250)}
        areaGetaran.AxisX.MajorGrid.LineColor = Color.LightGray
        areaGetaran.AxisX.MajorGrid.LineDashStyle = ChartDashStyle.Dot
        areaGetaran.AxisX.LabelStyle.Format = "HH:mm:ss"
        areaGetaran.AxisX.LabelStyle.Angle = -45
        areaGetaran.AxisX.IntervalType = DateTimeIntervalType.Seconds
        areaGetaran.AxisX.Interval = 1

        areaGetaran.AxisY.Title = "SW-420"
        areaGetaran.AxisY.TitleFont = New Font("Segoe UI", 9.0F, FontStyle.Bold)
        areaGetaran.AxisY.TitleForeColor = Color.FromArgb(253, 126, 20)
        areaGetaran.AxisY.MajorGrid.LineColor = Color.LightGray
        areaGetaran.AxisY.MajorGrid.LineDashStyle = ChartDashStyle.Dot

        areaGetaran.AxisY.Minimum = -0.2
        areaGetaran.AxisY.Maximum = 1.2
        areaGetaran.AxisY.Interval = 1
        areaGetaran.AxisY.CustomLabels.Add(-0.1, 0.1, "0-Aman")
        areaGetaran.AxisY.CustomLabels.Add(0.9, 1.1, "1-Getar!")

        grafikGetaran.ChartAreas.Add(areaGetaran)

        seriesGetaran = New Series("Status Getaran") With {
            .ChartArea = "AreaGetaran",
            .ChartType = SeriesChartType.StepLine, .BorderWidth = 3,
            .Color = Color.FromArgb(253, 126, 20),
            .XValueType = ChartValueType.DateTime
        }

        grafikGetaran.Series.Add(seriesGetaran)
        grafikGetaran.Titles.Add("Monitor Stabilitas Getaran Mesin (Digital)")
        grafikGetaran.Titles(0).Font = New Font("Segoe UI", 10.0F, FontStyle.Bold)
    End Sub

    ' ==========================================
    ' BAGIAN 3: KONEKSI SOCKET TCP
    ' ==========================================
    Private Async Sub btnConnect_Click(sender As Object, e As EventArgs)
        If isConnected Then
            isConnected = False
            If client IsNot Nothing Then client.Close()
            lblStatusKoneksi.Text = "Status: Terputus"
            lblStatusKoneksi.ForeColor = Color.Black
            btnConnect.Text = "🔌 Hubungkan"
            Return
        End If

        Try
            lblStatusKoneksi.Text = "Status: Mencari ESP32..."
            btnConnect.Enabled = False

            client = New TcpClient()
            Await client.ConnectAsync(txtIP.Text, 8080)
            reader = New StreamReader(client.GetStream())
            isConnected = True

            lblStatusKoneksi.Text = "Status: TERHUBUNG"
            lblStatusKoneksi.ForeColor = Color.FromArgb(40, 167, 69)
            btnConnect.Text = "🔌 Putuskan"
            btnConnect.Enabled = True

            waktuTerakhirUpdateGrafik = DateTime.Now
            MulaiBacaData()
        Catch ex As Exception
            MessageBox.Show("Gagal terhubung ke ESP32!", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error)
            lblStatusKoneksi.Text = "Status: Terputus"
            btnConnect.Enabled = True
        End Try
    End Sub

    Private Async Sub MulaiBacaData()
        Try
            While isConnected AndAlso client.Connected
                Dim dataMasuk As String = Await reader.ReadLineAsync()
                If dataMasuk IsNot Nothing Then ProsesDataSensor(dataMasuk)
            End While
        Catch ex As Exception
            isConnected = False
            lblStatusKoneksi.Text = "Status: Terputus (Koneksi Hilang)"
            lblStatusKoneksi.ForeColor = Color.FromArgb(220, 53, 69)
            btnConnect.Text = "🔌 Hubungkan"
            If isLaguDiputar Then My.Computer.Audio.Stop() : isLaguDiputar = False
        End Try
    End Sub

    ' ==========================================
    ' BAGIAN 4: PEMROSESAN DATA SENSOR & AUDIO
    ' ==========================================
    Private Sub ProsesDataSensor(dataKasar As String)
        Dim paket() As String = dataKasar.Split(","c)

        If paket.Length = 4 Then
            Dim nilaiSuhuStr As String = paket(0).Trim()
            Dim statusSuhu As String = paket(1).Trim()
            Dim statusGetaran As String = paket(2).Trim()
            Dim nilaiJarakStr As String = paket(3).Trim()

            ' Perbarui Panel Teks Informasi
            lblSuhuAngka.Text = nilaiSuhuStr & " °C"
            lblSuhuStatus.Text = "Status Suhu : " & statusSuhu
            lblGetaranStatus.Text = "Status Getaran : " & statusGetaran
            lblJarakAngka.Text = "Jarak Lantai : " & nilaiJarakStr & " mm"

            lblSuhuStatus.ForeColor = If(statusSuhu.ToUpper().Contains("BAHAYA") OrElse statusSuhu.ToUpper().Contains("PANAS"), Color.FromArgb(220, 53, 69), Color.FromArgb(40, 167, 69))
            lblGetaranStatus.ForeColor = If(statusGetaran.ToUpper().Contains("ANOMALI") OrElse statusGetaran.ToUpper().Contains("KASAR"), Color.FromArgb(220, 53, 69), Color.FromArgb(40, 167, 69))

            ' Manajemen Pembaruan Kedua Grafik (Sesuai konstanta INTERVAL_GRAFIK_DETIK)
            If (DateTime.Now - waktuTerakhirUpdateGrafik).TotalSeconds >= INTERVAL_GRAFIK_DETIK Then
                waktuTerakhirUpdateGrafik = DateTime.Now

                Dim valSuhu As Double = Val(nilaiSuhuStr.Replace(",", "."))
                Dim valJarak As Double = Val(nilaiJarakStr)
                Dim valGetaran As Integer = If(statusGetaran.ToUpper().Contains("ANOMALI") OrElse statusGetaran.ToUpper().Contains("KASAR") OrElse statusGetaran.Trim() = "1", 1, 0)

                ' Kunci Waktu: Menghilangkan milidetik agar titik X sempurna persis per detik bulat (HH:mm:ss.000)
                Dim waktuSekarang As New DateTime(DateTime.Now.Year, DateTime.Now.Month, DateTime.Now.Day, DateTime.Now.Hour, DateTime.Now.Minute, DateTime.Now.Second)

                seriesSuhu.Points.AddXY(waktuSekarang, valSuhu)
                seriesJarak.Points.AddXY(waktuSekarang, valJarak)
                seriesGetaran.Points.AddXY(waktuSekarang, valGetaran)

                ' Menghapus titik lama jika melebihi batas MAKSIMAL_TITIK_DATA
                While seriesSuhu.Points.Count > MAKSIMAL_TITIK_DATA
                    seriesSuhu.Points.RemoveAt(0)
                    seriesJarak.Points.RemoveAt(0)
                    seriesGetaran.Points.RemoveAt(0)
                End While

                ' SINKRONISASI SKALA WAKTU ABSOLUT:
                ' Memaksa batas Minimum dan Maksimum sumbu X pada kedua grafik agar selalu identik!
                If seriesSuhu.Points.Count > 0 Then
                    Dim minX As Double = seriesSuhu.Points(0).XValue
                    Dim maxX As Double = seriesSuhu.Points(seriesSuhu.Points.Count - 1).XValue

                    ' Jika baru ada 1 titik, beri jarak buatan sedikit agar grafik tidak error
                    If minX = maxX Then
                        maxX = minX + (1.0 / 86400.0) ' Tambah 1 detik dalam format OLE Automation Date
                    End If

                    grafikSensor.ChartAreas(0).AxisX.Minimum = minX
                    grafikSensor.ChartAreas(0).AxisX.Maximum = maxX

                    grafikGetaran.ChartAreas(0).AxisX.Minimum = minX
                    grafikGetaran.ChartAreas(0).AxisX.Maximum = maxX
                End If

                grafikSensor.ChartAreas(0).RecalculateAxesScale()
                grafikGetaran.ChartAreas(0).RecalculateAxesScale()
            End If

            ' Deteksi Tingkat Bahaya untuk Alarm Audio
            Dim kondisiSekarang As String = "AMAN"
            If (statusSuhu.ToUpper().Contains("BAHAYA") OrElse statusSuhu.ToUpper().Contains("PANAS")) AndAlso (statusGetaran.ToUpper().Contains("ANOMALI") OrElse statusGetaran.ToUpper().Contains("KASAR")) Then
                kondisiSekarang = "KEDUANYA"
            ElseIf statusSuhu.ToUpper().Contains("BAHAYA") OrElse statusSuhu.ToUpper().Contains("PANAS") Then
                kondisiSekarang = "SUHU_SAJA"
            ElseIf statusGetaran.ToUpper().Contains("ANOMALI") OrElse statusGetaran.ToUpper().Contains("KASAR") Then
                kondisiSekarang = "GETARAN_SAJA"
            End If

            If kondisiSekarang = "KEDUANYA" AndAlso Not isLaguDiputar Then
                My.Computer.Audio.Play("E:\Visual Studio\repos\Bahaya_MesinCuci_Project_IoT\Rickroll Meme Template.wav", AudioPlayMode.BackgroundLoop)
                isLaguDiputar = True
            ElseIf kondisiSekarang <> "KEDUANYA" AndAlso isLaguDiputar Then
                My.Computer.Audio.Stop()
                isLaguDiputar = False
            End If

            ' Manajemen Log Database
            If kondisiSekarang <> "AMAN" AndAlso kondisiSekarang <> statusBahayaTerakhir Then
                Dim keteranganBahaya As String = ""
                If kondisiSekarang = "KEDUANYA" Then
                    keteranganBahaya = "Suhu & Getaran Kritis (Jarak: " & nilaiJarakStr & " mm)"
                ElseIf kondisiSekarang = "SUHU_SAJA" Then
                    keteranganBahaya = "Overheating - Suhu Tinggi"
                ElseIf kondisiSekarang = "GETARAN_SAJA" Then
                    keteranganBahaya = "Getaran Kasar / Unbalance (Jarak: " & nilaiJarakStr & " mm)"
                End If

                SimpanAnomali(nilaiSuhuStr, keteranganBahaya)
            End If

            statusBahayaTerakhir = kondisiSekarang
        End If
    End Sub

    ' ==========================================
    ' BAGIAN 5: ENGINE DATABASE SQLITE (iot_sensor.db)
    ' ==========================================
    Private Sub BuatDatabaseDanTabel()
        If Not File.Exists("iot_sensor.db") Then
            SQLiteConnection.CreateFile("iot_sensor.db")
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

        TampilkanHistory()
    End Sub

    Private Sub TampilkanHistory()
        Try
            Using conn As New SQLiteConnection(dbConnection)
                conn.Open()
                Dim adapter As New SQLiteDataAdapter("SELECT Tanggal, Waktu, Suhu, Keterangan FROM TabelAnomali ORDER BY ID DESC", conn)
                Dim tabelData As New DataTable()
                adapter.Fill(tabelData)

                dgvHistory.DataSource = tabelData
            End Using
        Catch ex As Exception
        End Try
    End Sub

    Private Sub btnHapusRiwayat_Click(sender As Object, e As EventArgs)
        If MessageBox.Show("Hapus seluruh catatan riwayat di database iot_sensor.db?", "Konfirmasi", MessageBoxButtons.YesNo, MessageBoxIcon.Warning) = DialogResult.Yes Then
            Using conn As New SQLiteConnection(dbConnection)
                conn.Open()
                Using cmd As New SQLiteCommand("DELETE FROM TabelAnomali;", conn)
                    cmd.ExecuteNonQuery()
                End Using
                Using cmdReset As New SQLiteCommand("DELETE FROM sqlite_sequence WHERE name='TabelAnomali';", conn)
                    cmdReset.ExecuteNonQuery()
                End Using
            End Using
            MessageBox.Show("Database berhasil dikosongkan.", "Sukses", MessageBoxButtons.OK, MessageBoxIcon.Information)
            TampilkanHistory()
        End If
    End Sub
End Class