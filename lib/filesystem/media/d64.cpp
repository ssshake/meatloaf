#include "d64.h"

// D64 Utility Functions

bool D64Image::seekSector( uint8_t track, uint8_t sector, size_t offset )
{
	uint16_t sectorOffset = 0;

    Debug_printv("track[%d] sector[%d] offset[%d]", track, sector, offset);

    track--;
	for (uint8_t index = 0; index < track; ++index)
	{
		sectorOffset += sectorsPerTrack[speedZone(index)];
        //Debug_printv("track[%d] speedZone[%d] secotorsPerTrack[%d] sectorOffset[%d]", index, speedZone(index), sectorsPerTrack[speedZone(index)], sectorOffset);
	}
	sectorOffset += sector;

    this->track = track + 1;
    this->sector = sector;

    return containerStream->seek( (sectorOffset * block_size) + offset );
}

bool D64Image::seekSector( uint8_t* trackSector, size_t offset )
{
    return seekSector(trackSector[0], trackSector[1], offset);
}



std::string D64Image::readBlock(uint8_t track, uint8_t sector)
{
    return "";
}

bool D64Image::writeBlock(uint8_t track, uint8_t sector, std::string data)
{
    return true;
}

bool D64Image::allocateBlock( uint8_t track, uint8_t sector)
{
    return true;
}

bool D64Image::deallocateBlock( uint8_t track, uint8_t sector)
{
    return true;
}

bool D64Image::seekEntry( std::string filename )
{
    uint8_t index = 0;

    // Read Directory Entries
    while ( seekEntry( index ) )
    {
        Debug_printv("track[%d] sector[%d] filename[%s] entry.filename[%.16s]", track, sector, filename.c_str(), entry.filename);

        // Read Entry From Stream
        if (entry.file_type & 0b00000111 && filename == "*")
        {
            filename == entry.filename;
        }
        
        if ( filename == entry.filename )
        {
            // Move stream pointer to start track/sector
            return true;
        }
        index++;
    }
    
    entry.next_track = 0;
    entry.next_sector = 0;
    entry.blocks[0] = 0;
    entry.blocks[1] = 0;
    entry.filename[0] = '\0';

    return false;
}

bool D64Image::seekEntry( size_t index )
{
    bool r = false;
    static uint8_t next_track = 0;
    static uint8_t next_sector = 0;

    // Calculate Sector offset & Entry offset
    // 8 Entries Per Sector, 32 bytes Per Entry
    index--;
    int8_t sectorOffset = index / 8;
    int8_t entryOffset = (index % 8) * 32;

    Debug_printv("index[%d] sectorOffset[%d] entryOffset[%d] entryIndex[%d]", index, sectorOffset, entryOffset, entryIndex);


    if (index == 0 || index != entryIndex)
    {
        // Start at first sector of directory
        r = seekSector( directory_list_offset );
        
        do
        {
            if ( next_track )
                r = seekSector( entry.next_track, entry.next_sector );

            containerStream->read((uint8_t *)&entry, sizeof(entry));
            next_track = entry.next_track;
            next_sector = entry.next_sector;

            //Debug_printv("sectorOffset[%d] -> track[%d] sector[%d]", sectorOffset, this->track, this->sector);
        } while ( sectorOffset-- > 0 );
        r = seekSector( this->track, this->sector, entryOffset );
    }
    else
    {
        if ( entryOffset == 0 )
        {
            //Debug_printv("Follow link track[%d] sector[%d] entryOffset[%d]", next_track, next_sector, entryOffset);
            r = seekSector( next_track, next_sector, entryOffset );
        }

        containerStream->read((uint8_t *)&entry, sizeof(entry));        
    }

    if ( entryOffset == 0 )
    {
        next_track = entry.next_track;
        next_sector = entry.next_sector;
    }

    //Debug_printv("r[%d] file_type[%02X] file_name[%.16s]", r, entry.file_type, entry.filename);

    //if ( next_track == 0 && next_sector == 0xFF )
    entryIndex = index + 1;    
    if ( entry.file_type == 0x00 )
        return false;
    else
        return true;
}

