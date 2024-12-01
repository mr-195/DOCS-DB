#include<bits/stdc++.h>
#include<sys/sem.h>
#include<filesystem>
#include<bitset>
#define MAX 4000000
#define MIN_TH 4
#define MAX_TH 12
#define NOFILTERS 3
using namespace std;
using namespace filesystem;
using std::atomic;
#ifndef HI
#define HI
extern struct sembuf vop, pop, top;
#define V(semid) semop(semid, &vop, 1)
#define P(semid) semop(semid, &pop, 1)
#define T(semid) semop(semid, &top, 1)
extern string empty_string, TOMBSTONE;

/**
 * @class BloomFilter
 * @brief Implements a probabilistic function with false-positive chances to check whether a key exists in a file.
 */
class BloomFilter {
private:
    vector<hash<string>> filter; ///< Hash functions for the Bloom filter.
    bitset<10000> bitArray;      ///< Bit array to store hash results.

public:
    /**
     * @brief Constructor to initialize the class variables.
     */
    BloomFilter();

    /**
     * @brief Adds a key to the Bloom filter.
     * @param key The key to be added.
     */
    void add(const string& key);

    /**
     * @brief Checks if a key is present in the Bloom filter.
     * @param key The key to be checked.
     * @return True if the key is present; otherwise, false.
     */
    bool contains(string& key);

    /**
     * @brief Clears the Bloom filter.
     */
    void clear();
};

/**
 * @class Database
 * @brief Manages data storage and retrieval, supporting operations like compaction, flushing, and concurrency control.
 */
class Database {
protected:
    int flushid, compactid;                ///< IDs for flush and compaction semaphores.
    bool destroy;                          ///< Indicates whether the database is being destroyed.
    vector<vector<BloomFilter>> filters;   ///< Bloom filters for different tiers.
    ofstream wal;                          ///< Write-ahead log (WAL) file stream.
    map<string, string> memtable;          ///< In-memory key-value storage.
    vector<int> levels_main;               ///< Stores the number of data files in Tier_i.
    size_t mem_size;                       ///< Size of the in-memory table.
    vector<int> semids, wsemids, wcount, rcount, reader, writer, mtx; ///< Semaphore and thread control variables.
    bool flushrunning;                     ///< Flag indicating if a flush operation is in progress.
    thread compact_main_thread, flush_thread; ///< Threads for compaction and flushing operations.

    /**
     * @brief Searches for a key in the database.
     * @param tier The tier to search in.
     * @param index The index within the tier.
     * @param key The key to search for.
     * @param value The value associated with the key, if found.
     * @return True if the key is found; otherwise, false.
     */
    bool Find(int tier, int index, string& key, string& value);

    /**
     * @brief Performs a binary search within a file stream for a given key.
     * @param In The primary input file stream.
     * @param secondary An optional secondary file stream.
     * @param entries The number of entries in the stream.
     * @param key The key to search for.
     * @param value The value associated with the key, if found.
     * @return True if the key is found; otherwise, false.
     */
    bool binary_search(ifstream& In, ifstream& secondary, size_t entries, string& key, string& value);

    /**
     * @brief Main compaction process.
     */
    void compact_main();

    /**
     * @brief Flushes the in-memory table to SSTable storage.
     */
    void FLUSH();

    /**
     * @brief Constructor for the Database class.
     */
    Database();

    /**
     * @brief Destructor for the Database class.
     */
    ~Database();

    /**
     * @brief Renames files based on the nomenclature of the specified tier.
     * @param oldPath The current file path.
     * @param newPath The new file path.
     * @param tier The tier number.
     */
    void Rename(path& oldPath, path& newPath, int tier);

    /**
     * @brief Performs compaction on a specific tier.
     * @param tier The tier to compact.
     */
    void compact(int tier);

    void write_lock(int tier);
    void write_unlock(int tier);
    void read_lock(int tier);
    void read_unlock(int tier);
    void merge_lock(int tier);
    void merge_unlock(int tier);

    /**
     * @brief Retrieves the path to a specific tier directory. Creates it if it doesn't exist.
     * @param tier The tier number.
     * @param dir The resulting directory path.
     */
    void get_folder(int tier, path& dir);

    /**
     * @brief Initializes variables based on the data in a tier during database initialization.
     * @param tier The tier number.
     * @return True if initialization succeeds; otherwise, false.
     */
    bool initialize_folder(int tier);

    /**
     * @brief Initializes the in-memory table using the WAL.
     */
    void initialize_memtable();

    /**
     * @brief Appends a key-value pair to the WAL.
     * @param key The key to append.
     * @param value The value to append.
     */
    void append_to_WAL(string& key, string& value);

    /**
     * @brief Initializes the Bloom filters for a tier based on its data.
     * @param tier The tier number.
     * @param filter The Bloom filter vector to initialize.
     * @param dir The directory path of the tier.
     */
    void initialize_filter(int tier, vector<BloomFilter>& filter, path& dir);

    /**
     * @brief Pushes semaphore configurations for new tiers and threads.
     */
    void push_semaphores();
};

#endif
