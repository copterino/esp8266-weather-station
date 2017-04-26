#ifndef _DEBUGSTREAM_H_
#define _DEBUGSTREAM_H_

#include <Arduino.h>

#define _DEBUG

template <typename... Args>
void DebugPrint(const char* format, Args&&... args)
{
#ifdef _DEBUG
    Serial.printf(format, args...);
#endif
}

class DebugStream
{
public:
    DebugStream(int bufCapacity) :
        buffer((char*)malloc(bufCapacity)),
        head(buffer + 1),  // start from +1 character because we will make -1 in Print()
        capacity(bufCapacity),
        size(0)
    {
        buffer[0] = 0;
    }

    ~DebugStream()
    {
        if (buffer)
            free(buffer);
    }

    template <typename... MyArgs>
    void Print(const char* format, MyArgs&&... args)
    {
        const int freeSize = capacity - size - 1;  // 1 for zero
        //DebugPrint("Print - size: %d freeSize: %d\n", size, freeSize);
        if (!buffer || freeSize <= 0)
            return;

        // minus one means we erase last null terminator to make one solid string
        int res = snprintf(head - 1, freeSize, format, args...);
        if (res < 0 || res >= freeSize)
        {
            DebugPrint("MsgSize is too small, %d needed.\n", res);
            return;
        }

        size += res;
        head = buffer + size;
        //DebugPrint("Print - res: %d, size: %d, head: '%d'\n", res, size, *head);
        //DebugPrint("Print - buf: '%s'\n", buffer);
    }

    const uint8_t* PopData()
    {
        if (size == 0)
            return nullptr;

        size = 0;
        head = buffer + 1;
        return (uint8_t*)buffer;
    }
    
    int GetSize() { return size; }

private:
    char* buffer;
    char* head;
    int   capacity;
    int   size;
};

template <typename... Args>
void DebugPrint(DebugStream& stream, const char* format, Args&&... args)
{
#ifdef _DEBUG
    Serial.printf(format, args...);
#endif
    stream.Print(format, args...);
}

#endif  // _DEBUGSTREAM_H_
