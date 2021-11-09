#include "d64.h"

// D64 Utility Functions

bool CBMImageStream::seekSector( uint8_t track, uint8_t sector, size_t offset )
{
	uint16_t sectorOffset = 0;

    //Debug_printv("track[%d] sector[%d] offset[%d]", track, sector, offset);

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

bool CBMImageStream::seekSector( std::vector<uint8_t> trackSectorOffset )
{
    return seekSector(trackSectorOffset[0], trackSectorOffset[1], trackSectorOffset[2]);
}


std::string CBMImageStream::readBlock(uint8_t track, uint8_t sector)
{
    return "";
}

bool CBMImageStream::writeBlock(uint8_t track, uint8_t sector, std::string data)
{
    return true;
}

bool CBMImageStream::allocateBlock( uint8_t track, uint8_t sector)
{
    return true;
}

bool CBMImageStream::deallocateBlock( uint8_t track, uint8_t sector)
{
    return true;
}

bool CBMImageStream::seekEntry( std::string filename )
{
    uint8_t index = 1;
    mstr::rtrimA0(filename);

    // Read Directory Entries
    if ( filename.size() )
    {
        while ( seekEntry( index ) )
        {
            std::string entryFilename = entry.filename;
            mstr::rtrimA0(entryFilename);        
            Debug_printv("track[%d] sector[%d] filename[%s] entry.filename[%.16s]", track, sector, filename.c_str(), entryFilename.c_str());

            // Read Entry From Stream
            if (entry.file_type & 0b00000111 && filename == "*")
            {
                filename == entryFilename;
            }
            
            if ( filename == entryFilename.substr(0, filename.size()) )
            {
                // Move stream pointer to start track/sector
                return true;
            }
            index++;
        }
    }

    entry.next_track = 0;
    entry.next_sector = 0;
    entry.blocks[0] = 0;
    entry.blocks[1] = 0;
    entry.filename[0] = '\0';

    return false;
}

bool CBMImageStream::seekEntry( size_t index )
{
    bool r = false;

    // Calculate Sector offset & Entry offset
    // 8 Entries Per Sector, 32 bytes Per Entry
    index--;
    uint8_t sectorOffset = index / 8;
    uint8_t entryOffset = (index % 8) * 32;

    //Debug_printv("----------");
    //Debug_printv("index[%d] sectorOffset[%d] entryOffset[%d] entry_index[%d]", index, sectorOffset, entryOffset, entry_index);


    if (index == 0 || index != entry_index)
    {
        // Start at first sector of directory
        next_track = 0;
        r = seekSector( directory_list_offset );
        
        // Find sector with requested entry
        do
        {
            if ( next_track )
            {
                //Debug_printv("next_track[%d] next_sector[%d]", entry.next_track, entry.next_sector);
                r = seekSector( entry.next_track, entry.next_sector, 0x00 );
            }

            containerStream->read((uint8_t *)&entry, sizeof(entry));
            next_track = entry.next_track;
            next_sector = entry.next_sector;

            //Debug_printv("sectorOffset[%d] -> track[%d] sector[%d]", sectorOffset, track, sector);
        } while ( sectorOffset-- > 0 );
        r = seekSector( track, sector, entryOffset );
    }
    else
    {
        if ( entryOffset == 0 )
        {
            if ( next_track == 0 )
                return false;

            //Debug_printv("Follow link track[%d] sector[%d] entryOffset[%d]", next_track, next_sector, entryOffset);
            r = seekSector( next_track, next_sector, entryOffset );
        }        
    }

    containerStream->read((uint8_t *)&entry, sizeof(entry));      

    // If we are at the first entry in the sector then get next_track/next_sector
    if ( entryOffset == 0 )
    {
        next_track = entry.next_track;
        next_sector = entry.next_sector;        
    }

    //Debug_printv("r[%d] file_type[%02X] file_name[%.16s]", r, entry.file_type, entry.filename);

    //if ( next_track == 0 && next_sector == 0xFF )
    entry_index = index + 1;    
    if ( entry.file_type == 0x00 )
        return false;
    else
        return true;
}

std::string CBMImageStream::decodeEntry() 
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

uint16_t CBMImageStream::blocksFree()
{
    uint16_t free_count = 0;

    for(uint8_t x = 0; x < block_allocation_map.size(); x++)
    {
        uint8_t bam[block_allocation_map[0].byte_count] = { 0 };
        Debug_printv("start_track[%d] end_track[%d]", block_allocation_map[x].start_track, block_allocation_map[x].end_track);

        seekSector(block_allocation_map[x].track, block_allocation_map[x].sector, block_allocation_map[x].offset);
        for(uint8_t i = block_allocation_map[x].start_track; i <= block_allocation_map[x].end_track; i++)
        {
            containerStream->read((uint8_t *)&bam, sizeof(bam));
            if ( sizeof(bam) > 3 )
            {               
                if ( i != block_allocation_map[x].track )
                {
                    Debug_printv("x[%d] track[%d] count[%d] size[%d]", x, i, bam[0], sizeof(bam));
                    free_count += bam[0];            
                }
            }
            else
            {
                // D71 tracks 36 - 70 you have to count the 0 bits (0 is allocated)
                uint8_t bit_count = 0;
                bit_count += 8 - std::bitset<8>(bam[0]).count();
                bit_count += 8 - std::bitset<8>(bam[1]).count();
                bit_count += 8 - std::bitset<8>(bam[2]).count();

                Debug_printv("x[%d] track[%d] count[%d] size[%d] (counting 0 bits)", x, i, bit_count, sizeof(bam));
                free_count += bit_count;
            }                    
        }
    }



    return free_count;
}

size_t CBMImageStream::readFile(uint8_t* buf, size_t size) {
    size_t bytesRead = 0;

    if ( sector_offset % block_size == 0 )
    {
        // We are at the beginning of the block
        // Read track/sector link
        containerStream->read((uint8_t *)&next_track, 1);
        containerStream->read((uint8_t *)&next_sector, 1);
        sector_offset += 2;
        //Debug_printv("next_track[%d] next_sector[%d] sector_offset[%d]", next_track, next_sector, sector_offset);
    }

    bytesRead += containerStream->read(buf, size);
    sector_offset += bytesRead;
    m_bytesAvailable -= bytesRead;

    if ( sector_offset % block_size == 0 )
    {
        // We are at the end of the block
        // Follow track/sector link to move to next block
        seekSector( next_track, next_sector );
        //Debug_printv("track[%d] sector[%d] sector_offset[%d]", track, sector, sector_offset);
    }

    // if ( !bytesRead )
    // {
    //     sector_offset = 0;
    // }

    return bytesRead;
}


/********************************************************
 * File impls
 ********************************************************/

bool D64File::isDirectory() {
    //Debug_printv("pathInStream[%s]", pathInStream.c_str());
    if ( pathInStream == "" )
        return true;
    else
        return false;
};

bool D64File::rewindDirectory() {
    dirIsOpen = true;
    Debug_printv("streamFile->url[%s]", streamFile->url.c_str());
    auto image = ImageBroker::obtain(streamFile->url);
    if ( image == nullptr )
        Debug_printv("image pointer is null");

    image->resetEntryCounter();

    // Read Header
    image->seekHeader();

    // Set Media Info Fields
    media_header = mstr::format("%.16s", image->header.disk_name);
    mstr::A02Space(media_header);
    media_id = mstr::format("%.5s", image->header.id_dos);
    mstr::A02Space(media_id);
    media_blocks_free = image->blocksFree();
    media_block_size = image->block_size;
    media_image = name;

    return true;
}

MFile* D64File::getNextFileInDir() {

    if(!dirIsOpen)
        rewindDirectory();

    // Get entry pointed to by containerStream
    auto image = ImageBroker::obtain(streamFile->url);

    if ( image->seekNextImageEntry() )
    {
        std::string fileName = image->entry.filename;
        mstr::replaceAll(fileName, "/", "\\");
        Debug_printv( "entry[%s]", (streamFile->url + "/" + fileName).c_str() );
        auto d64_file = MFSOwner::File(streamFile->url + "/" + fileName);
        d64_file->extension = image->decodeEntry();
        return d64_file;
    }
    else
    {
        //Debug_printv( "END OF DIRECTORY");
        dirIsOpen = false;
        return nullptr;
    }
}

MIStream* D64File::createIStream(std::shared_ptr<MIStream> containerIstream) {
    Debug_printv("[%s]", url.c_str());

    return new CBMImageStream(containerIstream);
}

time_t D64File::getLastWrite() {
    return getCreationTime();
}

time_t D64File::getCreationTime() {
    tm *entry_time = 0;
    auto entry = ImageBroker::obtain(streamFile->url)->entry;
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
    // Debug_printv("[%s]", streamFile->url.c_str());
    // use D64 to get size of the file in image
    auto entry = ImageBroker::obtain(streamFile->url)->entry;
    // (_ui16 << 8 | _ui16 >> 8)
    //uint16_t blocks = (entry.blocks[0] << 8 | entry.blocks[1] >> 8);
    uint16_t blocks = entry.blocks[0] * 256 + entry.blocks[1];
    return blocks;
}

/********************************************************
 * Istream impls
 ********************************************************/


bool CBMImageStream::seekPath(std::string path) {
    // Implement this to skip a queue of file streams to start of file by name
    // this will cause the next read to return bytes of 'path'
    seekCalled = true;

    next_track = 0;
    next_sector = 0;
    sector_offset = 0;

    entry_index = 0;

    // call D54Image method to obtain file bytes here, return true on success:
    // return D64Image.seekFile(containerIStream, path);
    mstr::toPETSCII(path);
    if ( seekEntry(path) )
    {
        //auto entry = containerImage->entry;
        auto type = decodeEntry().c_str();
        //auto blocks = (entry.blocks[0] << 8 | entry.blocks[1] >> 8);
        //auto blocks = (entry.blocks[0] * 256) + entry.blocks[1];
        Debug_printv("filename [%.16s] type[%s] start_track[%d] start_sector[%d]", entry.filename, type, entry.start_track, entry.start_sector);
        seekSector(entry.start_track, entry.start_sector);

        // Calculate file size
        uint8_t t = 0;
        uint8_t s = 0;
        size_t blocks = 0; 
        do
        {
            //Debug_printv("t[%d] s[%d]", t, s);

            containerStream->read(&t, 1);
            containerStream->read(&s, 1);
            blocks++;
            seekSector( t, s );
        } while ( t > 0 );
        blocks--;
        m_length = (blocks * 254) + s - 2;
        m_bytesAvailable = m_length;
        
        // Set position to beginning of file
        seekSector( entry.start_track, entry.start_sector );

        Debug_printv("File Size: blocks[%d] size[%d] available[%d]", (blocks + 1), m_length, m_bytesAvailable);
        
        return true;
    }
    else
    {
        Debug_printv( "Not found! [%s]", path.c_str());
    }

    return false;
};

// std::string CBMImageStream::seekNextEntry() {
//     // Implement this to skip a queue of file streams to start of next file and return its name
//     // this will cause the next read to return bytes of "next" file in D64 image
//     // might not have sense in this case, as D64 is kinda random access, not a stream.
//     return "";
// };


bool CBMImageStream::open() {
    // return true if we were able to read the image and confirmed it is valid.
    // it's up to you in what state the stream will be after open. Could be either:
    // 1. EOF-like state (0 available) and the state will be cleared only after succesful seekNextEntry or seekPath
    // 2. non-EOF-like state, and ready to send bytes of first file, because you did immediate seekNextEntry here
    Debug_printv("here");
    return false;
};

void CBMImageStream::close() {

};

// bool CBMImageStream::seek(uint32_t pos) {
//     // seek only within current "active" ("seeked") file within the image (see above)
//     if(pos==m_position)
//         return true;

//     return false;
// };

size_t CBMImageStream::position() {
    return m_position; // return position within "seeked" file, not the D64 image!
};


int CBMImageStream::available() {
    // return bytes available in currently "seeked" file
    return m_bytesAvailable;
};

size_t CBMImageStream::size() {
    // size of the "seeked" file, not the image.
    return m_length;
};

size_t CBMImageStream::read(uint8_t* buf, size_t size) {
    size_t bytesRead = 0;

    if(seekCalled) {
        // if we have the stream set to a specific file already, either via seekNextEntry or seekPath, return bytes of the file here
        // or set the stream to EOF-like state, if whle file is completely read.
        bytesRead = readFile(buf, size);

    }
    else {
        // seekXXX not called - just pipe image bytes, so it can be i.e. copied verbatim
        bytesRead = containerStream->read(buf, size);
    }

    m_position += bytesRead;
    return bytesRead;
};

bool CBMImageStream::isOpen() {
    Debug_printv("here");
    return m_isOpen;
};


std::unordered_map<std::string, CBMImageStream*> ImageBroker::repo;