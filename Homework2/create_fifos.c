#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#define FIFO1 "/tmp/fifo1"
#define FIFO2 "/tmp/fifo2"


int main() {
    // FIFO1 oluştur
    if (mkfifo(FIFO1, 0666) == -1 && errno != EEXIST) {
        perror("FIFO1 oluşturulamadı");
        exit(EXIT_FAILURE);
    }

    // FIFO2 oluştur
    if (mkfifo(FIFO2, 0666) == -1 && errno != EEXIST) {
        perror("FIFO2 oluşturulamadı");
        exit(EXIT_FAILURE);
    }

    printf("FIFO'lar başarıyla oluşturuldu.\n");
    return 0;
}