std::string D64Image::decodeEntry() 
{
    bool hide = false;
    uint8_t file_type = entry.file_type & 0b00000111;
    std::string type = file_type_label[ file_type ];
    if ( file_type == 0 )
        hide = true;

    switch ( entry.file_type & 0b11000000 )
    {
        case 0xC0:			// Bit 6: Locked flag (Set produces "<" locked files)
            type += "<";
            hide = false;
            break;
            
        case 0x00:
            type += "*";	// Bit 7: Closed flag  (Not  set  produces  "*", or "splat" files)
            hide = true;
            break;					
    }

    return type;
}

uint16_t D64Image::blocksFree()
{
    uint16_t free_count = 0;
    BAMEntry bam = { 0 };
    seekSector(block_allocation_map->track, block_allocation_map->sector, block_allocation_map->offset);
    for(uint8_t i = block_allocation_map->start_track; i <= block_allocation_map->end_track; i++)
    {
        containerStream->read((uint8_t *)&bam, sizeof(bam));
        if ( i != block_allocation_map->track )
        {
            //Debug_printv("track[%d] count[%d]", i, bam.free_sectors);
            free_count += bam.free_sectors;            
        }
    }

    return free_count;
}


void D64Image::sendFile( std::string filename )
{
    if ( seekEntry( filename ) )
    {
        seekSector( entry.start_track, entry.start_sector );

        bool last_block = false;
        do
        {
            uint8_t next_track = 0;  // Read first byte of sector
            uint8_t next_sector = 0; // Read second byte of sector
            
            if (next_track == 0) // Is this the last block?
            {
                //echo fread($this->fp, $next_sector); // Read number of bytes specified by next_sector since this is the last block
                last_block = true;
            }
            else
            {
                // echo fread($this->fp, 254);  // Read next 254 bytes
                seekSector( next_track, next_sector);
            }
        } while ( !last_block );        
    }
}


/********************************************************
 * File impls
 ********************************************************/

bool D64File::isDirectory() {
    return isDir;
};

bool D64File::rewindDirectory() {
    dirIsOpen = true;
    image().get()->resetEntryCounter();
    return getNextFileInDir();
}

MFile* D64File::getNextFileInDir() {

    if(!dirIsOpen)
        rewindDirectory();

    // Get entry pointed to by containerStream
    if ( image().get()->seekNextEntry() )
    {
        std::string fileName = image().get()->entry.filename;
        mstr::replaceAll(fileName, "/", "\\");
        //Debug_printv( "entry[%s]", (streamFile->url + "/" + fileName).c_str() );
        auto d64_file = new D64File(_d64ImageStruct, streamFile->url + "/" + fileName, false);
        d64_file->extension = d64_file->_d64ImageStruct->decodeEntry();
        return d64_file;
    }
    else
    {
        //Debug_printv( "END OF DIRECTORY");
        dirIsOpen = false;
        return nullptr;
    }
}

MIStream* D64File::createIStream(MIStream* is) {
    // has to return OPENED stream
    Debug_printv("[%s]", url.c_str());
    MIStream* istream = new D64IStream(is);
    istream->open();
    return istream;
}

time_t D64File::getLastWrite() {
    return getCreationTime();
}

time_t D64File::getCreationTime() {
    tm *entry_time = 0;
    auto entry = image().get()->entry;
    entry_time->tm_year = entry.year + 1900;
    entry_time->tm_mon = entry.month;
    entry_time->tm_mday = entry.day;
    entry_time->tm_hour = entry.hour;
    entry_time->tm_min = entry.minute;

    return mktime(entry_time);
}

bool D64File::exists() {
    // here I'd rather use D64 logic to see if such file name exists in the image!
    Debug_printv("here");
    return true;
} 

size_t D64File::size() {
    Debug_printv("here");
    // use D64 to get size of the file in image
    auto entry = image().get()->entry;
    // (_ui16 << 8 | _ui16 >> 8)
    uint16_t blocks = (entry.blocks[0] << 8 | entry.blocks[1] >> 8);
    return blocks;
}

/********************************************************
 * Istream impls
 ********************************************************/


