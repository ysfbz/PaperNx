#include "papernx/localization.hpp"
#include <cassert>
#include <iostream>
using papernx::localizeUiText;
int main() {
    assert(localizeUiText("İçindekiler",0)=="İçindekiler");
    assert(localizeUiText("İçindekiler",1)=="Contents");
    assert(localizeUiText("Sürekli Kaydırma · Yukarı / aşağı",1)=="Continuous Scroll · Up / down");
    assert(localizeUiText("Yayıncı stilleri: Kapalı",1)=="Publisher styles: Off");
    assert(localizeUiText("Sıralama: Değiştirilme",1)=="Sort: Modified");
    assert(localizeUiText("Yazı tipi: Yayıncı",1)=="Font: Publisher");
    assert(localizeUiText("PDF filtresi: Ters renkler",1)=="PDF filter: Invert colors");
    assert(localizeUiText("−    Kaydırma hassasiyeti: %125    +",1)=="−    Scroll sensitivity: %125    +");
    assert(localizeUiText("-    Yazı boyutu: 26 px    +",1)=="-    Font size: 26 px    +");
    assert(localizeUiText("Vazgeç",1)=="Cancel");
    assert(localizeUiText("Bölüm 2 · 34 / 152",1)=="Chapter 2 · 34 / 152");
    assert(localizeUiText("12 kitap · EPUB ve PDF · Çevrimdışı",1)=="12 books · EPUB and PDF · Offline");
    assert(localizeUiText("Klasör   İstanbul / Şiir",1)=="Folder   İstanbul / Şiir");
    assert(localizeUiText("Kitap    İçindekiler.epub",1)=="Book    İçindekiler.epub");
    assert(localizeUiText("Klasör   ",1)=="Folder   ");
    assert(localizeUiText("Kitap    ",1)=="Book    ");
    assert(localizeUiText("Klasör   ",0)=="Klasör   ");
    assert(localizeUiText("Kitap    ",0)=="Kitap    ");
    assert(localizeUiText("Kitap açılamadı: çığlık.epub",1)=="Could not open book: çığlık.epub");
    // Unknown proper titles and document content must remain unchanged; callers
    // must additionally bypass this UI-only API for titles identical to UI labels.
    const std::string title="İstanbul Masalları — Çığlık ve Şiir";
    assert(localizeUiText(title,1)==title);
    assert(localizeUiText("EPUB",1)=="EPUB");
    assert(localizeUiText("Disk hits/misses",0)=="Disk isabet/kaçırma");
    assert(localizeUiText("Disk isabet/kaçırma",1)=="Disk hits/misses");
    assert(localizeUiText("PaperNX'ten çık",1)=="Exit PaperNX");
    assert(localizeUiText("Yazı boyutu",1)=="Font size");
    assert(localizeUiText("Satır aralığı",1)=="Line height");
    assert(localizeUiText("Paragraf aralığı",1)=="Paragraph spacing");
    assert(localizeUiText("Kenar boşluğu",1)=="Margins");
    assert(localizeUiText("Kaydırma hassasiyeti",1)=="Scroll sensitivity");
    assert(localizeUiText("Sayfa dokunma alanı",1)=="Page touch zone");
    assert(localizeUiText("Yakınlaştırma",1)=="Zoom");
    assert(localizeUiText("Düşük kontrast",1)=="Low contrast");
    assert(localizeUiText("Metin rengi",1)=="Text color");
    assert(localizeUiText("Arka plan rengi",1)=="Background color");
    assert(localizeUiText("Metin rengi · Renk paleti",1)=="Text color · Color palette");
    assert(localizeUiText("Arka plan · Renk paleti",1)=="Background · Color palette");
    assert(localizeUiText("Ekran yönü",1)=="Display orientation");
    assert(localizeUiText("Günlüğüm",1)=="My journal");
    assert(localizeUiText("Kitabı sil",1)=="Delete book");
    assert(localizeUiText("Update verified and saved as PaperNX.nro.new.",0)=="Güncelleme doğrulandı ve PaperNX.nro.new olarak kaydedildi.");
    assert(localizeUiText("Güncelleme klasörü",1)=="Update folder");
    assert(localizeUiText("Close",0)=="Kapat");
    assert(localizeUiText("Kitaplık",42)=="Kitaplık");
    assert(localizeUiText("Kitaplık",0)=="Kitaplık");

    // Reading presets and journal export
    assert(localizeUiText("15 dk",1)=="15 min");
    assert(localizeUiText("30 dk",1)=="30 min");
    assert(localizeUiText("45 dk",1)=="45 min");
    assert(localizeUiText("15 dk",0)=="15 dk");
    assert(localizeUiText("Hata: JSON aktarılamadı",1)=="Error: Could not export JSON");
    assert(localizeUiText("Hata: CSV aktarılamadı",1)=="Error: Could not export CSV");

    // Settings scope and shortcuts
    assert(localizeUiText("Genel tema · Kitaplığa uygulanır",1)=="Global theme · Applied to library");
    assert(localizeUiText("Bu kitabın teması · Genel temaya dön",1)=="This book's theme · Return to global");
    assert(localizeUiText("Genel temayı kullanıyor · Bu kitabı özelleştir",1)=="Using global theme · Customize this book");
    assert(localizeUiText("Sayfalar arası yakınlaştırma: Açık",1)=="Preserve zoom across pages: On");
    assert(localizeUiText("Sayfalar arası yakınlaştırma: Kapalı",1)=="Preserve zoom across pages: Off");
    assert(localizeUiText("L/R sayfa kısayolu: Açık",1)=="L/R page shortcuts: On");
    assert(localizeUiText("L/R sayfa kısayolu: Kapalı",1)=="L/R page shortcuts: Off");
    assert(localizeUiText("Sayfa çevirme animasyonu: Açık",1)=="Page curl animation: On");
    assert(localizeUiText("Sayfa çevirme animasyonu: Kapalı",1)=="Page curl animation: Off");

    // Live preview paragraphs
    assert(localizeUiText("Bir sayfa, bir dünya",1)=="One page, one world");
    assert(localizeUiText("İstanbul’da güneş doğarken, çığlık çığlığa uçan kuşlar şehrin üstünde süzülüyordu.",1)==
           "As the morning sun rises over the horizon, birds soar peacefully across the waking city.");
    assert(localizeUiText("Öğrenmek, düşünmek ve hayal kurmak için bir sayfa daha çevir. Güzel bir gün!",1)==
           "Turn another page to discover, imagine, and learn. It is a wonderful day to read!");

    // Bidirectional / error messages
    assert(localizeUiText("Settings could not be saved",0)=="Ayarlar kaydedilemedi");
    assert(localizeUiText("Library folder unavailable",0)=="Kitap klasörüne erişilemiyor");
    assert(localizeUiText("Kitap klasörüne erişilemiyor",1)=="Library folder unavailable");
    assert(localizeUiText("Settings could not be saved",1)=="Settings could not be saved");
    assert(localizeUiText("Ayarlar kaydedilemedi",1)=="Settings could not be saved");
    assert(localizeUiText("Kitap bilgileri kaydedilemedi",1)=="Book metadata could not be saved");
    assert(localizeUiText("Okuma hedefi kaydedilemedi",1)=="Reading goal could not be saved");
    assert(localizeUiText("Reading goal could not be saved",0)=="Okuma hedefi kaydedilemedi");
    assert(localizeUiText("Kaydedilemedi. SD kartı kontrol edin; kayıt yeniden denenecek.",1)==
           "Could not save. Check the SD card; saving will retry.");
    assert(localizeUiText("Reading status unchanged: progress write failed.",0)==
           "Okuma durumu değişmedi: ilerleme yazılamadı.");
    assert(localizeUiText("Status rollback failed. Check SD card before changing books.",0)==
           "Durum geri alınamadı. Kitap değiştirmeden önce SD kartı kontrol edin.");
    assert(localizeUiText("Portrait output unavailable; landscape restored",0)==
           "Dikey çıktı kullanılamıyor; yatay görünüme dönüldü");
    assert(localizeUiText("Reading journal not saved: disk full",0)=="Okuma günlüğü kaydedilemedi: disk full");
    assert(localizeUiText("Catalog could not be saved: write error",0)=="Katalog kaydedilemedi: write error");

    // Prefixes
    assert(localizeUiText("Son okuma: 2026-10-19",1)=="Last read: 2026-10-19");
    assert(localizeUiText("İlerleme: %50",1)=="Progress: %50");
    assert(localizeUiText("Bugün: 30",1)=="Today: 30");
    assert(localizeUiText("Toplam: 150",1)=="Total: 150");
    assert(localizeUiText("Tahmini kalan: 20",1)=="Estimated remaining: 20");
    assert(localizeUiText("Geri getir: Bölüm 2",1)=="Restore: Bölüm 2");

    std::cout << "localization tests passed\n";
}
