Amaç: Firma içi Ar-Ge süreçlerinde kullanılmak üzere, PCAN (Peak Systems) muadili, maliyet etkin ve özelleştirilebilir bir analizör donanımı ve yazılımı geliştirmek.
Haberleşme Arayüzü: PC tarafındaki özgün arayüz programı ile UART protokolü üzerinden tam entegrasyon sağlanması.
Protokol Desteği: CAN Bus ve LIN Bus veri paketlerinin yakalanması, analiz edilmesi ve PC'ye aktarılması.
Güç Elektroniği: Kart üzerine entegre edilen High-Side MOSFET yapıları sayesinde, harici besleme altında PWM sinyalleri ile yük (aktüatör/motor vb.) kontrolü yeteneğinin sisteme kazandırılması.

Donanım Mimarisi:
Mikrodenetleyici: STM32F042C6T6 (Seçim Kriteri: Çift UART ve entegre CAN donanım desteği).

Haberleşme Arayüzleri:
CAN: TJA1042 Transceiver (Endüstriyel standart uyumluluğu).
LIN: ATA663254 Transceiver (Dahili 5V regülatör avantajı ile kompakt tasarım).

Lin Hattı Teknik Değerlendirme ve Revizyon Hedefleri:
Mevcut tasarım, LIN protokolünde Slave modunda aktif olarak çalışmaktadır.
Geliştirme: Tam kapsamlı analizör fonksiyonu için sonraki revizyonlarda karta "Master/Slave" mod geçişi özelliği eklenecektir.
Çözüm: Master modunda gerekli olan pull-up direnci ve ters akım koruma diyodu, bir MOSFET anahtarlama devresi ile hatta dahil edilerek yazılımsal olarak mod seçimi sağlanacaktır.

Haberleşme Alt Yapısı:
Uygulanan Çözüm: CP2102 entegresi ile UART-USB dönüşümü.
Teknik Kazanım/Analiz: Proje sürecinde, STM32'nin Virtual COM Port (USB CDC) yeteneği incelenmiş olup, 
harici dönüştürücü (CP2102) kullanımının sonraki versiyonlarda kaldırılarak BOM (Malzeme Listesi) maliyetinin düşürülebileceği 
ve PCB alanından tasarruf edilebileceği sonucuna varılmıştır.

Sürücü Katı ve PWM Modülasyonu:
PWM Kaynağı: IS32FL3265A (I2C Kontrollü LED Sürücü).
Sürüş Topolojisi: LED sürücünün çok kanallı yapısından faydalanılarak MOSFET'ler için PWM sinyal jeneratörü olarak kullanılması.
Kontrol Mantığı:
Sürücü Yapısı: Current Sink (Akımı toprağa çeken yapı).
Devre Şeması: MOSFET Gate hattı Pull-Up direnci ile 3.3V referansına bağlanmıştır.
Çalışma Prensibi: Pull-up ile varsayılan olarak "Aktif" olan MOSFET, sürücü entegresi sinyali GND'ye çektiğinde "Pasif" konuma geçerek PWM modülasyonunu gerçekleştirir.

Güç Mimarisi:
Ana Besleme: LM317 tabanlı lineer regülasyon ve harici/USB kaynaklar arasında otomatik geçiş sağlayan diyot tabanlı (OR-ing) koruma yapısı.

Step-Up (Boost) Konvertör:
Sorun: ATA663254 LIN transceiver entegresinin 5V ve 28V sınırlarında çalışması sebebiyle, USB beslemesinde gerilimin sınırda kalması ve kararsızlık riski.
Çözüm: MC34063AD entegresi ile tasarlanan yükseltici devre sayesinde gerilim 13.5V seviyesine çıkarılmıştır.
Yükseltilen gerilim hattı sadece düşük akım gereksinimi olan transceiver modüllerini ve mikro denetleyiciyi beslediği için, bu topoloji maliyet ve performans açısından en uygun çözüm olarak seçilmiştir.

Güç Mimarisi İyileştirme ve Tasarım Analizi:
Mevcut Durum: MCU'nun, sistemin kalanıyla birlikte Boost konvertör hattından beslenmesi.
Revizyon Hedefi: MCU beslemesinin USB VBUS hattı üzerinden izole edilmesi.
Tasarım Kısıtı (Dropout Voltage Problemi):
USB VBUS (5V) hattı, koruma diyotlarından geçtikten sonra yaklaşık 4.3V seviyesine düşmektedir.
LM317 regülatörünün 3.3V çıkış verebilmesi için giriş geriliminin en az 4.55V (Vout + Vdropout = 3.3V + 1.25V) olması gerekmektedir.
Mevcut fark (1V), regülatörün kararlı çalışması için yetersiz kalmıştır.
Nihai Çözüm: Ekstra bir regülatör maliyetine girmeden, CP2102 UART dönüştürücünün entegre 3.3V (LDO) çıkış pini kullanılarak MCU'nun temiz ve kararlı bir şekilde beslenmesi sağlanabilir 
fakat USB CDC haberleşmesi kullanılıp uart dönüştürücü kaldırılmak istenirse faklı bir çözüme gidilmeli.

Bu proje staj döneminin sonunda verildiği için sistemde istenenlerden sadece can analizör tamamlanabilmiştir ve başarılı bir şekilde çalışmaktadır. 

Yüksek Performanslı Veri Akışı (DMA & Ping-Pong Buffering): Sistem, 1.5 Mbps baudrate hızında çalışan UART hattında veri kaybını sıfıra indirmek için 
DMA ve Double Buffering mimarisi kullanmaktadır. (HAL_UART_RxHalfCplt) ve (HAL_UART_RxCplt) kesmeleri ile gelen veri paketleri (16-byte), 
işlemciyi bloklamadan arka planda işlenmekte ve (RxData_Processing) tamponuna güvenli bir şekilde aktarılmaktadır.

Dinamik Konfigürasyon Yönetimi: Cihaz yeniden başlatmaya gerek kalmadan, gelen veri paketinin ilk baytı üzerinden 
CAN Bus hızını (125k, 250k, 500k, 1M) dinamik olarak değiştirebilmektedir. Bu yapı, HAL_CAN_Stop, HAL_CAN_Init ve HAL_CAN_Start 
fonksiyonlarının çalışma sırasında yönetilmesiyle sağlanmıştır.

Özel Haberleşme Protokolü: PC ve MCU arasında veri bütünlüğünü sağlamak amacıyla 16 baytlık sabit paket yapısına 
sahip özel bir binary protokol tasarlanmıştır. Protokol; Header (0xAA), Footer (0x55), ID Tipi (Std/Ext), DLC ve Payload verilerini içerir.

Filtresiz (Promiscuous) Mod: Analizör fonksiyonu gereği, donanım filtreleri (Filter Bank) tüm CAN ID'lerini (Mask: 0x0000) kabul edecek şekilde ayarlanmış, 
böylece hattaki tüm trafiğin izlenmesi sağlanmıştır.

Şu an Header/Footer kontrolü var, bir sonraki versiyonda CRC ekleyerek bit hatalarının da yakalanması planlanmaktadır.
