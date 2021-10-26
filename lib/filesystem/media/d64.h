// .D64 - The D64 disk image format
// https://vice-emu.sourceforge.io/vice_17.html#SEC345
// https://ist.uwaterloo.ca/~schepers/formats/D64.TXT
// https://ist.uwaterloo.ca/~schepers/formats/GEOS.TXT
//

#ifndef MEATFILESYSTEM_MEDIA_D64
#define MEATFILESYSTEM_MEDIA_D64

#include "meat_io.h"
// #include "fs_littlefs.h"
// #include "MemoryInfo.h"
#include "string_utils.h"

#include "../../include/global_defines.h"

/********************************************************
 * Utility implementations
 ********************************************************/

class D64Image {

public:

    MIStream* containerStream;
    size_t entryIndex = 0;

    uint8_t dos_version;
    struct Header {
        char disk_name[16];
        char unused[2];
        char id_dos[5];
    };

    struct BAMInfo {
        uint8_t track;
        uint8_t sector;
        uint8_t offset;
        uint8_t start_track;
        uint8_t end_track;
        uint8_t byte_count;
    };

    struct BAMEntry {
        uint8_t free_sectors;
        uint8_t sectors_00_07;
        uint8_t sectors_08_15;
        uint8_t sectors_16_20;
    };

    struct Entry {
        uint8_t next_track;
        uint8_t next_sector;
        uint8_t file_type;
        uint8_t start_track;
        uint8_t start_sector;
        char filename[16];
        uint8_t rel_start_track;   // Or GOES info block start track
        uint8_t rel_start_sector;  // Or GEOS info block start sector
        uint8_t rel_record_length; // Or GEOS file structure (Sequential / VLIR file)
        uint8_t geos_type;         // $00 - Non-GEOS (normal C64 file)
        uint8_t year;
        uint8_t month;
        uint8_t day;
        uint8_t hour;
        uint8_t minute;
        uint8_t blocks[2];
    };

    D64Image(MIStream* istream) : containerStream(istream) {
        
    }

    ~D64Image() {
        Debug_printv("close");
        containerStream->close();
    }

    void fillHeader() {
        containerStream->read((uint8_t*)&header, sizeof(header));
    }

    void seekHeader() {
        seekSector(directory_header_offset, 0x90);
    }

    bool seekNextEntry() {
        return seekEntry(entryIndex + 1);
    }

    void resetEntryCounter() {
        entryIndex = 0;
    }

    uint8_t directory_header_offset[2] = {18, 0};
    uint8_t directory_list_offset[2] = {18, 1};
    BAMInfo block_allocation_map[1] = {18, 0, 0x04, 1, 35, 4};
    uint8_t sectorsPerTrack[4] = { 17, 18, 19, 21 };
    uint16_t block_size = 256;
    uint8_t sector_buffer[256] = { 0 };

    std::string file_type_label[8] = { "del", "seq", "prg", "usr", "rel", "cbm", "dir", "???" };

    uint8_t track;
    uint8_t sector;
    uint16_t offset;
    uint64_t blocks_free;

    uint8_t index = 0;  // Currently selected directory entry
    uint8_t length = 0; // Directory list entry count

    Header header;
    Entry entry;        // Directory entry data

    bool show_hidden = false;

    size_t read(uint8_t* buf, size_t size);

    void sendListing();
    void sendFile( std::string filename = "" );

    bool seekSector( uint8_t track, uint8_t sector, size_t offset = 0 );
    bool seekSector( uint8_t* trackSector, size_t offset = 0 );
    bool seekEntry( std::string filename );
    bool seekEntry( size_t index = 0 );

    std::vector<Entry> getEntries(uint8_t track, uint8_t sector);

    std::string decodeEntry();

    uint16_t blocksFree();
    std::string readBlock( uint8_t track, uint8_t sector );
    bool writeBlock( uint8_t track, uint8_t sector, std::string data );    
    bool allocateBlock( uint8_t track, uint8_t sector );
    bool deallocateBlock( uint8_t track, uint8_t sector );

	uint8_t speedZone( uint8_t track)
	{
		return (track < 30) + (track < 24) + (track < 17);
	};

    // uint8_t d64_get_type(uint16_t imgsize)
    // {
    //     switch (imgsize)
    //     {
    //         // D64
    //         case 174848:  // 35 tracks no errors
    //         case 175531:  // 35 w/ errors
    //         case 196608:  // 40 tracks no errors
    //         case 197376:  // 40 w/ errors
    //         case 205312:  // 42 tracks no errors
    //         case 206114:  // 42 w/ errors
    //             return D64_TYPE_D64;

    //         // D71
    //         case 349696:  // 70 tracks no errors
    //         case 351062:  // 70 w/ errors
    //             return D64_TYPE_D71;

    //         // D81
    //         case 819200:  // 80 tracks no errors
    //         case 822400:  // 80 w/ errors
    //             return D64_TYPE_D81;
    //     }

    //     return D64_TYPE_UNKNOWN;
    // }

private:

};


/********************************************************
 * File implementations
 ********************************************************/

class D64File: public MFile {

