<p align="center">
  <img src="assets/branding/PaperNX-icon-512.png" alt="PaperNX Logo" width="160">
</p>

<h1 align="center">PaperNX v0.9.3 — Türkçe Dokümantasyon</h1>

<p align="center">
  <strong>Nintendo Switch için yerel, çevrimdışı EPUB, PDF, CBZ ve CBR okuyucu.</strong>
</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/Lisans-AGPL_3.0-blue.svg" alt="AGPL-3.0 Lisansı"></a>
  <a href="https://github.com/ysfbz/PaperNx/releases/tag/v0.9.3"><img src="https://img.shields.io/badge/S%C3%BCr%C3%BCm-v0.9.3-brightgreen.svg" alt="Sürüm v0.9.3"></a>
  <img src="https://img.shields.io/badge/Platform-Nintendo_Switch-e60012.svg" alt="Platform Nintendo Switch">
  <img src="https://img.shields.io/badge/Dil-C%2B%2B17%20%2F%20C11-00599C.svg" alt="Dil C++17 / C11">
  <img src="https://img.shields.io/badge/Grafik-SDL2-black.svg" alt="Grafik SDL2">
  <img src="https://img.shields.io/badge/Motor-MuPDF_1.23.11-red.svg" alt="Motor MuPDF">
</p>

---

> [!IMPORTANT]
> v0.9.3 tema ve çizgi roman sayfalama değişiklikleri ana makinedeki C++/CTest paketini geçti (32/32). Switch AArch64 derlemesi ve NRO metadata kontrolleri beklemede; fiziksel Switch davranışı doğrulanmadı.

## Genel Bakış

**PaperNX**, Nintendo Switch için modern C++17, C11, SDL2 ve MuPDF kullanılarak geliştirilmiş, **yerel ve tamamen çevrimdışı bir doküman ve çizgi roman okuyucusudur**. Joy-Con kumandaları konsoldan ayrılmış durumdayken bile tek elle ya da masaüstü modunda rahatça kitap ve çizgi roman okuyabilmeniz amacıyla dokunmatik öncelikli olarak tasarlanmıştır.

Tüm kitaplarınız, çizgi roman arşivleriniz, kapak önbelleğiniz, okuma geçmişiniz ve okuma günlüğü istatistikleriniz yalnızca konsolunuzun microSD kartında saklanır. Kullanıcı hesabı, bulut bağlantısı, telemetri veya web tarayıcı motoru gerektirmez.

> [!NOTE]
> **Varsayılan Dil:** PaperNX temiz kurulumda varsayılan olarak **İngilizce** başlar. Eksiksiz **Türkçe** arayüz desteği dahili olarak mevcuttur ve **Ayarlar → Uygulama → Dil** (`+` butonu veya <kbd>Y</kbd>) üzerinden istendiğinde seçilebilir. Tercihiniz anında kaydedilir ve konsolu/uygulamayı yeniden başlattığınızda korunur.

---

## Temel Özellikler

