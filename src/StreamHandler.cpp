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
#include "handlers.h"

using namespace std;


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

std::string StreamHandler::xaddHandler(std::deque<std::string>&parsed_request){
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

    std::unique_lock<std::mutex>lock(m_stream_mutex);
    string result_id=m_streams[streamName]->AddEntry(firstId,secondId,fieldValues);
    lock.unlock();
    {
        unique_lock<mutex>lock1(blocked_streams_mutex);
        cout<<streamName<<" size : "<<blocked_streams[streamName].size()<<endl;
        for(auto &[required_id,client_fd]:blocked_streams[streamName]){
            cout<<"one : "<<streamName<<" "<<entryId<<" "<<required_id<<" "<<(required_id<entryId)<<endl;
            if(required_id<entryId)clients_cvs[client_fd].notify_one();
            else break;
        }
    }
    return result_id;
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

std::string StreamHandler::xrangeHandler(std::deque<std::string>&parsed_request){
    std::cout<<"xrange handler function called \n";
    std::string streamName=parsed_request[1];
    std::deque<std::string>dq;
    {
        std::lock_guard<std::mutex>lock(m_stream_mutex);
        if(!m_streams.count(streamName)){
            return create_resp_array(dq);
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
    return create_resp_array(finalDq,0,INT_MAX,true);
    
    
}

std::map<std::string,std::map<std::string,std::string>>Stream::GetEntriesInRange(std::string startId,std::string endId){
    std::cout<<"get entries in range \n";
    std::lock_guard<std::mutex>lock(Stream::m_streamStore_mutex);
    auto [startFirstId,startSecondId,endFirstId,endSecondId]=parseRangeQuery(startId,endId);

    std::map<std::string,std::map<std::string,std::string>>res;

    auto it=startFirstId==0?m_streamStore.begin():m_streamStore.lower_bound(startFirstId);

    auto endIt=endFirstId==0?m_streamStore.end():m_streamStore.upper_bound(endFirstId);
    if(endIt!=m_streamStore.begin())--endIt;

    std::map<unsigned long,std::map<std::string, std::string>>::iterator endSeqIt;

    bool hasEndSeqIt = false;
    if (endIt != m_streamStore.end()) {
        auto &innerMap = endIt->second;
        endSeqIt = (endSecondId == 0) ? innerMap.end()
                                      : innerMap.upper_bound(endSecondId);
        if (endSeqIt != innerMap.begin()) {
            --endSeqIt;
        }
        hasEndSeqIt = true;
    }


    while(it!=m_streamStore.end()){
        auto seqIt=startSecondId==0?m_streamStore[it->first].begin():m_streamStore[it->first].lower_bound(startSecondId);
        if(seqIt==m_streamStore[it->first].end()){
            ++it;
            startSecondId=0;
            continue;
        }
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

deque<string> StreamHandler::xreadHandler(deque<string>&parsed_request,bool ignoreEmptyArray){
        auto[streamKeys,streamIds]=get_stream_keys_ids(parsed_request);
        deque<string>resp_keys;
        for(int i=0;i<streamKeys.size();i++){
          string streamName=streamKeys[i];
          string id=streamIds[i];
          if(m_streams.count(streamName)){
            unique_lock<mutex>lock(m_stream_mutex);
            auto [startFirstId,startSecondId,endFirstId,endSecondId]=m_streams[streamName]->parseRangeQuery(id,"+");
            lock.unlock();
            deque<string>dq={"streams",streamName,to_string(startFirstId)+"-"+to_string(startSecondId+1),"+"};
            resp_keys.push_back("*2\r\n"+create_bulk_string(streamName)+xrangeHandler(dq));
          }else if(!ignoreEmptyArray){
            resp_keys.push_back("*2\r\n"+create_bulk_string(streamName)+"*0\r\n");
          }
        }
        return resp_keys;
}

deque<string> StreamHandler::xreadBlocked$Handler(std::deque<std::string>&parsed_request){
    return {};
}
deque<string> StreamHandler::xreadBlockedHandler(int client_fd,std::deque<std::string>&parsed_request){
    unique_lock<mutex>lock(blocked_streams_mutex);
    int waiting_time=stoi(parsed_request[2]);
    bool has_data=false;
    deque<string>result;

    auto [streamKeys,streamIds]=get_stream_keys_ids(parsed_request);
    for(int i=0;i<streamKeys.size();i++){
        cout<<"adding in blocked streams key :"<<streamKeys[i]<<" id : "<<streamIds[i]<<endl;
        blocked_streams[streamKeys[i]].insert(make_tuple(streamIds[i],client_fd));
    }

    auto &cv=clients_cvs[client_fd];
    if(waiting_time==0){
        cv.wait(lock,[&](){return has_data=!(result=xreadHandler(parsed_request,true)).empty(); });
    }else{
        
        has_data = cv.wait_for(lock, chrono::milliseconds(waiting_time), [&](){ return !(result=xreadHandler(parsed_request,true)).empty(); });
        cout<<"wait over"<<endl;
    }
    for(int i=0;i<streamKeys.size();i++){
        blocked_streams[streamKeys[i]].erase(make_tuple(streamIds[i],client_fd));
    }
    cout<<"result size : "<<result.size()<<endl;
    return result;
    

}

