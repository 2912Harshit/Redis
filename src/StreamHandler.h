#pragma once

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <unordered_map>

class Stream{
    private:
        std::string m_streamName;
        unsigned long m_latestFirstId{};
        unsigned long m_latestSecondId{};
        bool firstIdDefault{false};
        bool secondIdDefault{false};
        std::map<unsigned long,
                    std::map<unsigned long,
                            std::unordered_map<std::string,std::string>>>m_streamStore;
        std::mutex m_streamStore_mutex;

    
    public:
        Stream(const std::string &streamName):m_streamName(streamName){}
        
        std::string AddEntry(unsigned long &entryFirstId,unsigned long &entrySecondId,std::unordered_map<std::string,std::string>&fieldValues);
        void setFirstIdDefault();
        void setSecondIdDefault();


};

class StreamHandler{
    public:
        std::unordered_map<std::string,std::unique_ptr<Stream>>m_streams;
        std::tuple<unsigned long,unsigned long>parseEntryId(const std::string &streamName,const std::string &entryId);
        std::string xaddHandler(std::vector<std::string>&parsed_request);

};