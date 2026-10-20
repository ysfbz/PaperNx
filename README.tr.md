<p align="center">
  <img src="assets/branding/PaperNX-icon-512.png" alt="PaperNX Logo" width="160">
</p>

<h1 align="center">PaperNX — Türkçe Dokümantasyon</h1>

<p align="center">
  <strong>Nintendo Switch için yerel, çevrimdışı EPUB, PDF ve CBZ okuyucu.</strong>
</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/Lisans-AGPL_3.0-blue.svg" alt="AGPL-3.0 Lisansı"></a>
  <a href="https://github.com/ysfbz/PaperNx/releases/tag/v0.9.1"><img src="https://img.shields.io/badge/S%C3%BCr%C3%BCm-v0.9.1-brightgreen.svg" alt="Sürüm v0.9.1"></a>
  <img src="https://img.shields.io/badge/Platform-Nintendo_Switch-e60012.svg" alt="Platform Nintendo Switch">
  <img src="https://img.shields.io/badge/Dil-C%2B%2B17%20%2F%20C11-00599C.svg" alt="Dil C++17 / C11">
  <img src="https://img.shields.io/badge/Grafik-SDL2-black.svg" alt="Grafik SDL2">
  <img src="https://img.shields.io/badge/Motor-MuPDF_1.23.11-red.svg" alt="Motor MuPDF">
</p>

---

## Genel Bakış

**PaperNX**, Nintendo Switch için modern C++17, C11, SDL2 ve MuPDF kullanılarak sıfırdan geliştirilmiş, **yerel ve tamamen çevrimdışı bir doküman ve çizgi roman okuyucusudur**. Joy-Con kumandaları konsoldan ayrılmış durumdayken bile tek elle ya da masaüstü modunda rahatça kitap ve çizgi roman okuyabilmeniz amacıyla dokunmatik öncelikli olarak tasarlanmıştır.

Tüm kitaplarınız, çizgi roman arşivleriniz, kapak önbelleğiniz, okuma geçmişiniz ve okuma günlüğü istatistikleriniz yalnızca konsolunuzun microSD kartında saklanır. Kullanıcı hesabı, bulut bağlantısı, telemetri veya web tarayıcı motoru gerektirmez.

> [!NOTE]
> **Varsayılan Dil:** PaperNX temiz kurulumda varsayılan olarak **İngilizce** başlar. Eksiksiz **Türkçe** arayüz desteği dahili olarak mevcuttur ve **Ayarlar → Uygulama → Dil** (`+` butonu veya <kbd>Y</kbd>) üzerinden istendiğinde seçilebilir. Tercihiniz anında kaydedilir ve konsolu/uygulamayı yeniden başlattığınızda korunur.

---

## Temel Özellikler

