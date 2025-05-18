#ifndef BUFFER_H
#define BUFFER_H

#include <pthread.h>
#include <stdbool.h>

#define MAX_LINE_LENGTH 1024

typedef struct {
    char **lines;
    int capacity;
    int count;
    int head;
    int tail;

    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;

    bool finished;
} Buffer;

void buffer_init(Buffer *buffer, int capacity);
void buffer_destroy(Buffer *buffer);
void buffer_add(Buffer *buffer, const char *line);
char *buffer_get(Buffer *buffer);
void buffer_set_finished(Buffer *buffer);
bool buffer_is_finished(Buffer *buffer);

// Global buffer ve sarmalayıcı fonksiyonlar
void init_buffer(int capacity);
void destroy_buffer();
void put_line_to_buffer(const char *line);
char *get_line_from_buffer();

#endif
