#!/bin/bash

# Test Script for 15 Concurrent Client Connections
# Bu script 15 farklı client'ı eş zamanlı olarak bağlar ve test eder
# Mevcut server_log.txt ve trial.txt dosyalarını kullanır

SERVER_IP="127.0.0.1"
SERVER_PORT="8080"
TEST_ROOM="testroom"

echo "=========================================="
echo "15 Eş Zamanlı Client Bağlantı Testi"
echo "=========================================="
echo "Server IP: $SERVER_IP"
echo "Server Port: $SERVER_PORT"
echo "Test Room: $TEST_ROOM"
echo "Test dosyası: trial.txt (mevcut dosya)"
echo ""

# trial.txt dosyasının varlığını kontrol et
if [ ! -f "trial.txt" ]; then
    echo "❌ trial.txt dosyası bulunamadı!"
    exit 1
fi

echo "✅ trial.txt dosyası kullanılıyor"
echo ""

# Server'ı başlat (arka planda)
echo "Server başlatılıyor..."
./chatserver $SERVER_PORT &
SERVER_PID=$!

# Server'ın başlaması için bekle
sleep 2

echo "Server PID: $SERVER_PID"
echo ""

# 15 client'ı eş zamanlı başlat
echo "15 client eş zamanlı başlatılıyor..."
for i in {1..15}; do
    (
        # Her client için ayrı bir terminal session'da çalıştır
        echo "Client $i başlatılıyor..."
        
        # Client'ı başlat ve otomatik komutları gönder
        (
            echo "client$i"                    # Username
            sleep 1
            echo "/join $TEST_ROOM"            # Odaya katıl
            sleep 1
            echo "/broadcast Merhaba! Ben client$i"  # Mesaj gönder
            sleep 2
            echo "/sendfile trial.txt client$((i % 15 + 1))"  # trial.txt dosyasını gönder
            sleep 3
            echo "/broadcast Test mesajı $i"   # İkinci mesaj
            sleep 2
            echo "/exit"                       # Çık
        ) | timeout 30s ./chatclient $SERVER_IP $SERVER_PORT > /dev/null 2>&1
        
        echo "Client $i tamamlandı"
    ) &
done

echo ""
echo "Tüm client'lar başlatıldı. Test sürüyor..."
echo ""

# Tüm client'ların tamamlanmasını bekle
wait

echo ""
echo "=========================================="
echo "Test Tamamlandı!"
echo "=========================================="

# Sonuçları analiz et
echo "Test Sonuçları:"
echo "---------------"

# Bağlantı sayısını kontrol et
CONNECTIONS=$(grep -c "CONNECT" server_log.txt 2>/dev/null || echo "0")
echo "Toplam bağlantı sayısı: $CONNECTIONS"

# Başarılı bağlantıları kontrol et
SUCCESSFUL=$(grep -c "user 'client" server_log.txt 2>/dev/null || echo "0")
echo "Başarılı bağlantı sayısı: $SUCCESSFUL"

# Oda katılımlarını kontrol et
JOINS=$(grep -c "joined room" server_log.txt 2>/dev/null || echo "0")
echo "Oda katılım sayısı: $JOINS"

# Dosya transferlerini kontrol et
FILE_TRANSFERS=$(grep -c "FILE-QUEUE" server_log.txt 2>/dev/null || echo "0")
echo "Dosya transfer sayısı: $FILE_TRANSFERS"

echo ""
echo "Server log dosyası: server_log.txt"
echo "Server log satır sayısı: $(wc -l < server_log.txt 2>/dev/null || echo "0")"

# Server'ı durdur
echo "Server durduruluyor..."
kill $SERVER_PID 2>/dev/null

echo ""
echo "Test tamamlandı!" 