bool D64IStream::seekPath(std::string path) {
    // Implement this to skip a queue of file streams to start of file by name
    // this will cause the next read to return bytes of 'path'
    Debug_printv("here");
    seekCalled = true;

    // call D54Image method to obtain file bytes here, return true on success:
    // return D64Image.seekFile(containerIStream, path);
    mstr::toPETSCII(path);
    if ( containerImage->seekEntry(path) )
    {
        auto entry = containerImage->entry;
        auto type = containerImage->decodeEntry().c_str();
        auto blocks = (entry.blocks[0] << 8 | entry.blocks[1] >> 8);
        Debug_printv("filename [%.16s] type[%s] size[%d] start_track[%d] start_sector[%d]", entry.filename, type, blocks, entry.start_track, entry.start_sector);
        containerImage->seekSector(entry.start_track, entry.start_sector);

        m_length = blocks;
        m_bytesAvailable = blocks;
    }
    else
    {
        Debug_printv( "Not found! [%s]", path.c_str());
    }

    return false;
};

// std::string D64IStream::seekNextEntry() {
//     // Implement this to skip a queue of file streams to start of next file and return its name
//     // this will cause the next read to return bytes of "next" file in D64 image
//     // might not have sense in this case, as D64 is kinda random access, not a stream.
//     return "";
// };

// bool D64IStream::seek(uint32_t pos) {
//     // seek only within current "active" ("seeked") file within the image (see above)
//     if(pos==m_position)
//         return true;

//     return false;
// };

size_t D64IStream::position() {
    return m_position; // return position within "seeked" file, not the D64 image!
};

void D64IStream::close() {

};

bool D64IStream::open() {
    // return true if we were able to read the image and confirmed it is valid.
    // it's up to you in what state the stream will be after open. Could be either:
    // 1. EOF-like state (0 available) and the state will be cleared only after succesful seekNextEntry or seekPath
    // 2. non-EOF-like state, and ready to send bytes of first file, because you did immediate seekNextEntry here
    Debug_printv("here");
    return false;
};

int D64IStream::available() {
    // return bytes available in currently "seeked" file
    return m_bytesAvailable;
};

size_t D64IStream::size() {
    // size of the "seeked" file, not the image.
    return m_length;
};

size_t D64IStream::read(uint8_t* buf, size_t size) {
    size_t bytesRead = 0;
    
    static uint8_t next_track = 0;
    static uint8_t next_sector = 0;
    static uint8_t sector_offset = 0;

    if(seekCalled) {
        // if we have the stream set to a specific file already, either via seekNextEntry or seekPath, return bytes of the file here
        // or set the stream to EOF-like state, if whle file is completely read.
        //auto bytesRead= 0; // m_file.readBytes((char *) buf, size);
        //m_bytesAvailable = 0; //m_file.available();
        //bytesRead = image().get()->readFile(buf, size);

        //Debug_printv("track[%d] sector[%d] sector_offset[%d]", image().get()->track, image().get()->sector, sector_offset);

        if ( sector_offset % containerImage->block_size == 0 )
        {
            // We are at the beginning of the block
            // Read track/sector link
            // containerImage->containerStream->read(&next_track, 1);
            // containerImage->containerStream->read(&next_sector, 1);
            // sector_offset += 2;
            //Debug_printv("next_track[%d] next_sector[%d] sector_offset[%d]", next_track, next_sector, sector_offset);
        }

        bytesRead = containerImage->containerStream->read((uint8_t *)&buf, size);
        //buf[0] = 8;
        //bytesRead = 1;

        sector_offset += bytesRead;

        //bytesRead = D64Image.readFileBytes(containerIStream, path, fromWhere, buffer[], bufferSize);
    }
    else {
        // seekXXX not called - just pipe image bytes, so it can be i.e. copied verbatim
        //Debug_printv("containerIStream.isOpen[%d]", containerIStream->isOpen());
        if ( containerIStream == nullptr)
            Debug_printv("containerIStream is null");

        bytesRead = containerIStream->read(buf, size);
    }

    m_position+=bytesRead;
    m_bytesAvailable = m_length - m_position;
    return bytesRead;
};

bool D64IStream::isOpen() {
    Debug_printv("here");
    return m_isOpen;
};

