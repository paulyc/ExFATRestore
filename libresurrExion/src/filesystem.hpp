//
//  filesystem.hpp
//  resurrExion
//
//  Created by Paul Ciarlo on 2/11/19
//
//  Copyright (C) 2020 Paul Ciarlo <paul.ciarlo@gmail.com>
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//  SOFTWARE.
//

#ifndef _github_paulyc_filesystem_hpp_
#define _github_paulyc_filesystem_hpp_

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <variant>
#include <memory>
#include <fstream>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <locale>
#include <codecvt>
#include <list>
#include <vector>
#include <tuple>

#include "exception.hpp"
#include "recoverylog.hpp"
#include "types.hpp"

#ifndef O_RSYNC
#define O_RSYNC O_SYNC
#endif

namespace github {
namespace paulyc {
namespace resurrExion {

class BaseEntity;
std::string get_utf8_filename(exfat::file_directory_entry_t *fde, struct exfat::stream_extension_entry_t *sde);

template <size_t SectorSize, size_t SectorsPerCluster, sectorofs_t NumSectors>
class ExFATFilesystem : public Loggable
{
public:
    class restore_error : public std::runtime_error {
    public:
        explicit restore_error(const std::string &msg) : std::runtime_error(msg) {}
    };

    ExFATFilesystem(const char *devname, byteofs_t devsize, byteofs_t partition_first_sector, bool write_changes);
    ~ExFATFilesystem();

    std::shared_ptr<BaseEntity> loadEntityOffset(byteofs_t entity_offset, std::shared_ptr<BaseEntity> parent);

    std::shared_ptr<BaseEntity> loadEntity(uint8_t *entity_start, std::shared_ptr<BaseEntity> parent);

    void loadDirectory(std::shared_ptr<DirectoryEntity> de);

    void init_metadata();

    void write_metadata();

    void load_directory_tree(const std::string &textlogfilename);

    void find_orphans(std::function<void(byteofs_t offset, std::shared_ptr<BaseEntity> ent)> fun);

    // TODO take out this trash
    void restore_all_files(const std::string &restore_dir_name, const std::string &textlogfilename);

    // todo take out this trash
    // todo split out the binlog logic
    /**
     * binlog format:
     * 64-bit unsigned int, disk offset of byte in record or sector
     * 32-bit int, number of bytes in record, OR -1 if bad sector
     * variable bytes equal to number of bytes in record
     */
    void textLogToBinLog(
        const std::string &textlogfilename,
        const std::string &binlogfilename);

    void scanWriteToLog();

private:
    static constexpr size_t ClustersInFat = (NumSectors - 0x283D8) / 512;

    int      _fd;
    uint8_t *_mmap;
    size_t   _devsize;
    bool     _write_changes;
    uint8_t *_partition_start;
    uint8_t *_partition_end;

    // not part of actual partition, to be copied over later after being initialized
    exfat::boot_region_t<SectorSize> _boot_region;
    exfat::file_allocation_table_t<SectorSize, SectorsPerCluster, ClustersInFat> _fat;
    exfat::allocation_bitmap_table_t<ClustersInFat> _allocation_bitmap;
    exfat::upcase_table_t<SectorSize, 256> _upcase_table;
    exfat::root_directory_t<SectorSize, SectorsPerCluster> _root_directory;

    // pointer to the start of the actual mmap()ed partition
    exfat::filesystem_t<SectorSize, SectorsPerCluster, NumSectors> *_fs; // actual mmaped filesystem

    std::unordered_map<exfat::metadata_entry_u*, std::shared_ptr<BaseEntity>> _offset_to_entity_mapping;
    std::unordered_set<std::shared_ptr<BaseEntity>> _orphan_entities;
    std::unique_ptr<RootDirectoryEntity> _root_directory_entity;
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> _cvt;

    std::vector<char16_t> _invalid_file_name_characters;
};

} /* namespace resurrExion */
} /* namespace paulyc */
} /* namespace github */

#include "filesystem.cpp"

#endif /* _github_paulyc_filesystem_hpp_ */
