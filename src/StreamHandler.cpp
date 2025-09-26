#include <stdexcept>
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>

#include "StreamHandler.h"
#include "state.h"
#include "resp_create.h"


std::tuple<unsigned long,unsigned long>StreamHandler::parseEntryId(const std::string &streamName,const std::string &entryId){
    std::string unquotedEntryId = entryId;
    if (unquotedEntryId.front() == '"' && unquotedEntryId.back() == '"')
    {
        unquotedEntryId = unquotedEntryId.substr(1, unquotedEntryId.size() - 2);
    }
    auto separatorPos=unquotedEntryId.find('-');
    unsigned long firstId{},secondId{};
    if(unquotedEntryId.substr(0,separatorPos)=="*"){
        std::lock_guard<std::mutex>lock(m_stream_mutex);
        m_streams[streamName]->setFirstIdDefault();
    }
    else
    {
        firstId=std::stoul(unquotedEntryId.substr(0,separatorPos));
        if(unquotedEntryId.substr(separatorPos+1)=="*"){
            std::lock_guard<std::mutex>lock(m_stream_mutex);
            m_streams[streamName]->setSecondIdDefault();
        }
        else{
            secondId=std::stoul(unquotedEntryId.substr(separatorPos+1));
        }
    }
    return std::make_tuple(firstId,secondId);
}

void Stream::setFirstIdDefault(){
    firstIdDefault=true;
}
void Stream::setSecondIdDefault(){
    secondIdDefault=true;
}

std::string StreamHandler::xaddHandler(std::vector<std::string>&parsed_request){
    if(parsed_request.size()<4 || (parsed_request.size()-3)%2!=0){
        return create_simple_error("wrong number of arguments for 'xadd' command");
    }
    std::string streamName=parsed_request[1];
    {
        std::lock_guard<std::mutex>lock(m_stream_mutex);
            if(m_streams.find(streamName)==m_streams.end()){
                m_streams[streamName]=std::make_unique<Stream>(streamName);
            }

    }
    std::string &entryId=parsed_request[2];
    auto [firstId,secondId]=parseEntryId(streamName,entryId);
    
    std::unordered_map<std::string,std::string>fieldValues;
    for(int i=3;i<parsed_request.size();i++){
        std::string field=parsed_request[i++];
        std::string value=parsed_request[i];
        fieldValues[field]=value;
    }

    std::lock_guard<std::mutex>lock(m_stream_mutex);
    return m_streams[streamName]->AddEntry(firstId,secondId,fieldValues);
}

std::string Stream::AddEntry(unsigned long &entryFirstId,unsigned long &entrySecondId,std::unordered_map<std::string,std::string>&fieldValues){
    std::lock_guard<std::mutex>lock(m_streamStore_mutex);
    if(firstIdDefault){
        auto now=std::chrono::system_clock::now();
        entryFirstId = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        entrySecondId = 0;
    }
    else if(secondIdDefault){
        if(m_streamStore.find(entryFirstId)!=m_streamStore.end()){
            auto &secondIdMap=m_streamStore[entryFirstId];
            entrySecondId=secondIdMap.rbegin()->first+1;
        }
        else{
            entrySecondId=entryFirstId==0?1:0;
        }
    }
    if(entryFirstId==0 && entrySecondId==0){
        return create_simple_error("The ID specified in XADD must be greater than 0-0");
    }
    else if(m_latestFirstId>entryFirstId || (m_latestFirstId==entryFirstId && m_latestSecondId>=entrySecondId)){
        return create_simple_error("The ID specified in XADD is equal or smaller than the target stream top item");
    }
    m_latestFirstId=entryFirstId;
    m_latestSecondId=entrySecondId;
    m_streamStore[entryFirstId][entrySecondId]=std::move(fieldValues);
    std::string resString=std::to_string(entryFirstId)+"-"+std::to_string(entrySecondId);
    return create_bulk_string(resString);

}


