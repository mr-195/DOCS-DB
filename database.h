#include<bits/stdc++.h>
#include<sys/sem.h>
#include<filesystem>
#include <bitset>
#define MAX 60
#define MIN_TH 4
#define MAX_TH 12
#define NOFILTERS 3
using namespace std;
using namespace filesystem;
using std::atomic;
#ifndef HI
#define HI
extern struct sembuf vop,pop,top;
#define V(semid) semop(semid,&vop,1)
#define P(semid) semop(semid,&pop,1)
#define T(semid) semop(semid,&top,1)
extern string empty_string,TOMBSTONE;
class BloomFilter {
private:
    vector<hash<string>> filter;
    bitset<1000> bitArray;

public:
    BloomFilter();

    void add(const string& key);
    bool contains(string& key);
    void clear();
    bitset<1000>& get();
};
class Database{
    protected:
    int flushid,compactid;
    bool destroy;
    vector<vector<BloomFilter>> filters;
    ofstream wal;
    map<string,string> memtable;
    vector<int> levels_main;
    size_t mem_size;
    // vector<atomic_flag> mtx;
    vector<int> semids,wsemids,wcount,rcount,reader,writer,mtx;
    bool flushrunning;
    thread compact_main_thread,flush_thread;
    bool Find(int,int,string&,string&);
    bool binary_search(ifstream &In,ifstream&,size_t entries,string &key,string& value);
    void compact_main();
    void FLUSH();
    Database();
    ~Database();
    void Rename(path&,path&,int);
    void compact(int);
    void write_lock(int);
    void write_unlock(int);
    void read_lock(int);
    void read_unlock(int);
    void merge_lock(int);
    void merge_unlock(int);
    void get_folder(int i,path&);
    bool initialize_folder(int i);
    void initialize_memtable();
    void append_to_WAL(string&,string&);
    void initialize_filter(int i,vector<BloomFilter>& filter,path& Tier);
};

#endif
