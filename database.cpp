#include"database.h"
#include<sstream>

// Empty string and TOMBSTONE definition
string empty_string="",TOMBSTONE="\r\n";

// Semaphore structures used for synchronization
struct sembuf pop,vop,top;

/**
 * @brief Locks the i-th tier for writing.
 * 
 * This function provides mutual exclusion between multiple writers to the i-th tier.
 * It waits for any ongoing reader operations to complete if this is the first writer.
 * 
 * @param i Index of the tier.
 */
void Database::write_lock(int i){
    P(writer[i]);                                   ///< Mutual exclusion between writers
    ++wcount[i];                                    
    if(wcount[i]==1){                               ///< If there is at least one writer, wait for readers to complete
        P(wsemids[i]);
    }
    V(writer[i]);
    P(mtx[i]);                                      ///< Lock for writers
}

/**
 * @brief Unlocks the i-th tier for writing.
 * 
 * This function releases the write lock on the i-th tier. It updates the number of writers
 * and signals the readers if there are no writers left.
 * 
 * @param i Index of the tier.
 */
void Database::write_unlock(int i){
    P(writer[i]);
    --wcount[i];
    if(!wcount[i]) V(wsemids[i]);                   ///< If there are no writers, signal to readers
    V(writer[i]);
    V(mtx[i]);
}

/**
 * @brief Locks the i-th tier for reading.
 * 
 * This function provides mutual exclusion between readers and ensures that if there are readers,
 * writers will be blocked.
 * 
 * @param i Index of the tier.
 */
void Database::read_lock(int i){
    P(reader[i]);
    ++rcount[i];
    if(rcount[i]==1) P(wsemids[i]);                 ///< If there is at least one reader, wait for writers to complete
    V(reader[i]);
}

/**
 * @brief Unlocks the i-th tier for reading.
 * 
 * This function releases the read lock on the i-th tier and signals writers if there are no readers left.
 * 
 * @param i Index of the tier.
 */
void Database::read_unlock(int i){
    P(reader[i]);   
    --rcount[i];
    if(!rcount[i]) V(wsemids[i]);                   ///< If there are no readers, signal to writers
    V(reader[i]);
}

/**
 * @brief Locks the i-th tier for merging data.
 * 
 * This function locks the semaphore for merging data in the i-th tier.
 * 
 * @param i Index of the tier.
 */
void Database::merge_lock(int i){
    P(semids[i]);
}

/**
 * @brief Unlocks the i-th tier after merging data.
 * 
 * This function releases the lock for merging data in the i-th tier.
 * 
 * @param i Index of the tier.
 */
void Database::merge_unlock(int i){
    V(semids[i]);
}

/**
 * @brief Pushes new semaphores for managing writers, readers, and mergers.
 * 
 * This function initializes and pushes new semaphores for managing operations in the database. These
 * semaphores are used for mutual exclusion and synchronization of writing, reading, and merging.
 */
void Database::push_semaphores(){
    semids.push_back(semget(IPC_PRIVATE,1,IPC_CREAT|0777));                                                      ///< Pushes semaphores for managing mergers, writers, readers
    wsemids.push_back(semget(IPC_PRIVATE,1,IPC_CREAT|0777));
    reader.push_back(semget(IPC_PRIVATE,1,IPC_CREAT|0777));
    writer.push_back(semget(IPC_PRIVATE,1,IPC_CREAT|0777));
    mtx.push_back(semget(IPC_PRIVATE,1,IPC_CREAT|0777));
    wcount.push_back(0);
    rcount.push_back(0);                                                                                           ///< Stores the number of writers and readers respectively
    assert((semids.back()>=0 && wsemids.back()>=0 && writer.back()>=0 && reader.back()>=0 && mtx.back()>=0));    ///< Check if the obtained semaphores are valid
    semctl(semids.back(),0,SETVAL,1);
    semctl(wsemids.back(),0,SETVAL,1);
    semctl(writer.back(),0,SETVAL,1);
    semctl(reader.back(),0,SETVAL,1);
    semctl(mtx.back(),0,SETVAL,1);
}

/**
 * @brief Checks if the i-th tier exists, and if not, creates it and initializes necessary variables.
 * 
 * This function checks the existence of the i-th tier and initializes necessary variables if the
 * tier does not exist.
 * 
 * @param i Index of the tier.
 * @param Tier Reference to the tier path.
 */
