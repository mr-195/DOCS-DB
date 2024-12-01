#include"REPL.h"
bool REPL::GET(string &key,string& value){
    read_lock(0);                                                   //locks for reading
    if(filters[0][0].contains(key)){                                //checks if the key is there in the BloomFilter for memtable
        auto pointer=memtable.find(key);                            //finds in the memtable, if it is there
        if(pointer!=memtable.end()){                                
            if (pointer->second!=TOMBSTONE) {                          //"\r\n" is used as TOMBSTONE, that is it is marked for deletion
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
    for(int i=1;i<(int)levels_main.size();++i){
        read_lock(i);
        for(int j=levels_main[i];j>0;--j){
            if(filters[i][j-1].contains(key)){                      
                auto res=Find(i,j,key,value);                           //calls Find function to find if BloomFliter contains it
                if(res && value==TOMBSTONE){
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
        T(flushid);                                                                         //checks if FLUSH is running, if so wait and unlock write for 0 for FLUSH to run
        write_lock(0);
    }
    if(key.length()+value.length()>=MAX){                                                   //if the total length>=MAX, then raise error
        write_unlock(0);
        return 0;
    }
    append_to_WAL(key,value);                                                               //append to WAL
    memtable[key]=value;                                                                    //insert it into memtable
    mem_size+=key.length()+value.length();                                                  //increment memtable size to check whether it should be flushed
    filters[0][0].add(key);                                                                 //add to BloomFilter
    if(mem_size>=MAX){                                                                      //if mem_size>=MAX flush          
        flushrunning=1;
        V(flushid);
        merge_unlock(0);
    }
    write_unlock(0);
    return 1;
}
bool REPL::DELETE(string &key){                                                             //SET to TOMBSTONE for marking as deleted
    return SET(key,TOMBSTONE);
}
