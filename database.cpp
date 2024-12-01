#include"database.h"
#include<sstream>
string empty_string="",TOMBSTONE="\r\n";
struct sembuf pop,vop,top;
//It locks for writing to i-th Tier
void Database::write_lock(int i){
    P(writer[i]);                                   //mutual exlcusion between writers
    ++wcount[i];                                    
    if(wcount[i]==1){                               //if there is atleast one writer, wait for reader to complete
        P(wsemids[i]);
    }
    V(writer[i]);
    P(mtx[i]);                                      //lock for writers
}
//It unlocks for writing to i-th Tier
void Database::write_unlock(int i){
    P(writer[i]);
    --wcount[i];
    if(!wcount[i]) V(wsemids[i]);                   //if there are no writers, then signal
    V(writer[i]);
    V(mtx[i]);
}
void Database::read_lock(int i){
    P(reader[i]);
    ++rcount[i];
    if(rcount[i]==1) P(wsemids[i]);                 //if there is atleast one reader, wait for writers to complete
    V(reader[i]);
}
void Database::read_unlock(int i){
    P(reader[i]);   
    --rcount[i];
    if(!rcount[i]) V(wsemids[i]);                   //if there are no readers, then signal
    V(reader[i]);
}
//It locks for merging i-th Tier data
void Database::merge_lock(int i){
    P(semids[i]);
}
void Database::merge_unlock(int i){
    V(semids[i]);
}
void Database::push_semaphores(){
    semids.push_back(semget(IPC_PRIVATE,1,IPC_CREAT|0777));                                                      //pushes sempahores for managaing mergers,writers,readers
    wsemids.push_back(semget(IPC_PRIVATE,1,IPC_CREAT|0777));
    reader.push_back(semget(IPC_PRIVATE,1,IPC_CREAT|0777));
    writer.push_back(semget(IPC_PRIVATE,1,IPC_CREAT|0777));
    mtx.push_back(semget(IPC_PRIVATE,1,IPC_CREAT|0777));
    wcount.push_back(0);
    rcount.push_back(0);                                                                                           //stores no of writers and readers respectively
    assert((semids.back()>=0 && wsemids.back()>=0 && writer.back()>=0 && reader.back()>=0 && mtx.back()>=0));    //checks if the obtained semaphores are valid
    semctl(semids.back(),0,SETVAL,1);
    semctl(wsemids.back(),0,SETVAL,1);
    semctl(writer.back(),0,SETVAL,1);
    semctl(reader.back(),0,SETVAL,1);
    semctl(mtx.back(),0,SETVAL,1);
}
//It checks if Tier_i exists, if not, it creates Tier and initializes necessay variables 
void Database::get_folder(int i,path &Tier){
    path database("./Database");
    assert(exists(database));
    Tier=path(database/("Tier_"+to_string(i)));
    if(!exists(Tier)){
        create_directory(Tier);
        levels_main.push_back(0);
        push_semaphores();
        filters.push_back(vector<BloomFilter>());                                                           //pushes BloomFilter for Tier_i 
    }
}
//It reloads memtable from WAL during initialization
void Database::initialize_memtable(){
    path database("./Database");
    assert(exists(database));
    path WAL(database/"WAL.bin");
    if(!exists(WAL)){
        wal=ofstream(WAL,ios::binary|ios::out);
        assert(!wal.fail());
        return;
    }
    ifstream file;
    file.open(WAL,ios::binary|ios::in);
    assert(!file.fail());
    size_t length;
    string key,val;
    while(file.peek()!=EOF){
        file.read(reinterpret_cast<char*>(&length),sizeof(length));
        key.resize(length);
        file.read(&key[0],length);
        mem_size+=length;
        file.read(reinterpret_cast<char*>(&length),sizeof(length));
        val.resize(length);
        file.read(&val[0],length);
        memtable[key]=val;
        mem_size+=length;
        filters[0][0].add(key);                                         //adds key to Bloomfilter
    }
    file.close();
    wal=ofstream(WAL,ios::binary|ios::out);
    assert(!wal.fail());
    if(mem_size>=MAX){                                                  //flush to disk if size of memtable>=MAX
        flushrunning=1;
        V(flushid);
        merge_unlock(0);
    }
}
//it initializes BloomFilter for Tier_1 during database initialization
void Database::initialize_filter(int i,vector<BloomFilter>& filter,path& Tier){
    ifstream file,metadata;
    filter.emplace_back();
    file.open(Tier/(to_string(i)+".bin"),ios::binary|ios::in);
    metadata.open(Tier/("metadata"+to_string(i)+".bin"),ios::binary|ios::in);
    assert((file.fail() || metadata.fail())==0);
    size_t idx,pre_idx;
    string key;
    metadata.read(reinterpret_cast<char*>(&pre_idx),sizeof(pre_idx));
    while(file.peek()!=EOF){
        metadata.read(reinterpret_cast<char*>(&idx),sizeof(idx));
        key.resize(idx-pre_idx);
        file.read(&key[0],idx-pre_idx);
        metadata.read(reinterpret_cast<char*>(&pre_idx),sizeof(pre_idx));
        filter.back().add(key);                                         //reads data from the file, and adds it to the filter
        file.seekg(pre_idx,ios::beg);
    }
    file.close(),metadata.close();
}
//It initializes the variables based on the Tiers and data files present in the database directory
//during database initialization
bool Database::initialize_folder(int i){
    path database("./Database");
    assert(exists(database));
    path Tier(database/("Tier_"+to_string(i)));
    if(!exists(Tier)){
        return 0;
    }
    int j=1;
    levels_main.push_back(0);
    filters.push_back(vector<BloomFilter>());
    //if data files and its metadata files are there, then initialize filter for them, and update levels_main
    while(exists(Tier/("metadata"+to_string(j)+".bin")) && exists(Tier/(to_string(j)+".bin"))){
        initialize_filter(j,filters.back(),Tier);
        ++levels_main[i];
        ++j;
    }
    push_semaphores();
    return 1;
}
//It renames the file from temporary to actual name, and increments the levels_main
void Database::Rename(path&folder,path&folder1,int i){
    ++levels_main[i];
    rename(folder/"temp.bin",folder1/(to_string(levels_main[i])+".bin"));
    rename(folder/"temp1.bin",folder1/("metadata"+to_string(levels_main[i])+".bin"));
}
//It merges the files in Tier_1 and appends it as a temp file in Tier i+1
void merge(ofstream &dest,ofstream &meta_dest,vector<ifstream>& src,vector<ifstream>& meta_src,BloomFilter& temp,bool last){
    string *key=0,*val=0,prev="";
    size_t key_idx,val_idx,tot_len=0,count=0;
    //It writes the key, value pair in dest and its metadata meta_dest files, and appends it in its BloomFilter
    auto Write=[&](string& key,string&val){
        tot_len+=key.length();
        dest.write(&key[0],key.length());
        count++;
        meta_dest.write(reinterpret_cast<char*>(&tot_len),sizeof(tot_len));
        tot_len+=val.length();
        dest.write(&val[0],val.length());
        meta_dest.write(reinterpret_cast<char*>(&tot_len),sizeof(tot_len));
        temp.add(key);
    };
    bool flag=0;
    vector<size_t> pre_idx(src.size(),0);
    vector<pair<string,string>> data(src.size(),{"",""});
    for(int j=src.size()-1;j>=0;--j) meta_src[j].read(reinterpret_cast<char*>(&pre_idx[j]),sizeof(pre_idx[j]));
    meta_dest.write(reinterpret_cast<char*>(&tot_len),sizeof(tot_len));
    //It does merging
    while(1){
        flag=0;
        for(int j=src.size()-1;j>=0;--j){
            if(data[j].first>prev){
                if(!key || data[j].first<*key){
                    key=&data[j].first;
                    val=&data[j].second;
                }
                flag=1;
            }
            else if(src[j].peek()!=EOF){
                meta_src[j].read(reinterpret_cast<char*>(&key_idx),sizeof(key_idx));
                meta_src[j].read(reinterpret_cast<char*>(&val_idx),sizeof(val_idx));
                data[j].first.resize(key_idx-pre_idx[j]);
                src[j].read(&data[j].first[0],key_idx-pre_idx[j]);
                data[j].second.resize(val_idx-key_idx);
                src[j].read(&data[j].second[0],val_idx-key_idx);
                pre_idx[j]=val_idx;
                if(!key || data[j].first<*key){
                    key=&data[j].first;
                    val=&data[j].second;
                }
                flag=1;
            }
        }
        if(!flag) break;
        prev=*key;
        if(!(last && *val==TOMBSTONE)){
            ++count;
            Write(*key,*val);
        }
        key=0;
        
    }
    //Writes no of entries in the merged file
    meta_dest.write(reinterpret_cast<char*>(&count),sizeof(count));
    return;
}
//It compacts the files in Tier_i, i.e. calls merge to merge the files and add it to Tier_i+1
void Database::compact(int i){
    ofstream temp,meta_temp;
    path folder,folder1;
    bool last=i==(int)(levels_main.size())-1;
    get_folder(i,folder);
    get_folder(i+1,folder1);
    BloomFilter temp1;
    auto multi_way_merge=[&](int i){
        vector<ifstream> datafiles(levels_main[i]), metafiles(levels_main[i]);
        temp.open(folder1/("temp.bin"),ios::out|ios::binary|ios::trunc);
        meta_temp.open(folder1/("temp1.bin"),ios::out|ios::binary|ios::trunc);
        assert((temp.fail() || meta_temp.fail())==0);
        for(int j=0;j<levels_main[i];++j){                                                      //opens all data and meta files in the Tier an passes it to merge for merging
            datafiles[j].open(folder/(to_string(j+1)+".bin"),ios::binary|ios::in);
            metafiles[j].open(folder/("metadata"+to_string(j+1)+".bin"),ios::binary|ios::in);
            assert((datafiles[j].fail() || metafiles[j].fail())==0);
        }
        merge(temp,meta_temp,datafiles,metafiles,temp1,last);
        for(auto j=0;j<levels_main[i];++j){
            datafiles[j].close();
            metafiles[j].close();
        }
        temp.close();
        meta_temp.close();
    };
    multi_way_merge(i);                                                                         //It calls multi-way merge for merging
    write_lock(i);                                                                              //locks for writing as file name is changed from temp to actual,
    for(int j=1;j<=levels_main[i];++j){                                                         //so that there is no race condition for reading and writing
        remove(folder/(to_string(j)+".bin"));                                                   
        remove(folder/("metadata"+to_string(j)+".bin"));
    }
    filters[i].clear();
    levels_main[i]=0;
    merge_lock(i+1);                                                                            //merge_lock is accessed to stop race condition for merging, as otherwise there might be
    write_lock(i+1);                                                                            //race condition for naming files, if Tier_i+1 and renaming in Tier_i+1 is done at the same time
    filters[i+1].push_back(temp1);
    Rename(folder1,folder1,i+1);
    merge_unlock(i);
    write_unlock(i);
    write_unlock(i+1);
    if(levels_main[i+1]>=MIN_TH){                                                               // if no of files >=threshold in Tier_i+1, then don't release merge_lock before merging Tier_i+1
        this->compact(i+1);
        return;
    }
    merge_unlock(i+1);
    
}
//main thread which calls compact threads for coompaction
void Database::compact_main(){
    vector<thread> compact_threads;
    while(1){
        P(compactid);
        if(destroy){
            for (size_t i=0;i<compact_threads.size();++i) compact_threads[i].join();
            return;
        }
        compact_threads.push_back(thread([this](){compact(1);}));
    }
    

}
//It does binary search over In data files using metadata file which contains the idx of each string in data file
bool Database::binary_search(ifstream &In,ifstream& metadata,size_t entries,string &key,string& value){
    long long lo=0,hi=(long long)(entries)-1;
    vector<size_t> idx(3);
    auto find_idx=[&](int mid){
        metadata.seekg(2*sizeof(size_t)*mid,ios::beg);
        metadata.read(reinterpret_cast<char*>(&idx[0]),3*sizeof(idx[0]));   
        return;
    };
    while(lo<=hi){
        long long mid=(lo+hi)>>1;
        find_idx(mid);
        In.seekg(idx[0],ios::beg);
        value.resize(idx[1]-idx[0]);
        In.read(&value[0],idx[1]-idx[0]);
        if(key<value){
            hi=mid-1;
        }
        else if(key==value){
            value.resize(idx[2]-idx[1]);
            In.read(&value[0],idx[2]-idx[1]);
            return 1;
        }
        else{
            lo=mid+1;
        }

    }
    return 0;
}
//It does bookkeeping and calls binary search for finding the key
bool Database::Find(int i,int j,string &key,string& value){
    size_t entries;
    path Tier;
    get_folder(i,Tier);
    path File(Tier/(to_string(j)+".bin")),metafile(Tier/("metadata"+to_string(j)+".bin"));
    assert(exists(File));
    ifstream In,metadata;
    In.open(File,ios::in | ios::binary);
    metadata.open(metafile,ios::in | ios::binary);
    assert((In.fail() || metadata.fail())==0);
    metadata.seekg(-sizeof(size_t),ios::end);                                       //last entry in metadata file contains no of entries
    metadata.read(reinterpret_cast<char*>(&entries),sizeof(entries));
    auto res=binary_search(In,metadata,entries,key,value);
    In.close();
    metadata.close();
    return res;
}
//It appends the data to be set in the WAL
void Database::append_to_WAL(string& key,string&val){
    size_t length=key.size();
    wal.write(reinterpret_cast<char*>(&length),sizeof(length));
    wal.write(&key[0],key.length());
    length=val.length();
    wal.write(reinterpret_cast<char*>(&length),sizeof(length));
    wal.write(&val[0],val.length());
    wal.flush();
}
//It initializes the Database by calling necessary functions like initialize_database and other reqd variables
Database::Database(){
    filters.push_back(vector<BloomFilter>(1));
    cout<<"INITIALIZING DATABASE....\n";
    path database("./Database");
    if(!exists(database)){
        create_directory(database);
    }
    levels_main.push_back(1);
    mem_size=0;
    auto initialize_database=[&](){
        int i=0;
        while(initialize_folder(++i));
        for(int j=i-1;j>0;--j){
            if(levels_main[j]>=MIN_TH){
                merge_lock(j);
                compact(j);
            }
        }
        initialize_memtable();
    };
    flushid=semget(IPC_PRIVATE,1,IPC_CREAT|0777),compactid=semget(IPC_PRIVATE,1,IPC_CREAT|0777),destroy=0;
    semids.push_back(semget(IPC_PRIVATE,1,IPC_CREAT|0777));
    wsemids.push_back(semget(IPC_PRIVATE,1,IPC_CREAT|0777));
    reader.push_back(semget(IPC_PRIVATE,1,IPC_CREAT|0777));
    writer.push_back(semget(IPC_PRIVATE,1,IPC_CREAT|0777));
    mtx.push_back(semget(IPC_PRIVATE,1,IPC_CREAT|0777));
    wcount.push_back(0);
    rcount.push_back(0);
    flushrunning=0;
    pop=(struct sembuf){0,-1,0},vop=(struct sembuf){0,1,0},top=(struct sembuf){0,0,0};
    assert((flushid<0 || compactid<0 || semids[0]<0 || wsemids[0]<0 || writer[0]<0 || reader[0]<0 || mtx[0]<0)==0);
    semctl(flushid,0,SETVAL,0);
    semctl(compactid,0,SETVAL,0);
    semctl(semids[0],0,SETVAL,0);
    semctl(wsemids[0],0,SETVAL,1);
    semctl(writer[0],0,SETVAL,1);
    semctl(reader[0],0,SETVAL,1);
    semctl(mtx[0],0,SETVAL,1);
    compact_main_thread=thread([this]() { compact_main(); });
    flush_thread=thread([this]() { FLUSH(); });
    initialize_database();
    cout<<"DATABASE INITIALIZED\n";
}
//It deconstructs the database and sets destroy variable to terminate FLUSH and compact_main threads
Database::~Database(){
    destroy=1;
    merge_unlock(0);
    V(compactid);
    V(flushid);
    for(auto i:semids) semctl(i,0,IPC_RMID,0);
    for(auto i:wsemids) semctl(i,0,IPC_RMID,0);
    for(auto i:mtx) semctl(i,0,IPC_RMID,0);
    for(auto i:reader) semctl(i,0,IPC_RMID,0);
    for(auto i:writer) semctl(i,0,IPC_RMID,0);
    compact_main_thread.join();
    flush_thread.join();
    wal.close();
    semctl(flushid,0,IPC_RMID,0);
    semctl(compactid,0,IPC_RMID,0);
}
//It flushes the memtable to new sstable in disk
void Database::FLUSH(){
    size_t tot_size=0,nentries;
    BloomFilter temp;
    ofstream file,metadata;
    while(1){
        merge_lock(0);
        if(destroy){
            return;
        }
        path folder;
        get_folder(1,folder);
        file.open(folder/"temp.bin",ios::out| ios::binary);
        metadata.open(folder/"temp1.bin",ios::out| ios::binary);
        tot_size=0;
        assert((file.fail() || metadata.fail())==0);
        metadata.write(reinterpret_cast<char*>(&tot_size),sizeof(tot_size));
        for(auto &[i,j]:memtable){
            file.write(&i[0],i.length());
            file.write(&j[0],j.length());
            tot_size+=i.length();
            metadata.write(reinterpret_cast<char*>(&tot_size),sizeof(tot_size));
            tot_size+=j.length();
            metadata.write(reinterpret_cast<char*>(&tot_size),sizeof(tot_size));
            temp.add(i);
        }
        nentries=memtable.size();
        metadata.write(reinterpret_cast<char*>(&nentries),sizeof(nentries));
        file.close();
        metadata.close();
        write_lock(0);
        memtable.clear();
        filters[0][0].clear();
        mem_size=0;
        merge_lock(1);
        write_lock(1);
        filters[1].push_back(temp);
        Rename(folder,folder,1);
        if(levels_main[1]>=MIN_TH){
            V(compactid);
        }
        else merge_unlock(1);
        wal.close();
        //clears WAL, i.e. closes and reopens it in truncate mode
        wal.open("./Database/WAL.bin",ios::binary|ios::trunc);
        assert(wal.fail()==0);
        //for bookkeeping about whether FLUSH is running
        flushrunning=0;
        P(flushid);
        write_unlock(0);
        write_unlock(1);
    }
}
//BloomFilter instance
BloomFilter::BloomFilter(){
    for(int i=0;i<NOFILTERS;++i) filter.push_back(hash<string>());
}
//It clears the bitArray
void BloomFilter::clear(){
    bitArray=0;
}
//It adds a key to the BloomFilter
void BloomFilter::add(const string&key){
    int hash;
    for(int i=0;i<filter.size();++i){
        hash=(filter[i](key) + i) % bitArray.size();
        bitArray[hash] = true;
    }
}
//It checks whether a key is there in the BloomFilter
bool BloomFilter::contains(string& key){
    int hash;
    for(int i=0;i<filter.size();++i){
        hash=(filter[i](key)+i) % bitArray.size();
        if(!bitArray[hash]) return 0;
    }
    return 1;
}
