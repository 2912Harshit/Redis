#pragma once

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <unordered_map>
#include <deque>
#include <mutex>

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
        std::mutex m_streamStore_mutex;

    
    public:
        Stream(const std::string &streamName):m_streamName(streamName){}
        
        std::string AddEntry(unsigned long &entryFirstId,unsigned long &entrySecondId,std::map<std::string,std::string>&fieldValues);
        void setFirstIdDefault();
        void setSecondIdDefault();
        std::map<std::string,std::map<std::string,std::string>>GetEntriesInRange(std::string startId,std::string endId);
        std::tuple<unsigned long,unsigned long,unsigned long,unsigned long>parseRangeQuery(std::string startId,std::string endId);
        std::pair<unsigned long,unsigned long>GetLatestId();

};

class StreamHandler{
    public:
        std::unordered_map<std::string,std::unique_ptr<Stream>>m_streams;

        std::tuple<unsigned long,unsigned long>parseEntryId(const std::string &streamName,const std::string &entryId);

        std::string xaddHandler(std::deque<std::string>&parsed_request);

        std::string xrangeHandler(std::deque<std::string>&parsed_request);

        std::deque<std::string> xreadHandler(std::deque<std::string>&parsed_request,bool ignoreEmptyArray);

        std::deque<std::string> xreadBlocked$Handler(std::deque<std::string>&parsed_request);

        std::deque<std::string> xreadBlockedHandler(int client_fd,std::deque<std::string>&parsed_request);

        static std::shared_ptr<StreamHandler> getInstance() {
            static std::shared_ptr<StreamHandler> instance(new StreamHandler());
            return instance;
        }
        StreamHandler(){}
};