void Database::get_folder(int i, path &Tier){
    path database("./Database");
    assert(exists(database)); // Ensure the database folder exists
    Tier = path(database / ("Tier_" + to_string(i)));
    if(!exists(Tier)){
        create_directory(Tier);  // Create the directory if it doesn't exist
        levels_main.push_back(0);
        push_semaphores();  // Initialize semaphores for this tier
        filters.push_back(vector<BloomFilter>());  // Initialize BloomFilter for the tier
    }
}

/**
 * @brief Initializes the memtable by loading data from the Write-Ahead Log (WAL).
 * 
 * This function reloads the memtable from the WAL file during database initialization.
 */
void Database::initialize_memtable(){
    path database("./Database");
    assert(exists(database));
    path WAL(database/"WAL.bin");
    if(!exists(WAL)){
        wal = ofstream(WAL, ios::binary | ios::out);
        assert(!wal.fail());
        return;
    }
    ifstream file;
    file.open(WAL, ios::binary | ios::in);
    assert(!file.fail());
    size_t length;
    string key, val;
    while(file.peek() != EOF){
        file.read(reinterpret_cast<char*>(&length), sizeof(length));
        key.resize(length);
        file.read(&key[0], length);
        mem_size += length;
        file.read(reinterpret_cast<char*>(&length), sizeof(length));
        val.resize(length);
        file.read(&val[0], length);
        mem_size += length;
        memtable[key] = val;
        filters[0][0].add(key);  // Add key to BloomFilter
    }
    file.close();
    wal = ofstream(WAL, ios::binary | ios::out);
    assert(!wal.fail());
    if(mem_size >= MAX){  // Flush to disk if memtable size exceeds MAX
        flushrunning = 1;
        V(flushid);
        merge_unlock(0);
    }
}

/**
 * @brief Initializes the BloomFilter for the i-th tier.
 * 
 * This function initializes the BloomFilter for the i-th tier by loading data from disk.
 * It reads both data files and their corresponding metadata.
 * 
 * @param i Index of the tier.
 * @param filter Reference to the BloomFilter vector for the tier.
 * @param Tier Reference to the path of the tier.
 */
void Database::initialize_filter(int i, vector<BloomFilter>& filter, path& Tier){
    ifstream file, metadata;
    filter.emplace_back();  // Add new BloomFilter instance
    file.open(Tier / (to_string(i) + ".bin"), ios::binary | ios::in);
    metadata.open(Tier / ("metadata" + to_string(i) + ".bin"), ios::binary | ios::in);
    assert((file.fail() || metadata.fail()) == 0);  // Ensure both files exist
    size_t idx, pre_idx;
    string key;
    metadata.read(reinterpret_cast<char*>(&pre_idx), sizeof(pre_idx));
    while(file.peek() != EOF){
        metadata.read(reinterpret_cast<char*>(&idx), sizeof(idx));
        key.resize(idx - pre_idx);
        file.read(&key[0], idx - pre_idx);
        metadata.read(reinterpret_cast<char*>(&pre_idx), sizeof(pre_idx));
        filter.back().add(key);  // Add key to BloomFilter
        file.seekg(pre_idx, ios::beg);
    }
    file.close();
    metadata.close();
}

/**
 * @brief Initializes the folder structure for the i-th tier based on existing data files.
 * 
 * This function checks if the folder for the i-th tier exists and initializes it with the necessary
 * data files and BloomFilters.
 * 
 * @param i Index of the tier.
 * @return true if the tier was successfully initialized, false otherwise.
 */
bool Database::initialize_folder(int i){
    path database("./Database");
    assert(exists(database));
    path Tier(database / ("Tier_" + to_string(i)));
    if(!exists(Tier)){
        return 0;
    }
    int j = 1;
    levels_main.push_back(0);
    filters.push_back(vector<BloomFilter>());
    // Initialize filter if metadata and data files exist for the tier
    while(exists(Tier / ("metadata" + to_string(j) + ".bin")) && exists(Tier / (to_string(j) + ".bin"))){
        initialize_filter(j, filters.back(), Tier);
        ++levels_main[i];
        ++j;
    }
    push_semaphores();
    return 1;
}
/**
 * @brief Renames the temporary files to actual files and increments the `levels_main`.
 * 
 * This function renames the temporary data and metadata files to their actual names and increments
 * the `levels_main` for the specified tier.
 * 
 * @param folder Path to the source folder containing temporary files.
 * @param folder1 Path to the destination folder.
 * @param i Index of the tier.
 */
