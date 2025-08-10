#!/bin/bash

# Test Script for File Upload Queue (5 concurrent uploads max)
# Bu script 10 farklı client'tan eş zamanlı dosya gönderimi yapar
# İlk 5 dosya anında yüklenmeli, kalan 5 dosya kuyruğa alınmalı
# Mevcut server_log.txt ve trial.txt dosyalarını kullanır

SERVER_IP="127.0.0.1"
SERVER_PORT="8080"
TEST_ROOM="filetest"

echo "=========================================="
echo "Dosya Yükleme Kuyruğu Testi"
echo "=========================================="
echo "Server IP: $SERVER_IP"
echo "Server Port: $SERVER_PORT"
echo "Test Room: $TEST_ROOM"
echo "Test dosyası: trial.txt (mevcut dosya)"
echo "Beklenen: İlk 5 dosya anında, sonraki 5 dosya kuyruğa alınacak"
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

# Önce 10 client'ı bağla ve odaya katıl
echo "10 client bağlanıyor ve odaya katılıyor..."
for i in {1..10}; do
    (
        echo "Client $i bağlanıyor..."
        (
            echo "fileclient$i"                # Username
            sleep 1
            echo "/join $TEST_ROOM"            # Odaya katıl
            sleep 2
        ) | timeout 10s ./chatclient $SERVER_IP $SERVER_PORT > /dev/null 2>&1
    ) &
done

# Tüm client'ların bağlanmasını bekle
wait
sleep 2

echo ""
echo "Tüm client'lar bağlandı. Dosya gönderimi başlıyor..."
echo ""

# Şimdi 10 dosyayı eş zamanlı gönder (hepsi trial.txt)
echo "10 dosya eş zamanlı gönderiliyor (trial.txt)..."
for i in {1..10}; do
    (
        echo "Dosya $i gönderiliyor..."
        (
            echo "fileclient$i"                # Username
            sleep 1
            echo "/join $TEST_ROOM"            # Odaya katıl
            sleep 1
            # Dosyayı farklı bir client'a gönder
            target_client=$((i % 10 + 1))
            echo "/sendfile trial.txt fileclient$target_client"
            sleep 5  # Dosya transfer süresi için bekle
            echo "/exit"
        ) | timeout 30s ./chatclient $SERVER_IP $SERVER_PORT > /dev/null 2>&1
        
        echo "Dosya $i gönderimi tamamlandı"
    ) &
done

echo ""
echo "Tüm dosya gönderimleri başlatıldı. Test sürüyor..."
echo ""

# Tüm transferlerin tamamlanmasını bekle
wait

echo ""
echo "=========================================="
echo "Dosya Kuyruğu Testi Tamamlandı!"
echo "=========================================="

# Sonuçları analiz et
echo "Test Sonuçları:"
echo "---------------"

# Kuyruk durumunu kontrol et
QUEUE_ENTRIES=$(grep -c "FILE-QUEUE" server_log.txt 2>/dev/null || echo "0")
echo "Kuyruğa alınan dosya sayısı: $QUEUE_ENTRIES"

# Anında işlenen dosyaları kontrol et (semaphore başarılı)
IMMEDIATE=$(grep -c "added to queue" server_log.txt 2>/dev/null || echo "0")
echo "Anında kuyruğa alınan dosya sayısı: $IMMEDIATE"

# Kuyruk dolu mesajlarını kontrol et
QUEUE_FULL=$(grep -c "queue is full" server_log.txt 2>/dev/null || echo "0")
echo "Kuyruk dolu mesajı sayısı: $QUEUE_FULL"

# Bekleme sürelerini kontrol et
WAIT_TIMES=$(grep -c "waiting for" server_log.txt 2>/dev/null || echo "0")
echo "Bekleme mesajı sayısı: $WAIT_TIMES"

# Başarılı transferleri kontrol et
SUCCESSFUL_TRANSFERS=$(grep -c "Successfully transferred" server_log.txt 2>/dev/null || echo "0")
echo "Başarılı transfer sayısı: $SUCCESSFUL_TRANSFERS"

echo ""
echo "Detaylı Kuyruk Analizi:"
echo "----------------------"

# Kuyruk mesajlarını göster
echo "Kuyruk mesajları:"
grep "FILE-QUEUE" server_log.txt 2>/dev/null | tail -10

echo ""
echo "Bekleme mesajları:"
grep "waiting for" server_log.txt 2>/dev/null | tail -5

echo ""
echo "Transfer tamamlama mesajları:"
grep "started upload after" server_log.txt 2>/dev/null | tail -5

echo ""
echo "Server log dosyası: server_log.txt"
echo "Server log satır sayısı: $(wc -l < server_log.txt 2>/dev/null || echo "0")"

# Server'ı durdur
echo "Server durduruluyor..."
kill $SERVER_PID 2>/dev/null

echo ""
echo "Dosya kuyruğu testi tamamlandı!" 