    std::shared_ptr<D64Image> _d64ImageStruct;

    // a function for lazily initializing the struct
    std::shared_ptr<D64Image> image() {
        if(_d64ImageStruct == nullptr) {
            Debug_printv("nullptr!");
            _d64ImageStruct = std::make_shared<D64Image>(streamFile->inputStream());
        }
        return _d64ImageStruct;
    }

public:

    D64File(std::string path, bool is_dir = true): MFile(path) {
        isDir = is_dir;
    };

    D64File(std::shared_ptr<D64Image> image, std::string path, bool is_dir = true): MFile(path) {
        _d64ImageStruct = image;
        isDir = is_dir;
    };


    void onInitialized () override {

        // THIS should work now. If it still doesn't just comment this, and uncomment two lines below :D
        // containerStream = streamFile->inputStream();

        //MFile* containerFile = new LittleFile(path);

        Debug_printv( "streamPath: [%s]", streamFile->url.c_str());
        Debug_printv( "pathInStream: [%s]", pathInStream.c_str());

        // Are we at the root of the pathInStream?
        if ( pathInStream == "")
        {
            Debug_printv("ROOT FOLDER");
            // Read Header
            image().get()->seekHeader();
            image().get()->fillHeader();
            Debug_printv("Disk Header [%.16s] [%.5s]", image().get()->header.disk_name, image().get()->header.id_dos);

            // Count Directory Entries
            // Calculate Blocks Free

            // Set Media Info Fields
            media_header = mstr::format("%.16s", image().get()->header.disk_name);
            mstr::A02Space(media_header);
            media_id = mstr::format("%.5s", image().get()->header.id_dos);
            mstr::A02Space(media_id);
            media_blocks_free = image().get()->blocksFree();
            media_block_size = image().get()->block_size;
            media_image = name;
            isDir = true;
        }
        else
        {
            Debug_printv("SINGLE FILE");
            // Single file
            isDir = false;
        }       
    };
    
    ~D64File() {
        // don't close the stream here! It will be used by shared ptr D64Util to keep reading image params
    }

    virtual std::string petsciiName() {
        // It's already in PETSCII
        mstr::replaceAll(name, "\\", "/");
        return name;
    }

    bool isDirectory() override;
//    MIStream* inputStream() override { Debug_printv("here"); return nullptr; }; // has to return OPENED stream
//    MOStream* outputStream() override { Debug_printv("here"); return nullptr; }; // has to return OPENED stream

    time_t getLastWrite() override;
    time_t getCreationTime() override;
    bool rewindDirectory() override;
    MFile* getNextFileInDir() override;
    bool mkDir() override { return false; };
    bool exists() override;
    size_t size() override;
    bool remove() override { return false; };
    bool rename(std::string dest) { return false; };
    MIStream* createIStream(std::shared_ptr<MIStream> src);
    //void addHeader(const String& name, const String& value, bool first = false, bool replace = true);

private:
    bool isDir = true;
    bool dirIsOpen = false;
};


/********************************************************
 * Streams
 ********************************************************/

class D64IStream: public MIStream {

    bool seekCalled = false;
    MIStream* containerIStream;

    D64Image* containerImage;

    // // a function for lazily initializing the struct
    // std::shared_ptr<D64Image> image() {
    //     if(_d64ImageStruct == nullptr) {
    //         Debug_printv("nullptr!");
    //         _d64ImageStruct = std::make_shared<D64Image>(containerIStream);
    //     }
    //     return _d64ImageStruct;
    // }

public:
    D64IStream(MIStream* is) {
        // TODO - store is somewhere, so you can read from it!
        containerIStream = is;
        containerImage = new D64Image(containerIStream);
    }
    // D64IStream(D64Image* image, MIStream* is) {
    //     containerIStream = is;
    //     containerImage = image;
    // };

    // MStream methods
    size_t position() override;
    void close() override;
    bool open() override;
    ~D64IStream() {
        Debug_printv("close");
        delete containerImage;
        close();
    }

    // MIStream methods
    bool isBrowsable() override { return false; };
    bool isRandomAccess() override { return true; };

    bool seek(int32_t pos, SeekMode mode) override { 
        Debug_printv("here");
        return true; 
    }; 
    bool seek(int32_t pos) override { 
        Debug_printv("here");
        return true; 
    };

    bool seekPath(std::string path) override;


    int available() override;
    size_t size() override;
    size_t read(uint8_t* buf, size_t size) override;
    bool isOpen();

protected:

    bool m_isOpen;
    int m_length;
    int m_bytesAvailable = 0;
    int m_position = 0;
};

/********************************************************
 * FS
 ********************************************************/

class D64FileSystem: public MFileSystem
{
public:
    MFile* getFile(std::string path) override {
        return new D64File(path);
    }

    bool handles(std::string fileName) {
        //Serial.printf("handles w dnp %s %d\n", fileName.rfind(".dnp"), fileName.length()-4);
        return byExtension(".d64", fileName);
    }

    D64FileSystem(): MFileSystem("d64") {};
};


#endif /* MEATFILESYSTEM_MEDIA_D64 */