void Database::Rename(path& folder, path& folder1, int i) {
    ++levels_main[i];
    rename(folder / "temp.bin", folder1 / (to_string(levels_main[i]) + ".bin"));
    rename(folder / "temp1.bin", folder1 / ("metadata" + to_string(levels_main[i]) + ".bin"));
}

/**
 * @brief Merges files from multiple tiers into a temporary file in the next tier.
 * 
 * This function merges files from the current tier and appends them into a temporary file in the
 * next tier. It performs the merging operation and updates the BloomFilter for the merged files.
 * 
 * @param dest The destination output file stream for the merged data.
 * @param meta_dest The destination output file stream for the merged metadata.
 * @param src Vector of input file streams for the source data files.
 * @param meta_src Vector of input file streams for the source metadata files.
 * @param temp The temporary BloomFilter for the merged data.
 * @param last Flag indicating if this is the last merge operation.
 */
void merge(ofstream& dest, ofstream& meta_dest, vector<ifstream>& src, vector<ifstream>& meta_src, BloomFilter& temp, bool last) {
    string *key = 0, *val = 0, prev = "";
    size_t key_idx, val_idx, tot_len = 0, count = 0;
    
    // Helper function to write key-value pairs to destination files
    auto Write = [&] (string& key, string& val) {
        tot_len += key.length();
        dest.write(&key[0], key.length());
        count++;
        meta_dest.write(reinterpret_cast<char*>(&tot_len), sizeof(tot_len));
        tot_len += val.length();
        dest.write(&val[0], val.length());
        meta_dest.write(reinterpret_cast<char*>(&tot_len), sizeof(tot_len));
        temp.add(key);  // Add key to the BloomFilter
    };
    
    bool flag = 0;
    vector<size_t> pre_idx(src.size(), 0);
    vector<pair<string, string>> data(src.size(), {"", ""});
    
    // Read the initial index from metadata files
    for (int j = src.size() - 1; j >= 0; --j) {
        meta_src[j].read(reinterpret_cast<char*>(&pre_idx[j]), sizeof(pre_idx[j]));
    }
    
    // Write initial total length to the metadata
    meta_dest.write(reinterpret_cast<char*>(&tot_len), sizeof(tot_len));
    
    // Perform the merging of files
    while (1) {
        flag = 0;
        for (int j = src.size() - 1; j >= 0; --j) {
            if (data[j].first > prev) {
                if (!key || data[j].first < *key) {
                    key = &data[j].first;
                    val = &data[j].second;
                }
                flag = 1;
            } 
            else if(src[j].peek() != EOF) {
                meta_src[j].read(reinterpret_cast<char*>(&key_idx), sizeof(key_idx));
                meta_src[j].read(reinterpret_cast<char*>(&val_idx), sizeof(val_idx));
                data[j].first.resize(key_idx - pre_idx[j]);
                src[j].read(&data[j].first[0], key_idx - pre_idx[j]);
                data[j].second.resize(val_idx - key_idx);
                src[j].read(&data[j].second[0], val_idx - key_idx);
                pre_idx[j] = val_idx;
                if (!key || data[j].first < *key) {
                    key = &data[j].first;
                    val = &data[j].second;
                }
                flag = 1;
            }
        }
        
        if (!flag) break;
        prev = *key;
        
        // Skip Tombstone entries if this is the last merge operation
        if (!(last && *val == TOMBSTONE)) {
            ++count;
            Write(*key, *val);
        }
        key = 0;
    }
    
    // Write the total number of entries to the metadata
    meta_dest.write(reinterpret_cast<char*>(&count), sizeof(count));
}

/**
 * @brief Compacts the data in Tier_i and merges it into Tier_i+1.
 * 
 * This function performs the compaction of data in Tier_i, merges the files, and appends the
 * result to Tier_i+1 as a temporary file.
 * 
 * @param i Index of the current tier to be compacted.
 */
