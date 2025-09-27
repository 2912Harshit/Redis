#pragma once

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <unordered_map>
#include <deque>

class Stream{
    private:
        std::string m_streamName;
        unsigned long m_latestFirstId{};
        unsigned long m_latestSecondId{};
        bool firstIdDefault{false};
        bool secondIdDefault{false};
        std::map<unsigned long,
                    std::map<unsigned long,
                            std::map<std::string,std::string>>>m_streamStore;
        std::recursive_mutex m_streamStore_mutex;

    
    public:
        Stream(const std::string &streamName):m_streamName(streamName){}
        
        std::string AddEntry(unsigned long &entryFirstId,unsigned long &entrySecondId,std::map<std::string,std::string>&fieldValues);
        void setFirstIdDefault();
        void setSecondIdDefault();
        std::map<std::string,std::map<std::string,std::string>>GetEntriesInRange(std::string startId,std::string endId);
        std::tuple<unsigned long,unsigned long,unsigned long,unsigned long>parseRangeQuery(std::string startId,std::string endId);

};

class StreamHandler{
    public:
        std::unordered_map<std::string,std::unique_ptr<Stream>>m_streams;
        std::tuple<unsigned long,unsigned long>parseEntryId(const std::string &streamName,const std::string &entryId);
        std::string xaddHandler(std::deque<std::string>&parsed_request);
        std::string xrangeHandler(int client_fd,std::deque<std::string>&parsed_request);

};