#ifndef MEATFILE_STREAM_WRITER_H
#define MEATFILE_STREAM_WRITER_H

#include "buffered_io.h"

class StreamWriter: public BufferedWriter {
public:
    char delimiter = '\n';

    StreamWriter(MOstream* os) : BufferedWriter(os) { 
    };

    bool print(std::string line) {
        MBufferConst buffer(line);
        return write(&buffer);        
    }

    bool printLn(std::string line) {
        MBufferConst buffer(line+delimiter);
        return write(&buffer);    
    }
};

class StreamReader: public BufferedReader {
    int buffPos;
    std::string lineBuilder;

public:
    char delimiter = '\n';

    StreamReader(MIstream* is) : BufferedReader(is), buffPos(0), lineBuilder("") { 
        //read();
    };
    
    StreamReader(const std::function<int(uint8_t* buf, size_t size)>& fn) : BufferedReader(fn), buffPos(0), lineBuilder("") {
        //read();
    }

    bool eof() {
        return buffPos >= mbuffer.length() && BufferedReader::eof();
    }

    std::string readLn() {
        if(buffPos==0 && mbuffer.length()==0 && BufferedReader::eof()) {
            //Serial.printf("FSTEST: returning from first condition\n");
            return "";
        }
//tu dodać read dla lambdowego
        lineBuilder="";
//Serial.printf("FSTEST: wchodze do realn przy buffPos=%d, buf.len=%d, eof=%d\n", buffPos, mbuffer.length(), eof());
        do {
            // we read through the whole buffer? Let's get more!
            if(buffPos >= mbuffer.length() && !BufferedReader::eof()) {
//Serial.printf("FSTEST: doczytywanie bufora\n");
                buffPos=0;
                read();
            }

            for(; buffPos<mbuffer.length(); buffPos++) {
                if(mbuffer[buffPos]==delimiter) {
                    //Serial.printf("delimiter '%c'found, buffer pos %d:\n", delimiter, buffPos);
                    buffPos++;
                    return lineBuilder;
                } else {
                    //Serial.printf("%d:",buffPos);
                    lineBuilder+=mbuffer[buffPos];
                }
            }
        } while(!eof());

        return lineBuilder;
    };

};

#endif