void Database::compact(int i) {
    ofstream temp, meta_temp;
    path folder, folder1;
    bool last = i == (int)(levels_main.size()) - 1;
    
    get_folder(i, folder);
    get_folder(i + 1, folder1);
    
    BloomFilter temp1;
    
    // Multi-way merge function for merging files from multiple tiers
    auto multi_way_merge = [&] (int i) {
        vector<ifstream> datafiles(levels_main[i]), metafiles(levels_main[i]);
        temp.open(folder1 / "temp.bin", ios::out | ios::binary | ios::trunc);
        meta_temp.open(folder1 / "temp1.bin", ios::out | ios::binary | ios::trunc);
        assert((temp.fail() || meta_temp.fail()) == 0);
        
        // Open data and metadata files for each tier
        for (int j = 0; j < levels_main[i]; ++j) {
            datafiles[j].open(folder / (to_string(j + 1) + ".bin"), ios::binary | ios::in);
            metafiles[j].open(folder / ("metadata" + to_string(j + 1) + ".bin"), ios::binary | ios::in);
            assert((datafiles[j].fail() || metafiles[j].fail()) == 0);
        }
        
        // Perform the merge operation
        merge(temp, meta_temp, datafiles, metafiles, temp1, last);
        
        // Close all data and metadata files
        for (auto j = 0; j < levels_main[i]; ++j) {
            datafiles[j].close();
            metafiles[j].close();
        }
        temp.close();
        meta_temp.close();
    };
    
    multi_way_merge(i);  // Perform multi-way merge
    
    write_lock(i);  // Lock for writing as filenames are changed from temp to actual
    for (int j = 1; j <= levels_main[i]; ++j) {
        remove(folder / (to_string(j) + ".bin"));
        remove(folder / ("metadata" + to_string(j) + ".bin"));
    }
    
    filters[i].clear();
    levels_main[i] = 0;
    
    merge_lock(i + 1);  // Lock for merging in the next tier
    write_lock(i + 1);  // Lock for writing to Tier_i+1
    
    filters[i + 1].push_back(temp1);
    Rename(folder1, folder1, i + 1);
    
    merge_unlock(i);  // Unlock after merging in Tier_i
    write_unlock(i);  // Unlock after writing in Tier_i
    write_unlock(i + 1);  // Unlock after writing in Tier_i+1
    
    if (levels_main[i + 1] >= MIN_TH) {  // If number of files in Tier_i+1 exceeds threshold, initiate further compaction
        this->compact(i + 1);
        return;
    }
    
    merge_unlock(i + 1);  // Unlock merging in the next tier
}

/**
 * @brief Main thread for calling compact operations in a loop.
 * 
 * This function continuously monitors the `compactid` semaphore and initiates compaction operations
 * when required. It also ensures that the threads are properly joined before terminating the function.
 */
void Database::compact_main() {
    vector<thread> compact_threads;
    
    while (1) {
        P(compactid);
        if (destroy) {
            for (size_t i = 0; i < compact_threads.size(); ++i) compact_threads[i].join();
            return;
        }
        compact_threads.push_back(thread([this](){ compact(1); }));
    }
}

/**
 * @brief Performs binary search on the data file using the metadata file.
 * 
 * This function performs a binary search over a data file using the metadata file, which contains
 * the indexes of each string in the data file. It returns true if the key is found, false otherwise.
 * 
 * @param In Input file stream for the data file.
 * @param metadata Input file stream for the metadata file.
 * @param entries Total number of entries in the data file.
 * @param key The key to be searched for.
 * @param value The value associated with the key, returned if found.
 * @return true if the key was found, false otherwise.
 */
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
/**
 * @brief Does bookkeeping and calls binary_search for searching key in the file using binary search.
 * 
 * This function calls binary_search for looking for a specific key in the data files of a given tier and file, 
 * performs a binary search using metadata, and retrieves the corresponding value.
 * 
 * @param i The tier level where the data file is located.
 * @param j The index of the specific file in the tier.
 * @param key The key to search for.
 * @param value The value corresponding to the key if found.
 * @return true If the key is found, false otherwise.
 */
