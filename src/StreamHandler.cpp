#include <stdexcept>
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <deque>
#include <map>
#include <unordered_map>

#include "StreamHandler.h"
#include "state.h"
#include "resp_create.h"
#include "resp_send.h"


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
    
    std::map<std::string,std::string>fieldValues;
    for(int i=3;i<parsed_request.size();i++){
        std::string field=parsed_request[i++];
        std::string value=parsed_request[i];
        fieldValues[field]=value;
    }

    std::lock_guard<std::mutex>lock(m_stream_mutex);
    return m_streams[streamName]->AddEntry(firstId,secondId,fieldValues);
}

std::string Stream::AddEntry(unsigned long &entryFirstId,unsigned long &entrySecondId,std::map<std::string,std::string>&fieldValues){
    std::lock_guard<std::mutex>lock(Stream::m_streamStore_mutex);
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

void StreamHandler::xrangeHandler(int client_fd,std::vector<std::string>&parsed_request){
    std::cout<<"xrange handler function called \n";
    std::string streamName=parsed_request[1];
    std::deque<std::string>dq;
    {
        std::lock_guard<std::mutex>lock(m_stream_mutex);
        if(!m_streams.count(streamName)){
            send_array(client_fd,dq);
            return;
        }
    }

    auto startId=parsed_request[2];
    auto endId=parsed_request[3];
    std::map<std::string,std::map<std::string,std::string>>entries;
    {
        std::lock_guard<std::mutex>lock(m_stream_mutex);
        entries= m_streams[streamName]->GetEntriesInRange(startId,endId);
    }
    
    std::deque<std::string>finalDq;
    for(auto &entry:entries){
        std::deque<std::string>outerDq;
        std::string id=entry.first;
        outerDq.push_back(create_bulk_string(id));
        std::deque<std::string>innerDq;
        for(auto &keys:entry.second){
            std::string key=keys.first;
            std::string value=keys.second;
            innerDq.push_back(key);
            innerDq.push_back(value);
        }
        outerDq.push_back(create_resp_array(innerDq));
        finalDq.push_back(create_resp_array(outerDq,0,INT_MAX,true));
    }
    send_array(client_fd,finalDq,0,INT_MAX,true);
}

std::map<std::string,std::map<std::string,std::string>>Stream::GetEntriesInRange(std::string startId,std::string endId){
    std::cout<<"get entries in range \n";
    std::lock_guard<std::mutex>lock(Stream::m_streamStore_mutex);
    auto [startFirstId,startSecondId,endFirstId,endSecondId]=parseRangeQuery(startId,endId);
    std::map<std::string,std::map<std::string,std::string>>res;
    std::cout<<startFirstId<<" "<<startSecondId<<" "<<endFirstId<<" "<<endSecondId<<"\n";
    auto it=startFirstId==0?m_streamStore.begin():m_streamStore.lower_bound(startFirstId);
    auto endIt=endFirstId==0?m_streamStore.end():m_streamStore.upper_bound(endFirstId);
    if(endIt!=m_streamStore.begin())--endIt;
    std::cout<<endIt->first<<"\n";
    std::map<unsigned long,std::map<std::string, std::string>>::iterator endSeqIt;
    bool hasEndSeqIt = false;
    if (endFirstId != 0 && endIt != m_streamStore.end()) {
        auto &innerMap = endIt->second;
        endSeqIt = (endSecondId == 0) ? innerMap.end()
                                      : innerMap.upper_bound(endSecondId);
        if (endSeqIt != innerMap.begin()) {
            --endSeqIt;
        }
        hasEndSeqIt = true;
    }
    std::cout<<endSeqIt->first<<std::endl;
    while(it!=m_streamStore.end()){
        auto seqIt=startSecondId==0?m_streamStore[it->first].begin():m_streamStore[it->first].lower_bound(startSecondId);
        while(seqIt!=m_streamStore[it->first].end()){
            std::string id=std::to_string(it->first)+"-"+std::to_string(seqIt->first);
            res[id]=seqIt->second;
            // std::cout<<id<<" "<<(seqIt->second).begin()->first;
            startSecondId=0;
            if(hasEndSeqIt && seqIt==endSeqIt && it==endIt)break;
            ++seqIt;
        }
        if(endIt==it)break;
        ++it;
    }
    
    return res;

}

std::tuple<unsigned long,unsigned long,unsigned long,unsigned long>Stream::parseRangeQuery(std::string startId,std::string endId){
    unsigned long startMilliId{},startSeqId{},endMilliId{},endSeqId{};
    if(startId=="-"){
        startMilliId=0;
        startSeqId=0;
    }
    else{
        auto startSep=startId.find('-');
        startMilliId=std::stoul(startId.substr(0,startSep));
        if(startSep!=std::string::npos){
            startSeqId=std::stoul(startId.substr(startSep+1));
        }
    }
    if(endId=="+"){
        endMilliId=0;
        endSeqId=0;
    }
    else
    {
        auto endSep=endId.find('-');
        endMilliId=std::stoul(endId.substr(0,endSep));
        if(endSep!=std::string::npos){
            endSeqId=std::stoul(endId.substr(endSep+1));
        }
    }
    return std::make_tuple(startMilliId,startSeqId,endMilliId,endSeqId);
}