- **Dokunmatik Odaklı Ergonomi:** Tüm okuma ve menü eylemleri doğrudan ve hassas dokunmatik hedeflerle yönetilir (minimum 56px dokunmatik alan ve dokunulduğunda görsel basılma tepkisi). Joy-Con ve Pro Controller tuşları isteğe bağlı kısayol olarak tam desteklenir.
- **CBZ Çizgi Roman Desteği:** MuPDF altyapısıyla hiçbir harici bağımlılık veya gereksiz kod yükü getirmeden `.cbz` arşivlerini doğrudan okuma. Sayfaları 16 MiB sıkı bellek sınırıyla işleme, çizgi roman çizimlerini bozmayan orijinal RGB renklendirme ve ilk sayfadan kapak resmi çıkarma.
- **Doğal Sayısal Sıralama (Natural Sort):** Cilt ve bölüm numaralarını doğru sırada dizen taşma korumalı `compareNatural` algoritması (Örn: Cilt 2, Cilt 10'dan önce doğru sıralanır).
- **Gelişmiş Kumanda ve Ekran Yönü Uyumu:** Dikey modlarda ekrana göre yönlenen analog kaydırma ve menü gezintisi; okuyucuda dikey tutuşta dahi doğrudan yatay sayfa çeviren D-pad kontrolleri; tetik tuşlarının kazara menü değiştirmesini önleyen koruma.
- **Gerçek Dikey Sürekli EPUB Kaydırma:** Sahte sayfa hissi olmadan akıcı dikey piksel kaydırma, ayarlanabilir momentum ve pürüzsüz bölüm geçişleri. Klasik sayfalı yatay okuma modu da eksiksiz korunur.
- **Güçlü PDF Motoru:** MuPDF 1.23.11 tabanlı; genişliğe/sayfaya sığdırma, serbest analog ve dokunmatik kaydırma, yakınlaştırma ve belge renk filtreleri (Orijinal, Koyu, Sepya, Ters).
- **Bütünleşik Kâğıt Görünümü:** Dokuz doğal kâğıt teması (Beyaz, Kitap Kâğıdı, Klasik Sepya, Krem, Koyu Kömür, Gece vb.). Kitaplık, araç çubukları ve okuma pencereleri ortak renk paletini paylaşır.
- **Çift Dilli Arayüz:** Doğal Türkçe ve İngilizce dil desteği. Kitap içerikleri ve içindekiler orijinal dilinde korunur.
- **Disk Üzerinde Kapak Önbelleği:** 150×210 RGB24 boyutunda, 32 MiB / 512 dosya ile sınırlandırılmış hızlı kapak önbelleği; sağlama toplamı doğrulaması ve anında beliren şık vektörel kapak yer tutucuları.
- **Okuma Günlüğü ve Çevrimdışı İstatistikler:** Okuma süresi, oturumlar, tamamlama oranları ve günlük hedefleri tamamen yerel olarak takip eder.
- **Veri Koruma ve Kurtarma:** Geçici dosya yazımı, `.bak` yedekleme ve otomatik kurtarma mekanizmaları veri kaybı riskini asgariye indirir. Paket kullanıcı veritabanlarını içermez.

---

## Kurulum ve Güncelleme

### Temiz Kurulum

1. Güncel sürüm arşivini [`PaperNX-switch-v0.9.1.zip`](https://github.com/ysfbz/PaperNx/releases/download/v0.9.1/PaperNX-switch-v0.9.1.zip) indirin.
2. Arşivi Switch microSD kartınızın ana dizinine (`sdmc:/`) çıkartın.
3. `.epub`, `.pdf` ve `.cbz` dosyalarınızı `sdmc:/switch/PaperNX/books/` dizinine kopyalayın.
4. Homebrew Menu üzerinden **PaperNX** uygulamasını başlatın.

```text
sdmc:/switch/PaperNX/
├── PaperNX.nro          # Uygulama ikili dosyası (fontlar ve assetler gömülüdür)
├── books/               # EPUB, PDF ve CBZ kitaplarınızı buraya koyun
└── fonts/               # Font dosyaları ve OFL lisans metni
```

### Önceki Sürümden Güncelleme

Yeni `PaperNX.nro` dosyasını `sdmc:/switch/PaperNX/PaperNX.nro` üzerine kopyalamanız yeterlidir.

> [!IMPORTANT]
> Dağıtım paketi yalnızca uygulama dosyaları, fontlar ve boş bir `books/` yer tutucusu içerir. Asla `data/` klasörünü içermez; bu sayede mevcut yer imleriniz, okuma istatistikleriniz ve özel temalarınız korunur. Güncelleme öncesinde `books/` ve `data/` klasörlerinizi yedeklemeniz önerilir.

---

## Dokunmatik ve Kumanda Kısayolları

| İşlem | Dokunmatik Yol | Kumanda Kısayolu |
|---|---|---|
| **Hızlı Menü / İşlemler** | Sağ üstteki `+` butonuna dokunun | <kbd>+</kbd> (Plus) |
| **Kitap Açma** | Kitap kartına veya gezgindeki satıra dokunun | <kbd>A</kbd> |
| **Sayfa Çevirme (Sayfalı EPUB/PDF/CBZ)** | Sağ/sol %30 kenara dokunun veya kaydırın | <kbd>L</kbd> / <kbd>R</kbd> veya Yön Tuşları Sol / Sağ |
| **Sürekli EPUB Kaydırma** | Dikey olarak yukarı / aşağı sürükleyin | Sağ Çubuk veya Yön Tuşları Yukarı / Aşağı |
| **Araç Çubuğunu Göster/Gizle** | Ekranın ortasına dokunun | <kbd>A</kbd> |
| **Okuma Ayarları (Aa)** | `+` → *Okuma ayarları* | <kbd>X</kbd> |
| **Uygulama Görünümü ve Dil** | `+` → *Uygulama görünümü* | <kbd>Y</kbd> |
| **İçindekiler** | `+` → *İçindekiler* | <kbd>-</kbd> (Minus) |
| **Yer İmleri** | `+` → *Yer imleri* | <kbd>ZL</kbd> / Sol Omuz |
| **PDF/CBZ Yakınlaştırma ve Kaydırma** | Çubukta −/+, büyütülmüş sayfayı sürükleyin | <kbd>ZL</kbd> / <kbd>ZR</kbd>; Sol veya Sağ Çubuk |
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
docker compose run --rm switch bash scripts/verify-switch.sh

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
- Lisans: [GNU AGPL-3.0](LICENSE)

