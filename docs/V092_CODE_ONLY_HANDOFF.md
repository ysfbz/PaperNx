# V092 Code-Only Handoff

Durum: **IMPLEMENTED — PENDING INDEPENDENT VERIFICATION** (hiçbir test/build çalıştırılmadı; G4/G5/G6 PASS değildir).

## Commitler
- ab0f3dd updater: `prepareBoot` `.boot.ok` dalında NRO yokken tek `.old` yedeği silinmez; Ready metni "manual installation required" der.
- Sonraki commit: `LibraryScanner` iptal edilen taramayı `complete=false` yayınlar; cover perf sayaçları + tekrarlı metadata açılışı engeli.
(Son SHA için `git log`.)

## Kapsam
- G4: A/B launcher benimsenmedi (chainload/atomik rename kanıtı yok); doğrulanmış indirme + manuel kurulum korunur. `docs/UPDATER.md` zaten gerekçeyi içerir. Auto-install yok.
- G5: Tarayıcı zaten tek worker, eksik/iptal taramada envanter yazmaz; iptal sonucu artık complete işaretlenmez. Parser değişmedi (somut kusur bulunmadı).
- G6: `src/main.cpp` thumbWorkerLoop: küçük resim geçişi metadata okuduysa metadataQueue işi atlanır (`metadataCollectedKeys_`). Sayaçlar (std::atomic, relaxed): `perfMetadataReopens_`, `perfMetadataSkipped_`, `perfArchiveOpens_`, `perfDiskHits_`, `perfTextureUploads_`, `perfStaleDiscards_`. Henüz UI/log'a bağlı değil; okumak için debug çıktısı eklenmeli. Disk önbellek formatı spekülatif değiştirilmedi.

## Test ajanının doğrulayacakları
- Derleme (main.cpp, library_index.cpp, self_update.cpp), mevcut self_update/library testleri.
- Kapalı .boot.ok + NRO yok senaryosu; iptal edilen tarama sonucu UI'da boş envanter ezmemeli.
- Metadata atlama: yeni EPUB kitaplarında başlık/yazar yine de dolmalı (collectBookMetadata thumbDoc yolu).
- Cover 100/500/1000 benchmark; sayaç okuma kancası.
- Not: kullanıcıya ait tests/ değişiklikleri ve untracked dosyalar commit edilmedi.