bool Database::Find(int i, int j, string &key, string &value) {
    size_t entries;
    path Tier;
    get_folder(i, Tier);
    path File(Tier / (to_string(j) + ".bin")), metafile(Tier / ("metadata" + to_string(j) + ".bin"));
    
    assert(exists(File));

    ifstream In, metadata;
    In.open(File, ios::in | ios::binary);
    metadata.open(metafile, ios::in | ios::binary);
    
    assert((In.fail() || metadata.fail()) == 0);

    metadata.seekg(-sizeof(size_t), ios::end); // last entry in metadata file contains the number of entries
    metadata.read(reinterpret_cast<char*>(&entries), sizeof(entries));
    
    auto res = binary_search(In, metadata, entries, key, value);
    
    In.close();
    metadata.close();
    
    return res;
}

/**
 * @brief Appends a key-value pair to the Write-Ahead Log (WAL).
 * 
 * This function writes a key-value pair to the WAL, including the lengths of 
 * both the key and value, followed by the actual data.
 * 
 * @param key The key to be written to the WAL.
 * @param val The value to be written to the WAL.
 */
void Database::append_to_WAL(string &key, string &val) {
    size_t length = key.size();
    wal.write(reinterpret_cast<char*>(&length), sizeof(length));
    wal.write(&key[0], key.length());

    length = val.length();
    wal.write(reinterpret_cast<char*>(&length), sizeof(length));
    wal.write(&val[0], val.length());

    wal.flush();
}

/**
 * @brief Initializes the database by setting up necessary structures and background tasks.
 * 
 * This constructor sets up Bloom filters, initializes directories, prepares the WAL, 
 * and starts background threads for compaction and flushing. It ensures the database 
 * is ready for operation by setting the initial levels and performing any necessary 
 * compactions.
 */
Database::Database() {
    filters.push_back(vector<BloomFilter>(1));
    cout << "INITIALIZING DATABASE....\n";
    
    path database("./Database");
    if (!exists(database)) {
        create_directory(database);
    }

    levels_main.push_back(1);
    mem_size = 0;

    // Initialize database (including folders and compactions)
    auto initialize_database = [&]() {
        int i = 0;
        while (initialize_folder(++i));

        for (int j = i - 1; j > 0; --j) {
            if (levels_main[j] >= MIN_TH) {
                merge_lock(j);
                compact(j);
            }
        }
        initialize_memtable();
    };

    // Initialize semaphores and other synchronization mechanisms
    flushid = semget(IPC_PRIVATE, 1, IPC_CREAT | 0777);
    compactid = semget(IPC_PRIVATE, 1, IPC_CREAT | 0777);
    destroy = 0;
    semids.push_back(semget(IPC_PRIVATE, 1, IPC_CREAT | 0777));
    wsemids.push_back(semget(IPC_PRIVATE, 1, IPC_CREAT | 0777));
    reader.push_back(semget(IPC_PRIVATE, 1, IPC_CREAT | 0777));
    writer.push_back(semget(IPC_PRIVATE, 1, IPC_CREAT | 0777));
    mtx.push_back(semget(IPC_PRIVATE, 1, IPC_CREAT | 0777));
    wcount.push_back(0);
    rcount.push_back(0);
    flushrunning = 0;

    // Initialize semaphore values
    pop = (struct sembuf){0, -1, 0}, vop = (struct sembuf){0, 1, 0}, top = (struct sembuf){0, 0, 0};
    assert((flushid < 0 || compactid < 0 || semids[0] < 0 || wsemids[0] < 0 || writer[0] < 0 || reader[0] < 0 || mtx[0] < 0) == 0);

    // Set initial values for semaphores
    semctl(flushid, 0, SETVAL, 0);
    semctl(compactid, 0, SETVAL, 0);
    semctl(semids[0], 0, SETVAL, 0);
    semctl(wsemids[0], 0, SETVAL, 1);
    semctl(writer[0], 0, SETVAL, 1);
    semctl(reader[0], 0, SETVAL, 1);
    semctl(mtx[0], 0, SETVAL, 1);

    // Start compaction and flushing threads
    compact_main_thread = thread([this]() { compact_main(); });
    flush_thread = thread([this]() { FLUSH(); });

    initialize_database();
    
    cout << "DATABASE INITIALIZED\n";
}

/**
 * @brief Destructor that cleans up resources and terminates background threads.
 * 
 * This destructor ensures the database is properly cleaned up when it is destroyed, 
 * releasing resources such as semaphores and threads. It sets the destroy flag to 
 * stop the FLUSH and compaction threads.
 */
