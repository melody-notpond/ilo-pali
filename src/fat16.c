#include <stdbool.h>
#include <stddef.h>

#include "console.h"
#include "fat16.h"

#define ATTR_READ_ONLY      0x01
#define ATTR_HIDDEN         0x02
#define ATTR_SYSTEM         0x04
#define ATTR_VOLUME_ID      0x08
#define ATTR_DIRECTORY      0x10
#define ATTR_ARCHIVE        0x20
#define ATTR_LONG_NAME      (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

#define SIGNATURE           0xaa55
#define FAT12_LIMIT         4085 
#define FAT16_LIMIT         65525

// verify_initrd(void*, void*) -> fat16_fs_t
// Verifies an initrd image as a FAT16 file system.
fat16_fs_t verify_initrd(void* start, void* end) {
    uint16_t signature = *(uint16_t*) (start + 0x1fe);
    if (signature != SIGNATURE) {
        console_printf("[verify_initrd] invalid fat16 signature %x\n", signature);
        return (fat16_fs_t) { 0 };
    }

    uint16_t bytes_per_sector = *(uint16_t*) (start + 11);
    uint8_t sectors_per_cluster = *(uint8_t*) (start + 13);
    uint16_t reserved_sector_count = *(uint16_t*) (start + 14);
    uint8_t number_of_fats = *(uint8_t*) (start + 16);
    uint16_t root_entry_count = *(uint16_t*) (start + 17);
    uint32_t total_sector_count = *(uint16_t*) (start + 19);
    if (total_sector_count == 0)
        total_sector_count = *(uint32_t*) (start + 32);
    uint16_t fat_size = *(uint16_t*) (start + 22);

    if (fat_size == 0 || total_sector_count == 0) {
        console_printf("[verify_initrd] one of fat size or total sector count is 0\n");
        return (fat16_fs_t) { 0 };
    }

    unsigned int root_sectors_count = (root_entry_count * 32 + (bytes_per_sector - 1)) / bytes_per_sector;
    unsigned int data_sectors = total_sector_count - (reserved_sector_count + number_of_fats * fat_size + root_sectors_count);
    unsigned int cluster_count = data_sectors / sectors_per_cluster;

    if (cluster_count < FAT12_LIMIT || cluster_count >= FAT16_LIMIT) {
        console_printf("[verify_initrd] fat file system is not FAT16\n");
        return (fat16_fs_t) { 0 };
    }

    fat16_fs_t fat = {
        .bytes_per_sector = bytes_per_sector,
        .sectors_per_cluster = sectors_per_cluster,
        .cluster_count = cluster_count,
        .root_entry_count = root_entry_count,

        .boot_sector = start,
        .fat = start + reserved_sector_count * bytes_per_sector,
    };
    fat.root_dir = (fat_root_dir_entry_t*) (fat.fat + number_of_fats * fat_size * bytes_per_sector / 2);
    fat.data = fat.root_dir + root_entry_count;

    void* fat_end = fat.data + cluster_count * sectors_per_cluster * bytes_per_sector;
    if (fat_end != end) {
        console_printf("[verify_initrd] fat end %p does not equal initrd end %p", fat_end, end);
        return (fat16_fs_t) { 0 };
    }

    return fat;
}

// find_file_in_root_directory(fat16_fs_t*, char*) -> fat_root_dir_entry_t*
// Finds a file in the root directory. Note that the kernel does not have the ability to traverse files in fat file systems because it only needs to spawn initd, which is in the root directory.
fat_root_dir_entry_t* find_file_in_root_directory(fat16_fs_t* fs, char* name) {
    for (uint16_t i = 0; i < fs->root_entry_count; i++) {
        if ((fs->root_dir[i].long_name.attributes & ATTR_LONG_NAME) == ATTR_LONG_NAME) {
            uint16_t j = i;
            size_t k = 0;
            bool equals = true;
            bool stop = false;

            while (true) {
                // Some assumptions: 1) the initrd image is not corrupt, and 2) the file names are not unicode
                // Because none of the initrd files are unicode
                // But it should probably check for corruption
                for (size_t l = 0; l < sizeof(fs->root_dir[j].long_name.name1) / sizeof(uint16_t); l++, k++) {
                    if (fs->root_dir[j].long_name.name1[l] != name[k]) {
                        equals = false;
                        break;
                    } else if (name[k] == '\0') {
                        stop = true;
                        break;
                    }
                }

                if (!equals || stop)
                    break;

                for (size_t l = 0; l < sizeof(fs->root_dir[j].long_name.name2) / sizeof(uint16_t); l++, k++) {
                    if (fs->root_dir[j].long_name.name2[l] != name[k]) {
                        equals = false;
                        break;
                    } else if (name[k] == '\0') {
                        stop = true;
                        break;
                    }
                }

                if (!equals || stop)
                    break;

                for (size_t l = 0; l < sizeof(fs->root_dir[j].long_name.name3) / sizeof(uint16_t); l++, k++) {
                    if (fs->root_dir[j].long_name.name3[l] != name[k]) {
                        equals = false;
                        break;
                    } else if (name[k] == '\0') {
                        stop = true;
                        break;
                    }
                }

                if (!equals || stop || (fs->root_dir[j].long_name.order & 0x40) != 0)
                    break;
                j++;
            }

            if (equals)
                return fs->root_dir + i + 1;

            i++;
        }
    }

    return NULL;
}

// get_fat_cluster_data(fat16_fs_t*, uint32_t) -> void*
// Returns a pointer to the data in the cluster.
void* get_fat_cluster_data(fat16_fs_t* fs, uint32_t cluster_id) {
    if (cluster_id > fs->cluster_count || cluster_id < 2)
        return NULL;
    return fs->data + (cluster_id - 2) * fs->sectors_per_cluster * fs->bytes_per_sector;
}

// get_next_cluster(fat16_fs_t*, uint32_t) -> uint32_t
// Returns the next cluster id if available, or 0 otherwise.
uint32_t get_next_cluster(fat16_fs_t* fs, uint32_t cluster_id) {
    if (cluster_id > fs->cluster_count || cluster_id < 2)
        return 0;

    uint32_t next = fs->fat[cluster_id];
    if (next > fs->cluster_count + 2)
        return 0;
    return next;
}
