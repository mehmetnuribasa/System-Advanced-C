#include "buffer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static Buffer shared_buffer;

void buffer_init(Buffer *buffer, int capacity) {
    buffer->capacity = capacity;
    buffer->count = 0;
    buffer->head = 0;
    buffer->tail = 0;
    buffer->finished = false;

    buffer->lines = malloc(sizeof(char*) * capacity);
    if (!buffer->lines) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    pthread_mutex_init(&buffer->mutex, NULL);
    pthread_cond_init(&buffer->not_empty, NULL);
    pthread_cond_init(&buffer->not_full, NULL);
}

void buffer_destroy(Buffer *buffer) {
    free(buffer->lines); // Free the allocated memory for lines

    pthread_mutex_destroy(&buffer->mutex);
    pthread_cond_destroy(&buffer->not_empty);
    pthread_cond_destroy(&buffer->not_full);
}

void buffer_add(Buffer *buffer, const char *line) {
    pthread_mutex_lock(&buffer->mutex);

    while (buffer->count == buffer->capacity) {
        pthread_cond_wait(&buffer->not_full, &buffer->mutex);
    }

    if (line == NULL) {
        buffer->lines[buffer->tail] = NULL;
    } else {
        buffer->lines[buffer->tail] = strdup(line);
    }

    buffer->tail = (buffer->tail + 1) % buffer->capacity;
    buffer->count++;

    pthread_cond_signal(&buffer->not_empty);
    pthread_mutex_unlock(&buffer->mutex);
}

char *buffer_get(Buffer *buffer) {
    pthread_mutex_lock(&buffer->mutex);

    while (buffer->count == 0 && !buffer->finished) {
        pthread_cond_wait(&buffer->not_empty, &buffer->mutex);
    }

    if (buffer->count == 0 && buffer->finished) {
        pthread_mutex_unlock(&buffer->mutex);
        return NULL;
    }

    char *line = buffer->lines[buffer->head];
    buffer->head = (buffer->head + 1) % buffer->capacity;
    buffer->count--;

    pthread_cond_signal(&buffer->not_full);
    pthread_mutex_unlock(&buffer->mutex);
    return line;
}

void buffer_set_finished(Buffer *buffer) {
    pthread_mutex_lock(&buffer->mutex);
    buffer->finished = true;
    pthread_cond_broadcast(&buffer->not_empty);
    pthread_mutex_unlock(&buffer->mutex);
}

bool buffer_is_finished(Buffer *buffer) {
    pthread_mutex_lock(&buffer->mutex);
    bool result = buffer->finished;
    pthread_mutex_unlock(&buffer->mutex);
    return result;
}

// Global wrapper functions
void init_buffer(int capacity) {
    buffer_init(&shared_buffer, capacity);
}

void destroy_buffer() {
    buffer_destroy(&shared_buffer);
}

void put_line_to_buffer(const char *line) {
    buffer_add(&shared_buffer, line);
}

char *get_line_from_buffer() {
    return buffer_get(&shared_buffer);
}