Database::~Database() {
    destroy = 1;
    merge_unlock(0);
    V(compactid);
    V(flushid);
    
    // Clean up semaphores
    for (auto i : semids) semctl(i, 0, IPC_RMID, 0);
    for (auto i : wsemids) semctl(i, 0, IPC_RMID, 0);
    for (auto i : mtx) semctl(i, 0, IPC_RMID, 0);
    for (auto i : reader) semctl(i, 0, IPC_RMID, 0);
    for (auto i : writer) semctl(i, 0, IPC_RMID, 0);

    compact_main_thread.join();
    flush_thread.join();

    wal.close();
    semctl(flushid, 0, IPC_RMID, 0);
    semctl(compactid, 0, IPC_RMID, 0);
}

/**
 * @brief Flushes the memtable to a new sstable on disk.
 * 
 * This function writes the current memtable (in-memory key-value pairs) to a new 
 * sstable on disk. It also handles writing the associated metadata and updating 
 * the Bloom filter.
 */
void Database::FLUSH() {
    size_t tot_size = 0, nentries;
    BloomFilter temp;
    ofstream file, metadata;

    while (1) {
        merge_lock(0);
        if (destroy) {
            return;
        }

        path folder;
        get_folder(1, folder);
        file.open(folder / "temp.bin", ios::out | ios::binary);
        metadata.open(folder / "temp1.bin", ios::out | ios::binary);

        tot_size = 0;
        assert((file.fail() || metadata.fail()) == 0);

        metadata.write(reinterpret_cast<char*>(&tot_size), sizeof(tot_size));
        
        // Write all entries from memtable to file and metadata
        for (auto &[i, j] : memtable) {
            file.write(&i[0], i.length());
            file.write(&j[0], j.length());

            tot_size += i.length();
            metadata.write(reinterpret_cast<char*>(&tot_size), sizeof(tot_size));

            tot_size += j.length();
            metadata.write(reinterpret_cast<char*>(&tot_size), sizeof(tot_size));

            temp.add(i);
        }

        nentries = memtable.size();
        metadata.write(reinterpret_cast<char*>(&nentries), sizeof(nentries));

        file.close();
        metadata.close();

        write_lock(0);
        memtable.clear();
        filters[0][0].clear();
        mem_size = 0;

        merge_lock(1);
        write_lock(1);

        filters[1].push_back(temp);

        Rename(folder, folder, 1);

        if (levels_main[1] >= MIN_TH) {
            V(compactid);
        } 
        else {
            merge_unlock(1);
        }

        wal.close();

        // Reopen WAL in truncate mode
        wal.open("./Database/WAL.bin", ios::binary | ios::trunc);
        assert(wal.fail() == 0);

        flushrunning = 0;
        P(flushid);

        write_unlock(0);
        write_unlock(1);
    }
}

/**
 * @brief Constructs a new Bloom filter for the database.
 * 
 * This constructor initializes a Bloom filter with a set number of filters.
 */
BloomFilter::BloomFilter() {
    for (int i = 0; i < NOFILTERS; ++i) {
        filter.push_back(hash<string>());
    }
}

/**
 * @brief Clears the Bloom filter's bit array.
 * 
 * This function resets the bit array to its initial state (empty).
 */
void BloomFilter::clear(){

    bitArray=0;
}
/**
 * @brief Adds a key to the Bloom filter.
 * 
 * This function hashes the given key using multiple hash functions and 
 * sets the corresponding bits in the Bloom filter's bit array.
 * 
 * @param key The key to be added to the Bloom filter.
 */
void BloomFilter::add(const string& key) {
    int hash;
    for (int i = 0; i < filter.size(); ++i) {
        hash = (filter[i](key) + i) % bitArray.size();
        bitArray[hash] = true;
    }
}

/**
 * @brief Checks whether a key is present in the Bloom filter.
 * 
 * This function checks the bits corresponding to the hash values of the given 
 * key. If any of the bits are not set, the key is not present in the Bloom filter.
 * 
 * @param key The key to be checked in the Bloom filter.
 * @return true If the key is possibly present in the Bloom filter, false if it is definitely not present.
 */
bool BloomFilter::contains(string& key) {
    int hash;
    for (int i = 0; i < filter.size(); ++i) {
        hash = (filter[i](key) + i) % bitArray.size();
        if (!bitArray[hash]) return 0;  // The key is definitely not in the filter.
    }
    return 1;  // The key is possibly in the filter.
}

