#ifndef MEATFILESYSTEM_WRAPPERS_LEGACY_BUFFERS
#define MEATFILESYSTEM_WRAPPERS_LEGACY_BUFFERS

//#define RECORD_SIZE 256
#define BUFFER_SIZE 512

#include "../meat_io.h"
#include "meat_stream.h"
#include "../../include/global_defines.h"

/********************************************************
 * A buffer
 ********************************************************/

class MBuffer {
    bool handmade = false;

protected:
    int len = 0;
    char* buffer; // will point to buffered reader internal buffer

public:
    MBuffer() {};

    MBuffer(char* start, size_t size) {
        buffer = start;
        len = size;
    }

    MBuffer(int size) {
        handmade = true;
        buffer = new char[size];
    };

    ~MBuffer() {
        if(handmade)
            delete [] buffer;
    }

    char& operator [](int idx) {
        return buffer[idx];
    }

    uint8_t getByte() {
        auto b = buffer[0];
        buffer++;
        len--;
        return b;
    }    

    int length() {
        return len;
    }

    char* getBuffer() {
        return buffer;
    }

    friend class BufferedReader;
};


/********************************************************
 * Buffered reader
 ********************************************************/

class BufferedReader {
    MIStream* istream = nullptr;
    std::function<int(uint8_t* buf, size_t size)> readFn;

    uint8_t rawBuffer[BUFFER_SIZE] = { 0 };

protected:
    MBuffer smartBuffer;
    bool eofOccured = false;
    size_t m_available = 0;

    virtual void refillBuffer() {
        if(istream != nullptr) {
            smartBuffer.len = istream->read((uint8_t*)rawBuffer, BUFFER_SIZE);
            Debug_printv("Refilling buffer by stream, got %d bytes!", smartBuffer.len);

        }
        else {
            smartBuffer.len = readFn((uint8_t*)rawBuffer, BUFFER_SIZE);
            Debug_printv("Refilling buffer by lambda, got %d bytes!", smartBuffer.len);
        }

        smartBuffer.buffer = (char *)rawBuffer;
        eofOccured = (smartBuffer.len == 0) || (istream != nullptr && istream->available() == 0);
    }

public:
    BufferedReader() {}; 

    BufferedReader(const std::function<int(uint8_t* buf, size_t size)>& fn) : readFn(fn) {

    };

    virtual MBuffer* read() {
        if(!eofOccured)
            refillBuffer();

        m_available-=smartBuffer.length();

        return &smartBuffer;
    }

    virtual uint8_t readByte() {
        if(smartBuffer.length()==0 && !eofOccured)
            refillBuffer();

        m_available--;

        return smartBuffer.getByte();
    }

    bool eof() {
        return eofOccured;
    }

    virtual size_t available() {
        return m_available;
    }
};

/********************************************************
 * Stream reader
 ********************************************************/

class LinedReader: public BufferedReader {
    int buffPos;
    std::string lineBuilder;

public:
    char delimiter = '\n';
    
    LinedReader(const std::function<int(uint8_t* buf, size_t size)>& fn) : BufferedReader(fn), buffPos(0), lineBuilder("") {}

    virtual ~LinedReader() {}

    bool eof() {
        return buffPos >= smartBuffer.length() && BufferedReader::eof();
    }

    std::string readLn() {
        if(buffPos==0 && smartBuffer.length()==0 && BufferedReader::eof()) {
            Debug_printv("EOF!");

            return "";
        }
        lineBuilder="";
        do {
            // we read through the whole buffer? Let's get more!
            if(buffPos >= smartBuffer.length() && !BufferedReader::eof()) {
                buffPos=0;
                read();
            }

            for(; buffPos<smartBuffer.length(); buffPos++) {
                if(smartBuffer[buffPos]==delimiter) {
                    buffPos++;
                    return lineBuilder;
                } else {
                    // if(codec != nullptr)
                    //     lineBuilder+=codec->toLocal(smartBuffer[buffPos]);
                    // else
                        lineBuilder+=smartBuffer[buffPos];
                }
            }
        } while(!eof());

        return lineBuilder;
    };
};


#endif /* MEATFILESYSTEM_WRAPPERS_LEGACY_BUFFERS */