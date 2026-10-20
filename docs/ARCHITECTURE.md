# PaperNX — Mimari notlar

Bu doküman mimari tasarım kararlarını ve sistem bileşenlerini içerir. Güncel v0.9.1 sürümü, fiziksel doğrulama durumu ve derleme yönergeleri için [STATUS.md](STATUS.md), [KNOWN_ISSUES.md](KNOWN_ISSUES.md) ve [BUILD_SWITCH.md](BUILD_SWITCH.md) belgeleri esas alınmalıdır.

PaperNX çevrimdışı, bellek tasarruflu bir C++17 uygulamasıdır:
- `src/main.cpp`: SDL olay döngüsü, ekran durum makinesi, dokunmatik hareketler ve kumanda yönlendirme ayrıştırması.
- `src/core.cpp`: Dosya tarama, meta veri ayrıştırma, doğal alfabetik/sayısal sıralama (`compareNatural`) ve kalıcı veri depoları.
- `src/mupdf_bridge.c`: C11 izolasyon sınırında MuPDF API entegrasyonu (`fz_try`/`fz_catch`), bellek havuzu denetimi, EPUB, PDF ve CBZ arşiv işleme.
- `src/typography.cpp`: SDL_ttf metin dizimi ve font önbellekleme.
- `src/page_cache.cpp`: LRU doku (texture) yaşam döngüsü ve bellek bütçesi yönetimi.

## Belge ve Konum Yönetimi

Her açık belge kendi sınırlı allocator'ına ve izole MuPDF context'ine sahiptir.
- **EPUB:** ZIP/XML ayrıştırma ile nav/cover meta verileri okunur. CSS/tema değişiminde sayfalama yeniden hesaplanır; mantıksal bölüm ve metin karakter sıra numarası saklanır (`PAPERNX-5`).
- **PDF:** Viewport boyutuna göre ölçeklenen dinamik raster dokular kullanılır (maksimum 4096 px). Bütünleşik filtreler (sepya/ters çevirme) sayfa piksellerine uygulanır.
- **CBZ (Çizgi Roman / Manga):** MuPDF ZIP arşiv API'si (`fz_open_archive`) üzerinden okunur. Arşiv içindeki resim dosyaları doğal sayısal sıralama (`compareNatural`) ile dizilir. Her sayfa orijinal RGB renk uzayında, metin/kağıt filtresi uygulanmadan ve 16 MiB'lık sıkı bellek güvenlik sınırıyla işlenir. İlk sayfa otomatik olarak kapak küçük resmi (thumbnail) olarak çıkarılır.

## Doğal Sayısal Sıralama (Natural Sort)

Kitaplık taramasında ve CBZ sayfa diziliminde standart `strcmp` yerine taşma korumalı doğal sayısal sıralama (`compareNatural`) kullanılır. Bu sayede "Bölüm 2", "Bölüm 10"dan önce doğru sırayla listelenir; çok ciltli seriler ve sayısal adlandırılmış çizgi roman sayfaları alfabetik karmaşa olmadan sıralanır.

## Kumanda ve Yönlendirme Ayrıştırması

Ekran saat yönünde veya tersinde dikey moda (Portrait) döndürüldüğünde, menüler ve kütüphane ızgarası görsel ekran yönünü takip eder. Ancak okuyucu modunda D-pad Sol/Sağ ve L/R omuz tuşları doğrudan mantıksal sayfa değiştirme işlemi yürütür; böylece dikey tutuşta sayfa geçişleri dikey kaydırmaya (pan) dönüşmez.

## Kalıcılık ve Veri Güvenliği

Tüm kalıcı depolar (`progress.txt`, `catalog.txt`, `reading-stats.txt`) FNV-1a checksum korumalıdır. Dosya yazma işlemleri önce `.tmp` dosyasına yapılıp fsync ile diske yazılır, byte doğrulaması yapıldıktan sonra eski dosya `.bak` olarak yedeklenip atomik olarak yer değiştirilir.

## Derleme Sistemi

- **Desktop & Sanitizers:** CMake 3.16+, Ninja, Clang/GCC (ASan/UBSan destekli).
- **Nintendo Switch:** devkitA64 / libnx, Cortex-A57 optimizasyonları, pozisyondan bağımsız yürütme (PIE) ve AArch64 ELF/NRO paketleme.

