#include<bits/stdc++.h>
#include"database.h"
#ifndef yu
#define yu
/**
 * @class REPL
 * @brief A class derived from Database that implements basic REPL operations (GET, SET, DELETE) for the key-value store.
 *
 * This class extends the Database class and provides methods to interact with the database 
 * through a REPL (Read-Eval-Print Loop). The methods allow retrieving, setting, and deleting key-value pairs 
 * from the database.
 */
class REPL : public Database {
public:
    /**
     * @brief Retrieves the value associated with a given key.
     * 
     * This function looks for the key in the memtable's BloomFilter and then searches for it in the 
     * memtable or disk if necessary. If the key is found and not marked as deleted (TOMBSTONE), 
     * the corresponding value is returned.
     * 
     * @param key The key for which the value needs to be retrieved.
     * @param value The value corresponding to the given key (output parameter).
     * 
     * @return True if the key was found and the value is valid, false otherwise.
     */
    bool GET(string &key, string &value);

    /**
     * @brief Sets the value for a given key.
     * 
     * This function stores the key-value pair in the memtable and appends it to the Write-Ahead Log (WAL).
     * If the memtable's size exceeds the threshold, a flush operation is triggered to persist the data on disk.
     * 
     * @param key The key to be inserted into the database.
     * @param value The value associated with the key.
     * 
     * @return True if the key-value pair was successfully inserted, false otherwise (e.g., if the total size is too large).
     */
    bool SET(string &key, string &value);

    /**
     * @brief Marks a key as deleted by setting its value to TOMBSTONE.
     * 
     * This function is used to delete a key from the database by setting the associated value to a special 
     * "TOMBSTONE" value. This does not immediately remove the key from the database but marks it for deletion.
     * 
     * @param key The key to be marked as deleted.
     * 
     * @return True if the operation was successful, false otherwise.
     */
    bool DELETE(string &key);
};
#endif
