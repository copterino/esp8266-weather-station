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
        m_capacity(bufCapacity),
        m_buffer((char*)malloc(bufCapacity)),
        m_head(m_buffer + 1),  // start from +1 character because we will make -1 in Print()
        m_size(0)
    {
        m_buffer[0] = 0;
    }

    ~DebugStream()
    {
        if (m_buffer)
            free(m_buffer);
    }

    template <typename... MyArgs>
    void printf(const char* format, MyArgs&&... args)
    {
        const int freeSize = m_capacity - m_size - 1;  // 1 for zero
        //DebugPrint("Print - size: %d freeSize: %d\n", m_size, freeSize);
        if (!m_buffer || freeSize <= 0)
            return;

        // minus one means we erase last null terminator to make one solid string
        int res = snprintf(m_head - 1, freeSize, format, args...);
        if (res < 0)
        {
            DebugPrint("printf format parse error.\n", freeSize, res);
            return;
        }
        else if (res >= freeSize)
        {
            if (m_capacity - 1 > res)
            {
                DebugPrint("Buffer is small: %d, %d is needed.\nDoing buffer reset and retry.\n", freeSize, res);
                clear();
                printf(format, args...);
            }
            return;
        }

        m_size += res;
        m_head = m_buffer + m_size;
        //DebugPrint("Print - res: %d, size: %d, head: '%d'\n", res, m_size, *m_head);
        //DebugPrint("Print - buf: '%s'\n", m_buffer);
    }

    const uint8_t* data()
    {
        if (m_size == 0)
            return nullptr;

        return (uint8_t*)m_buffer;
    }

    void clear()
    {
        m_size = 0;
        m_head = m_buffer + 1;
    }

    bool available() { return m_size > 0; }
    int  size() { return m_size; }

private:
    const int m_capacity;
    char* m_buffer;
    char* m_head;
    int   m_size;
};

template <typename... Args>
void DebugPrint(DebugStream& stream, const char* format, Args&&... args)
{
#ifdef _DEBUG
    Serial.printf(format, args...);
#endif
    stream.printf(format, args...);
}

#endif  // _DEBUGSTREAM_H_
