//
// Created by Matej on 18.12.2015.
//

#ifndef DIALER2_CIRCULARBUFFER_H
#define DIALER2_CIRCULARBUFFER_H

#include <pebble.h>

#define CIRCULAR_BUFFER_SIZE 13 //To keep it simple, have all buffers be same size

typedef struct
{
    bool loaded[CIRCULAR_BUFFER_SIZE];
    char* data;
    int8_t bufferCenterPos;
    uint16_t centerIndex;
    size_t singleEntrySize;

} CircularBuffer;

CircularBuffer* cb_create(size_t singleEntrySize);
void* cb_getEntry(CircularBuffer* buffer, uint16_t index);
void* cb_getEntryForFilling(CircularBuffer* buffer, uint16_t index);
bool cb_isLoaded(CircularBuffer* buffer, uint16_t index);
void cb_destroy(CircularBuffer* buffer);
void cb_shift(CircularBuffer* buffer, uint16_t newIndex);
uint8_t cb_getNumOfLoadedSpacesDownFromCenter(CircularBuffer* buffer, uint16_t limit);
uint8_t cb_getNumOfLoadedSpacesUpFromCenter(CircularBuffer* buffer);
void cb_clear(CircularBuffer* buffer);


#endif //DIALER2_CIRCULARBUFFER_H
