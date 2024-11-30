#include"REPL.h"
bool REPL::GET(string &key,string& value){
    read_lock(0);
    if(1||filters[0][0].contains(key)){
        auto pointer=memtable.find(key);
        if(pointer!=memtable.end()){
            if (pointer->second!="\r\n") {
                read_unlock(0);
                value=pointer->second;
                return 1;
            }
            else{
                read_unlock(0);
                return 0;
            }
        }
    }
    read_unlock(0);
    for(int i=1;i<levels_main.size();++i){
        read_lock(i);
        for(int j=levels_main[i];j>0;--j){
            if(1||filters[i][j-1].contains(key)){
                auto res=Find(i,j,key,value);
                if(res && value=="\r\n"){
                    read_unlock(i);
                    return 0;
                }
                else if(res){
                    read_unlock(i);
                    return 1;
                }
                continue;
            }
        }
        read_unlock(i);
    }
    return 0;
}
bool REPL::SET(string& key,string& value){
    write_lock(0);
    while(flushrunning){
        write_unlock(0);
        T(flushid);
        write_lock(0);
    }
    if(key.length()+value.length()>=MAX){
        // throw(runtime_error("Size > Max Threshold\nKey not inserted"));
        write_unlock(0);
        return 0;
    }
    append_to_WAL(key,value);
    memtable[key]=value;
    mem_size+=key.length()+value.length();
    filters[0][0].add(key);
    if(mem_size>=MAX){
        flushrunning=1;
        V(flushid);
        merge_unlock(0);
    }
    write_unlock(0);
    return 1;
}
bool REPL::DELETE(string &key){
    return SET(key,TOMBSTONE);
}