- **Dokunmatik ve Kumanda Kontrolleri:** Kitaplık, menüler ve okuma hareketleri doğrudan dokunmatik girişleri destekler; Joy-Con ve Pro Controller tuşları da kısayol olarak kullanılabilir.
- **Varsayılan Tek Sayfa:** Sayfalı EPUB/PDF tek sayfayla açılır; EPUB/PDF ve çizgi romanlarda isteğe bağlı çift sayfa, çizgi romanlarda akıllı spread ve geniş sayfayı tek gösterme kullanılabilir.
- **CBZ & CBR Çizgi Roman Desteği:** `.cbz` (ZIP) ve `.cbr` (RAR4, RAR5 ve solid arşivler, bütünleşik `unarr` kütüphanesiyle) formatlarında tam çizgi roman okuma desteği. Yanlış adlandırılmış arşivleri sihirli baytlarla algılama, orijinal RGB renklerini koruma, isteğe bağlı geri alınabilir okuma paleti, 48 MiB görsel ve 100 MP piksel güvenlik sınırları.
- **İnteraktif 3D Sayfa Çevirme (Page Curl):** Fiziksel silindir geometrisiyle çalışan, parmak takip eden, tutuş yüksekliği (`anchorY`) ve dikey çekme (`dragY`) dinamiklerini yansıtan, arka yüz kağıt tonlaması, parlama ve temas gölgesi içeren gerçekçi sayfa kıvrılma animasyonu. Sayfalı EPUB, PDF, CBZ ve CBR (RTL manga dahil) belgelerde çalışır, **Ayarlar → Navigasyon** üzerinden kapatılıp açılabilir.
- **Çizgi Roman Seri Yönetimi ve 3 Sekmeli Gezinti:** Çok sayılı çizgi roman serilerini (`#1`, `01`, `v01`, `Vol 01`, `ComicInfo.xml`) ana kütüphanede tek bir seri kartı altında toplar. 3 sekmeli (`Bölümler`, `Tüm sayfalar`, `Yer imleri`) içindekiler modalı, arka planda küçük resim üretimi ve canlı önizleme baloncuğuna sahip sayfa gezinti çubuğu (scrubber).
- **Doğal Sayısal Sıralama (Natural Sort):** Cilt ve bölüm numaralarını doğru sırada dizen taşma korumalı `compareNatural` algoritması (Örn: Cilt 2, Cilt 10'dan önce doğru sıralanır).
- **Gelişmiş Kumanda ve Ekran Yönü Uyumu:** Dikey modlarda ekrana göre yönlenen analog kaydırma ve menü gezintisi; okuyucuda dikey tutuşta dahi doğrudan yatay sayfa çeviren D-pad kontrolleri; ZL/ZR tetik tuşlarıyla pürüzsüz sürekli yakınlaştırma; ekrandaki "Sayfaya sığdır" butonuyla anında 1.0x görünüme dönüş.
- **Gerçek Dikey Sürekli EPUB Kaydırma:** Sahte sayfa hissi olmadan akıcı dikey piksel kaydırma, ayarlanabilir momentum ve pürüzsüz bölüm geçişleri. Klasik sayfalı yatay okuma modu da eksiksiz korunur.
- **Hızlı Menü ve Ekran Ayarları:** Dokunmatik `+` menüsü Joy-Con Plus tuşuyla aynıdır. Natural Paper ve Soft E-Ink görünümleri, yazılımsal karartma ve desteklenen elde kullanım parlaklık ayarı sunulur.
- **Asenkron Kitaplık Tarama:** Kök dizinler arka planda taranır; iptal veya eksik tarama son başarılı kitaplık envanterini değiştirmez.
- **Güçlü PDF Motoru:** MuPDF 1.23.11 tabanlı; genişliğe/sayfaya sığdırma, serbest analog ve dokunmatik kaydırma, yakınlaştırma ve belge renk filtreleri (Orijinal, Koyu, Sepya, Ters).
- **Bütünleşik Kâğıt Görünümü:** On kâğıt teması (Saf Kâğıt, Yumuşak Fildişi, Sıcak Krem, Doğal Kâğıt, Nostaljik Sepya, Keten, Serin Kâğıt, Kömür Gecesi, OLED Siyah, Yumuşak E-Mürekkep). Kitaplık, menüler ve okuma arayüzü varsayılan olarak genel okuma paletini takip eder; isteğe bağlı koyu/açık arayüz ve vurgu renkleri korunur.
- **Çift Dilli Arayüz:** Doğal Türkçe ve İngilizce dil desteği. Kitap içerikleri ve içindekiler orijinal dilinde korunur (İbranice/BiDi metin dizimi proje tasarımı gereği hariç tutulmuştur).
- **Disk Üzerinde Kapak Önbelleği:** 150×210 RGB24 kapaklar için 48 MiB / 512 kayıt sınırı, dosya bilgisi ve CRC denetimi; ayrıca bellekteki küçük resim texture cache'i 32 MiB ile sınırlıdır. Kapak yüklenene kadar vektörel yer tutucu görünür.
- **Okuma Günlüğü ve Çevrimdışı İstatistikler:** Okuma süresi, oturumlar, tamamlama oranları ve günlük hedefleri tamamen yerel olarak takip eder.
- **Doğrulanmış Manuel Güncelleme:** TLS, SHA-256, NRO/NACP kimliği ve sürüm denetiminden geçen aday staging alanına yazılır. Kurulu NRO'yu değiştirme adımı dosya yöneticisiyle manuel yapılır.
- **Veri Koruma ve Kurtarma:** Geçici dosya yazımı, `.bak` yedekleme ve otomatik kurtarma mekanizmaları veri kaybı riskini asgariye indirir. Paket kullanıcı veritabanlarını içermez.

---

## Kurulum ve Güncelleme

### Temiz Kurulum

1. Güncel sürüm arşivini [`PaperNX-switch-v0.9.3.zip`](https://github.com/ysfbz/PaperNx/releases/download/v0.9.3/PaperNX-switch-v0.9.3.zip) indirin.
2. Arşivi Switch microSD kartınızın ana dizinine (`sdmc:/`) çıkartın.
3. `.epub`, `.pdf`, `.cbz` ve `.cbr` dosyalarınızı `sdmc:/switch/PaperNX/books/` dizinine kopyalayın.
4. Homebrew Menu üzerinden **PaperNX** uygulamasını başlatın.

#### SHA-256 Sağlama Toplamları (v0.9.3)

| Dosya | SHA256 sidecar |
|---|---|
| [`PaperNX.nro`](https://github.com/ysfbz/PaperNx/releases/download/v0.9.3/PaperNX.nro) | [`PaperNX.nro.sha256`](https://github.com/ysfbz/PaperNx/releases/download/v0.9.3/PaperNX.nro.sha256) |
| [`PaperNX-switch-v0.9.3.zip`](https://github.com/ysfbz/PaperNx/releases/download/v0.9.3/PaperNX-switch-v0.9.3.zip) | [`PaperNX-switch-v0.9.3.zip.sha256`](https://github.com/ysfbz/PaperNx/releases/download/v0.9.3/PaperNX-switch-v0.9.3.zip.sha256) |
| [`PaperNX-licenses-v0.9.3.zip`](https://github.com/ysfbz/PaperNx/releases/download/v0.9.3/PaperNX-licenses-v0.9.3.zip) | [`PaperNX-licenses-v0.9.3.zip.sha256`](https://github.com/ysfbz/PaperNx/releases/download/v0.9.3/PaperNX-licenses-v0.9.3.zip.sha256) |

```text
sdmc:/switch/PaperNX/
├── PaperNX.nro          # Uygulama ikili dosyası (fontlar ve assetler gömülüdür)
├── books/               # EPUB, PDF, CBZ ve CBR kitaplarınızı buraya koyun
└── fonts/               # Font dosyaları ve OFL lisans metni
```

### Önceki Sürümden Güncelleme

Önce `books/` ve `data/` klasörlerinizi yedekleyip PaperNX'i kapatın. Doğrulanmış v0.9.3 Switch ZIP'ini microSD kartın ana dizinine (`sdmc:/`) açın ve içindeki NRO'yu `sdmc:/switch/PaperNX/PaperNX.nro` ile değiştirin. Yeni sürüm başarıyla açılana kadar eski NRO'nun bir kopyasını saklayın. Uygulama içi güncelleme de doğrulanmış adayı staging alanına alır; dosyayı uygulama kendisi kurmaz veya yeniden başlatmaz.

> [!IMPORTANT]
> Dağıtım paketi uygulamayı, fontları ve boş bir `books/` yer tutucusunu içerir; kitaplarınızı, `data/` klasörünü, ayarları, ilerlemeyi, yer imlerini, Okuma Günlüğünü veya logları içermez ve üzerine yazmaz. Yine de güncellemeden önce `books/` ve `data/` klasörlerinizi yedekleyin.

---

## Dokunmatik ve Kumanda Kısayolları

| İşlem | Dokunmatik Yol | Kumanda Kısayolu |
|---|---|---|
| **Hızlı Menü / İşlemler** | Sağ üstteki `+` butonuna dokunun | <kbd>+</kbd> (Plus) |
| **Kitap Açma** | Kitap kartına veya gezgindeki satıra dokunun | <kbd>A</kbd> |
| **Sayfa Çevirme (Sayfalı EPUB/PDF/CBZ/CBR)** | Sağ/sol %30 kenara dokunun veya kaydırın | <kbd>L</kbd> / <kbd>R</kbd> veya Yön Tuşları Sol / Sağ |
| **Sürekli EPUB Kaydırma** | Dikey olarak yukarı / aşağı sürükleyin | Sağ Çubuk veya Yön Tuşları Yukarı / Aşağı |
| **Araç Çubuğunu Göster/Gizle** | Ekranın ortasına dokunun | <kbd>A</kbd> |
| **Okuma Ayarları (Aa)** | `+` → *Okuma ayarları* | <kbd>X</kbd> |
| **Uygulama Görünümü ve Dil** | `+` → *Uygulama görünümü* | <kbd>Y</kbd> |
| **İçindekiler** | `+` → *İçindekiler* | <kbd>-</kbd> (Minus) |
| **Yer İmleri** | `+` → *Yer imleri* | <kbd>ZL</kbd> / Sol Omuz |
| **PDF/CBZ/CBR Yakınlaştırma ve Kaydırma** | Çubukta −/+, büyütülmüş sayfayı sürükleyin | <kbd>ZL</kbd> / <kbd>ZR</kbd>; Sol veya Sağ Çubuk |
| **Favorilere Ekleme** | Kitap kartındaki yıldıza dokunun | Sol Çubuk Tıklama (<kbd>L3</kbd>) |
| **Kitaplığa Dönüş** | `+` → *Kitaplığa dön* | <kbd>B</kbd> |
| **Uygulamadan Çıkış** | `+` → *PaperNX’ten çık* | <kbd>Home</kbd> / <kbd>B</kbd> (ana ekranda) |

---

## Kaynak Koddan Derleme

PaperNX, Docker kullanarak her ortamda tekrarlanabilir derlemeler sağlar:

```bash
git clone https://github.com/ysfbz/PaperNx.git
cd PaperNx

# Nintendo Switch NRO ve paket derlemesi
docker compose run --rm -e PAPERNX_RELEASE_VERSION=v0.9.3 switch bash -lc 'scripts/verify-switch.sh'

# Masaüstü testleri ve sanitizer doğrulamaları
docker compose run --rm desktop bash scripts/verify-desktop.sh
docker compose run --rm desktop bash scripts/verify-sanitizers.sh
```

---

## Geliştirici ve Telif Hakkı

**Yusuf Buzrul**  
- Yusuf Buzrul tarafından yapay zekâ desteğiyle geliştirilmiştir.
- GitHub: [@ysfbz](https://github.com/ysfbz) ([https://github.com/ysfbz](https://github.com/ysfbz))  
- Depo: [https://github.com/ysfbz/PaperNx](https://github.com/ysfbz/PaperNx)
- Lisans: [GNU AGPL-3.0](LICENSE); kaynak kökeni ve üçüncü taraf bildirimleri: [docs/LICENSES.md](docs/LICENSES.md), [docs/SOURCE_ATTRIBUTION.md](docs/SOURCE_ATTRIBUTION.md)

