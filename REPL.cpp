#include"REPL.h"
/**
 * @brief Retrieves the value for a given key from the database.
 * 
 * This function checks if the given key exists in the Bloom filter and
 * searches for the key in the memtable. If not found in the memtable, it
 * searches through the levels of the database (using Bloom filters and the
 * `Find` function). If the key is found and is not marked as a TOMBSTONE, 
 * the corresponding value is returned.
 * 
 * @param key The key to search for.
 * @param value The value associated with the key, to be returned if found.
 * @return true if the key is found and is not marked as a TOMBSTONE, false otherwise.
 */
bool REPL::GET(string &key, string &value) {
    read_lock(0);  // Locks for reading.

    // Check if the key is in the Bloom filter for the memtable.
    if (filters[0][0].contains(key)) {
        auto pointer = memtable.find(key);  // Find the key in the memtable.
        
        if (pointer != memtable.end()) {
            if (pointer->second != TOMBSTONE) {  // If the value is not a TOMBSTONE.
                read_unlock(0);
                value = pointer->second;
                return true;
            } 
            else{
                read_unlock(0);
                return false;
            }
        }
    }

    read_unlock(0);

    // Check all levels of the database if the key is not in the memtable.
    for (int i = 1; i < levels_main.size(); ++i) {
        read_lock(i);

        for (int j = levels_main[i]; j > 0; --j) {
            if (filters[i][j - 1].contains(key)) {
                auto res = Find(i, j, key, value);  // Calls Find function.

                if (res && value == TOMBSTONE) {  // If the key is found but marked as deleted.
                    read_unlock(i);
                    return false;
                } 
                else if (res) {  // If the key is found and not a TOMBSTONE.
                    read_unlock(i);
                    return true;
                }
                continue;
            }
        }
        
        read_unlock(i);
    }
    
    return false;  // Key not found.
}

/**
 * @brief Sets the value for a given key in the memtable and appends it to the WAL.
 * 
 * This function adds a key-value pair to the memtable and appends the same
 * to the Write-Ahead Log (WAL). If the memtable exceeds a certain size,
 * it triggers a flush operation. Additionally, it ensures that the key-value
 * pair does not exceed the maximum allowed size.
 * 
 * @param key The key to be set.
 * @param value The value to be associated with the key.
 * @return true if the operation is successful, false if the key-value pair exceeds the size limit.
 */
bool REPL::SET(string &key, string &value) {
    write_lock(0);  // Locks for writing.

    // Waits if the FLUSH operation is running.
    while (flushrunning) {
        write_unlock(0);
        T(flushid);  // Waits until FLUSH is not running.
        write_lock(0);
    }

    // Check if the key-value pair exceeds the maximum allowed size.
    if (key.length() + value.length() >= MAX) {
        write_unlock(0);
        return false;
    }

    append_to_WAL(key, value);  // Append the key-value pair to the WAL.
    memtable[key] = value;      // Insert the key-value pair into the memtable.
    mem_size += key.length() + value.length();  // Update the memtable size.

    filters[0][0].add(key);  // Add the key to the Bloom filter.

    // If memtable size exceeds the threshold, trigger flush.
    if (mem_size >= MAX) {
        flushrunning = 1;
        V(flushid);
        merge_unlock(0);
    }

    write_unlock(0);  // Unlocks the write lock.
    return true;
}

/**
 * @brief Marks a key as deleted by setting it to TOMBSTONE.
 * 
 * This function is a shortcut for calling `SET` with the TOMBSTONE value,
 * marking the given key as deleted in the database. The value associated with
 * the key will be set to TOMBSTONE, indicating that the key has been logically deleted.
 * 
 * @param key The key to be deleted.
 * @return true if the delete operation is successful, false otherwise.
 */
bool REPL::DELETE(string &key) {
    return SET(key, TOMBSTONE);  // Mark the key as deleted by setting it to TOMBSTONE.
}
