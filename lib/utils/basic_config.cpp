#include "basic_config.h"

std::string BasicConfigReader::get(std::string key) {
    if(entries->find(key) == entries->end())
        return nullptr;
    else
        return entries->at(key);
}

void BasicConfigReader::read(std::string name) {
    Meat::ifstream istream(name);
    istream.open();
    if(istream.is_open()) {
        // skip two bytes:
        // LH - load address

        while(!istream.eof()) {
            uint16_t word;
            std::string line;
            // parse line here:
            // LH - next line ptr, if both == 0 - stop here, no more lines
            istream >> word;
            if(word != 0) {
                // LH - line number
                istream >> word;
                // config_name:config value(s)
                // 0
                istream >> line;
                mstr::toASCII(line);
                auto split = mstr::split(line, ':', 2);
                if(split.size()>1) {
                    entries->insert(std::make_pair(split[0], split[1]));
                    //Serial.printf("konfig: %s=%s\n", split[0].c_str(), split[1].c_str());
                }
            }
        }

        istream.close();
    }
}