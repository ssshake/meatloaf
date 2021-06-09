#include "http.h"
/********************************************************
 * File impls
 ********************************************************/

bool HttpFile::isDirectory() {
    // hey, why not?
    return m_path.back() == '/';
};

MIstream* HttpFile::inputStream() {
    // has to return OPENED stream
    MIstream* istream = new HttpIStream(m_path);
    istream->open();   
    return istream;
} ; 

MIstream* HttpFile::createIStream(MIstream* is) {
    return is; // we've overriden istreamfunction, so this one won't be used
}

MOstream* HttpFile::outputStream() {
    // has to return OPENED stream
    MOstream* ostream = new HttpOStream(m_path);
    ostream->open();   
    return ostream;
} ; 

time_t HttpFile::getLastWrite() {
    return 0; // might be taken from Last-Modified, probably not worth it
} ;

time_t HttpFile::getCreationTime() {
    return 0; // might be taken from Last-Modified, probably not worth it
} ;

bool HttpFile::exists() {
    Serial.printf("\nHttpFile::exists: [%s]\n", m_path.c_str());
    // we may try open the stream to check if it exists
    std::unique_ptr<MIstream> test(inputStream());
    // remember that MIStream destuctor should close the stream!
    return test->isOpen();
} ; 

size_t HttpFile::size() {
    // we may take content-lenght from header if exists
    std::unique_ptr<MIstream> test(inputStream());

    size_t size = 0;

    if(test->isOpen())
        size = test->available();

    test->close();

    return size;
};

// void HttpFile::addHeader(const String& name, const String& value, bool first, bool replace) {
//     //m_http.addHeader
// }


/********************************************************
 * Ostream impls
 ********************************************************/

bool HttpOStream::seek(uint32_t pos, SeekMode mode) {};
bool HttpOStream::seek(uint32_t pos) {};
size_t HttpOStream::position() {};
void HttpOStream::close() {
    m_http.end();
};
bool HttpOStream::open() {
    // we'll ad a lambda that will allow adding headers
    // m_http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    bool initOk = m_http.begin(m_client, m_path.c_str());
Serial.printf("URLSTR: someRc=%d\n", initOk);
    if(!initOk)
        return false;
    
    //int httpCode = m_http.PUT(); //Send the request
//Serial.printf("URLSTR: httpCode=%d\n", httpCode);
    // if(httpCode != 200)
    //     return false;

    m_isOpen = true;
    m_file = m_http.getStream();  //Get the response payload as Stream
};
//size_t HttpOStream::write(uint8_t) {};
size_t HttpOStream::write(const uint8_t *buf, size_t size) {
    m_file.write(buf, size);
};

void HttpOStream::flush() {
    m_file.flush();
};
bool HttpOStream::isOpen() {
    return m_isOpen;
};


/********************************************************
 * Istream impls
 ********************************************************/

bool HttpIStream::seek(uint32_t pos, SeekMode mode) {
    // maybe we can use http resume here?

};

bool HttpIStream::seek(uint32_t pos) {
    if(isFriendlySkipper) {
        // then we can
        // Range: bytes=666-
        // to implement seeking
        // we should also add:
        // Keep-Alive: timeout=5, max=1000
        m_http.addHeader("range","bytes=pos.toString");
        int httpCode = m_http.GET(); //Send the request
        Serial.printf("URLSTR: httpCode=%d\n", httpCode);
        if(httpCode != 200)
            return false;

        Serial.printf("\nHttpIStream::open: [%s]\n", m_path.c_str());
        m_file = m_http.getStream();  //Get the response payload as Stream
        m_position = pos;
        m_bytesAvailable = m_length-pos;
        return true;
    } else {
        if(pos==m_position)
            return true;
        if(pos<m_position) {
            // skipping backward, let's do it from the start...
            m_http.end();
            bool op = open();
            if(!op)
                return false;
        }

        // ... and then:
        // read until we reach pos
        // but we can skip forward and then modify these accordingly:
        //     m_bytesAvailable-=bytesRead;
        // m_position+=bytesRead;

        return true;
    }
};

size_t HttpIStream::position() {
    return m_position;
};

void HttpIStream::close() {
    ledOFF();
    m_http.end();
};

bool HttpIStream::open() {
    bool initOk = m_http.begin(m_client, m_path.c_str());
Serial.printf("URLSTR: someRc=%d\n", initOk);
    if(!initOk)
        return false;
    
    int httpCode = m_http.GET(); //Send the request
Serial.printf("URLSTR: httpCode=%d\n", httpCode);
    if(httpCode != 200)
        return false;

    // Accept-Ranges: bytes - if we get such header from any request, good!
    isFriendlySkipper = m_http.header("accept-ranges") == "bytes";
    m_isOpen = true;
    Serial.printf("\nHttpIStream::open: [%s]\n", m_path.c_str());
    m_file = m_http.getStream();  //Get the response payload as Stream
    m_length = m_http.getSize();
Serial.printf("URLSTR: length=%d\n", m_length);
    m_bytesAvailable = m_length;
};

int HttpIStream::available() {
    return m_bytesAvailable;
};

size_t HttpIStream::read(uint8_t* buf, size_t size) {
    auto bytesRead= m_file.readBytes((char *) buf, size);
    m_bytesAvailable-=bytesRead;
    m_position+=bytesRead;
    ledToggle(true);
    return bytesRead;
};

bool HttpIStream::isOpen() {
    return m_isOpen;
};
