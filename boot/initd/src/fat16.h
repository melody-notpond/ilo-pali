// #ifndef FAT16_H
// #define FAT16_H

// #include <stddef.h>
// #include <stdint.h>

// typedef uint16_t fat_entry_t;

// typedef union {
//     struct __attribute__((__packed__)) {
//         char name[8];
//         char ext[3];
//         uint8_t attributes;

//         uint8_t reserved;

//         uint8_t creation_time_tenths;
//         uint16_t creation_time;
//         uint16_t creation_date;

//         uint16_t last_access_date;

//         uint16_t first_cluster_high;

//         uint16_t write_time;
//         uint16_t write_date;

//         uint16_t first_cluster_low;

//         uint32_t file_size;
//     } file;

//     struct __attribute__((__packed__)) {
//         uint8_t order;
//         uint16_t name1[5];
//         uint8_t attributes;

//         uint8_t reserved1;

//         uint8_t checksum;

//         uint16_t name2[6];

//         uint16_t reserved2;

//         uint16_t name3[2];
//     } long_name;
// } fat_root_dir_entry_t;

// typedef struct {
//     uint16_t bytes_per_sector;
//     uint8_t sectors_per_cluster;
//     uint32_t cluster_count;
//     uint16_t root_entry_count;

//     void* boot_sector;
//     fat_entry_t* fat;
//     fat_root_dir_entry_t* root_dir;
//     void* data;
// } fat16_fs_t;

// // verify_initrd(void*, void*) -> fat16_fs_t
// // Verifies an initrd image as a FAT16 file system.
// fat16_fs_t verify_initrd(void* start, void* end);

// // find_file_in_root_directory(fat16_fs_t*, char*) -> fat_root_dir_entry_t*
// // Finds a file in the root directory. Note that the kernel does not have the ability to traverse files in fat file systems because it only needs to spawn initd, which is in the root directory.
// fat_root_dir_entry_t* find_file_in_root_directory(fat16_fs_t* fs, char* name);

// // get_fat_cluster_data(fat16_fs_t*, uint32_t) -> void*
// // Returns a pointer to the data in the cluster.
// void* get_fat_cluster_data(fat16_fs_t* fs, uint32_t cluster_id);

// // get_next_cluster(fat16_fs_t*, uint32_t) -> uint32_t
// // Returns the next cluster id if available, or 0 otherwise.
// uint32_t get_next_cluster(fat16_fs_t* fs, uint32_t cluster_id);

// // read_file_full(fat16_fs_t*, char*, size_t*) -> void*
// // Reads a file into a buffer, storing the size in the given buffer. Returns NULL on failure.
// void* read_file_full(fat16_fs_t* fs, char* name, size_t* size_ptr);

// #endif /* FAT16_H */
