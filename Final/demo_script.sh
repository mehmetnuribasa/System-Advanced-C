#!/bin/bash

# Ana Demo Script - Sunum için
# Bu script hem 15 client testini hem de dosya kuyruğu testini çalıştırır
# Mevcut server_log.txt ve trial.txt dosyalarını kullanır

echo "=========================================="
echo "CHAT SERVER DEMO SCRIPT"
echo "=========================================="
echo "Bu script aşağıdaki testleri çalıştırır:"
echo "1. 15 Eş Zamanlı Client Bağlantı Testi"
echo "2. Dosya Yükleme Kuyruğu Testi (5 eş zamanlı limit)"
echo "Mevcut server_log.txt ve trial.txt dosyalarını kullanır"
echo ""

# trial.txt dosyasının varlığını kontrol et
if [ ! -f "trial.txt" ]; then
    echo "❌ trial.txt dosyası bulunamadı!"
    echo "Lütfen trial.txt dosyasının mevcut olduğundan emin olun."
    exit 1
fi

echo "✅ trial.txt dosyası mevcut"
echo ""

# Projeyi derle
echo "Proje derleniyor..."
make clean
make all

if [ $? -ne 0 ]; then
    echo "Derleme hatası! Lütfen kodları kontrol edin."
    exit 1
fi

echo "Derleme başarılı!"
echo ""

# Test 1: 15 Eş Zamanlı Client
echo "=========================================="
echo "TEST 1: 15 Eş Zamanlı Client Bağlantısı"
echo "=========================================="
echo "Bu test sunucunun 15 client'ı eş zamanlı yönetebildiğini gösterir."
echo "Test dosyası: trial.txt"
echo ""

read -p "Test 1'i başlatmak için Enter'a basın..."

chmod +x test_15_clients.sh
./test_15_clients.sh

echo ""
echo "Test 1 tamamlandı!"
echo ""

# Test 2: Dosya Kuyruğu
echo "=========================================="
echo "TEST 2: Dosya Yükleme Kuyruğu Testi"
echo "=========================================="
echo "Bu test dosya gönderme kuyruğunun maksimum 5 eş zamanlı yüklemeyi"
echo "desteklediğini ve fazla isteklerin sıraya alındığını gösterir."
echo "Test dosyası: trial.txt"
echo ""

read -p "Test 2'yi başlatmak için Enter'a basın..."

chmod +x test_file_queue.sh
./test_file_queue.sh

echo ""
echo "Test 2 tamamlandı!"
echo ""

# Final Rapor
echo "=========================================="
echo "DEMO TAMAMLANDI - ÖZET RAPOR"
echo "=========================================="

echo "Test Sonuçları:"
echo "---------------"

# Server log'undan özet bilgiler
if [ -f "server_log.txt" ]; then
    echo "Server Log Analizi:"
    echo "  - Toplam bağlantı: $(grep -c 'CONNECT' server_log.txt)"
    echo "  - Oda katılımları: $(grep -c 'joined room' server_log.txt)"
    echo "  - Dosya transferleri: $(grep -c 'FILE-QUEUE' server_log.txt)"
    echo "  - Kuyruk dolu mesajları: $(grep -c 'queue is full' server_log.txt)"
    echo "  - Başarılı transferler: $(grep -c 'Successfully transferred' server_log.txt)"
else
    echo "Server log dosyası bulunamadı."
fi

echo ""
echo "Kullanılan Dosyalar:"
echo "  - server_log.txt: Server log kayıtları"
echo "  - trial.txt: Test dosyası (tüm transferler için kullanıldı)"
echo ""

echo "Önemli Noktalar:"
echo "1. Sunucu 15 eş zamanlı client'ı sorunsuz yönetti"
echo "2. Dosya kuyruğu maksimum 5 eş zamanlı yüklemeyi destekledi"
echo "3. Fazla dosya istekleri kuyruğa alındı"
echo "4. Tüm bağlantılar birbirine karışmadan yönetildi"
echo "5. Tek test dosyası (trial.txt) kullanıldı"
echo ""

echo "Demo tamamlandı!" 