//
//  quick.cpp
//  resurrExion
//
//  Created by Paul Ciarlo on 11/5/19.
//
//  Copyright (C) 2019 Paul Ciarlo <paul.ciarlo@gmail.com>.
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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/fcntl.h>
#include <unistd.h>

#ifndef O_RSYNC
#define O_RSYNC O_SYNC
#endif

#include <unordered_map>
#include <typeinfo>
#include <exception>
#include <system_error>
#include <locale>
#include <codecvt>
#include <vector>
#include <fstream>
#include <iostream>
#include <functional>
#include <regex>

#include "exfat_structs.hpp"

using namespace github::paulyc;
using namespace github::paulyc::resurrExion;

typedef uint8_t my_cluster[512*512];

namespace {
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> cvt;
}

std::string get_utf8_filename(exfat::file_directory_entry_t *fde, struct exfat::stream_extension_entry_t *sde)
{
    const size_t continuations = fde->continuations;
    const size_t namelen = sde->name_length;
    std::basic_string<char16_t> u16s;
    for (size_t c = 2; c <= continuations; ++c) {
        exfat::file_name_entry_t *n = (exfat::file_name_entry_t *)(((uint8_t*)fde) + c*32);
        if (n->type == exfat::FILE_NAME) {
            for (size_t i = 0; i < sizeof(n->name); ++i) {
                // I suspect that exfat doesnt always set this length properly
                if (n->name[i] == 0 || u16s.length() == namelen) {
                    return cvt.to_bytes(u16s);
                } else {
                    u16s.push_back((char16_t)n->name[i]);
                }
            }
        }
    }
    return cvt.to_bytes(u16s);
}

class Entity;

Entity * make_entity(std::streamoff offset, exfat::file_directory_entry_t *fde, const char *suggested_name);

class Entity{
public:
    Entity(std::streamoff offset, exfat::file_directory_entry_t *fde) :
        _offset(offset), _fde(fde), _parent(nullptr) {
            _streamext = (exfat::stream_extension_entry_t*)(fde+1);
            _nameent = (exfat::file_name_entry_t*)(fde+2);
        }

    virtual ~Entity() {}
    virtual std::streamoff get_offset() const { return _offset; }
    virtual std::string get_name() const { return _name; }
    virtual Entity * get_parent() const { return _parent; }
    virtual void set_parent(Entity *parent) { _parent = parent; }

    // defname is given if this fails
    virtual Entity * load_name(const std::string &suggested_name) {
        try {
            _name = get_utf8_filename(_fde, _streamext);
        } catch (const std::exception &ex) {
            fprintf(stderr, "get_utf8_filename failed with [%s]: %s\n", typeid(ex).name(), ex.what());
            fprintf(stderr, "using suggested name %s for entity %016lx\n", suggested_name.c_str(), get_offset());
            _name = suggested_name;
        }
        return this; //convenience
    }

public: // can't deal
    std::string _name;
    std::streamoff _offset;
    exfat::file_directory_entry_t *_fde;
    exfat::stream_extension_entry_t *_streamext;
    exfat::file_name_entry_t *_nameent;
    Entity *_parent;
};

class File:public Entity{
public:
    File(std::streamoff offset, exfat::file_directory_entry_t *fde) : Entity(offset, fde) {
        _contiguous = _streamext->flags & exfat::CONTIGUOUS;
    }
    virtual ~File() {}
    bool is_contiguous() const {
        return _contiguous;
    }
    bool _contiguous;
};

class Directory:public Entity{
public:
    Directory(std::streamoff offset, exfat::file_directory_entry_t *fde) : Entity(offset, fde) {
    }
    virtual ~Directory() {}
    virtual void add_child(Entity *child) {
        child->set_parent(this);
        this->children.push_back(child);
    }
    virtual const std::vector<Entity*>& get_children() const { return children; }
private:
    std::vector<Entity*> children;
};

Entity * make_entity(std::streamoff offset, exfat::file_directory_entry_t *fde, const std::string &suggested_name) {
    if (fde->attributes & exfat::DIRECTORY) {
        return (new Directory(offset, fde))->load_name(suggested_name);
    } else {
        return (new File(offset, fde))->load_name(suggested_name);
    }
}

using namespace github::paulyc::resurrExion;

constexpr static size_t SectorSize = 512;
constexpr static size_t SectorsPerCluster = 512;
constexpr static size_t NumSectors = 7813560247;
constexpr static size_t ClustersInFat = (NumSectors - 0x283D8) / 512;
constexpr static size_t PartitionStartSector = 0x64028;
constexpr static size_t ClusterHeapStartSector = 0x283D8; // relative to partition start
constexpr static size_t DiskSize = (NumSectors + PartitionStartSector) * SectorSize;

class FilesystemStub
{
public:
    FilesystemStub():
        _fd(-1),
        _mmap((uint8_t*)MAP_FAILED),
        _partition_start(nullptr),
        _partition_end(nullptr)
    {
        _invalid_file_name_characters = {'"', '*', '/', ':', '<', '>', '?', '\\', '|'};
        for (char16_t ch = 0; ch <= 0x001F; ++ch) {
            _invalid_file_name_characters.push_back(ch);
        }
    }

    void open(std::string devpath) {
        //const bool write_changes = false;
        //const int oflags = write_changes ? O_RDWR | O_DSYNC | O_RSYNC : O_RDONLY;
        _fd = ::open(devpath.c_str(), O_RDONLY | O_DSYNC | O_RSYNC);
        if (_fd == -1) {
            std::cerr << "failed to open device " << devpath << std::endl;
            throw std::system_error(std::error_code(errno, std::system_category()));
        }

        //const int mprot = write_changes ? PROT_READ | PROT_WRITE : PROT_READ;
        //const int mflags = write_changes ? MAP_SHARED : MAP_PRIVATE;
        // Darwin doesn't like this with DiskSize == 3.6TB. Not sure if that's why,
        // but only about 1G ends up actually getting mapped for my disk with ~270000 entities....
        _mmap = (uint8_t*)mmap(0, DiskSize, PROT_READ, MAP_PRIVATE, _fd, 0);
        if (_mmap == (uint8_t*)MAP_FAILED) {
            std::cerr << "error opening mmap" << std::endl;
            throw std::system_error(std::error_code(errno, std::system_category()));
        }

         _partition_start = _mmap + PartitionStartSector * SectorSize;
         _partition_end = _partition_start + (NumSectors + 1) * SectorSize;
    }

    void close() {
        _partition_end = nullptr;
        _partition_start = nullptr;

        if (_mmap != MAP_FAILED) {
            munmap(_mmap, DiskSize);
            _mmap = (uint8_t*)MAP_FAILED;
        }

        if (_fd != -1) {
            ::close(_fd);
            _fd = -1;
        }
    }

    ~FilesystemStub()
    {
        close();
    }

    void parseTextLog(const std::string &filename)
    {
        std::unordered_map<Entity*, Entity*> child_to_parent;
        std::unordered_map<Entity*, std::streamoff> entity_to_offset;

        std::regex fde("FDE ([0-9a-fA-F]{16}) (.*)");
        std::regex badsector("BAD_SECTOR ([0-9a-fA-F]{16})");
        std::ifstream logfile(filename);
        size_t count = 0;
        for (std::string line; std::getline(logfile, line); ++count) {
            std::smatch sm;
            if (count % 1000 == 0) {
                printf("count: %zu\n", count);
            }
            if (std::regex_match(line, sm, fde)) {
                std::streamoff offset;
                std::string filename;
                filename = sm[2];
                try {
                    offset = std::stol(sm[1], nullptr, 16);
                    Entity * ent = loadEntityOffset(offset, filename);
                    _entities_to_offsets[ent] = offset;
                    _offsets_to_entities[offset] = ent;
                } catch (std::exception &ex) {
                    std::cerr << "Writing file entry to binlog, got exception: [" << typeid(ex).name() << "] with msg: " << ex.what() << std::endl;
                    std::cerr << "Invalid textlog FDE line, skipping: " << line << std::endl;
                }
            } else if (std::regex_match(line, sm, badsector)) {
                std::streamoff offset;
                try {
                    offset = std::stol(sm[1], nullptr, 16);
                    std::cerr << "Noted bad sector at offset " << offset << std::endl;
                } catch (std::exception &ex) {
                    std::cerr << "Writing bad sector to binlog, got exception " << typeid(ex).name() << " with msg: " << ex.what() << std::endl;
                    std::cerr << "Invalid textlog BAD_SECTOR line, skipping: " << line << std::endl;
                }
            } else {
                std::cerr << "Unknown textlog line format: " << line << std::endl;
            }
        }
    }

    void log_results() {
        std::cerr << "find_orphans" << std::endl;
        FILE *orphanlog = fopen("orphan.log", "w");
        for (auto it = _offsets_to_entities.begin(); it != _offsets_to_entities.end(); ++it) {
            const std::streamoff offset = it->first;
            const Entity *ent = it->second;
            if (ent == nullptr) {
                fprintf(orphanlog, "NOENT %016lx\n", offset);
            } else {
                const Entity *parent = ent->get_parent();
                if (parent == nullptr) {
                    fprintf(orphanlog, "ORPHAN %016ld %s\n", offset, ent->get_name().c_str());
                } else {
                    if (typeid(*ent) == typeid(File)) {
                        const File *f = dynamic_cast<const File*>(ent);
                        if (f->is_contiguous()) {
                            fprintf(orphanlog, "FILE %016lx ", f->get_offset());
                        } else {
                            fprintf(orphanlog, "FRAGMENT %016lx ", f->get_offset());
                        }
                    } else {
                        const Directory *d = dynamic_cast<const Directory*>(ent);
                        fprintf(orphanlog, "DIRECTORY %016lx ", d->get_offset());
                    }
                    fprintf(orphanlog, " %016lx %s %s\n", parent->get_offset(), ent->get_name().c_str(), parent->get_name().c_str());
                }
            }
        }
        fclose(orphanlog);
    }

    Entity * loadEntityOffset(std::streamoff entity_offset, const std::string &suggested_name) {
        //fprintf(stderr, "loadEntityOffset[%016lld]\n", entity_offset);
        return loadEntity(entity_offset, _mmap + entity_offset, suggested_name);
    }

    Entity * loadEntity(std::streamoff offset, uint8_t *entity_start, const std::string &suggested_name)
    {
        exfat::file_directory_entry_t *fde = (exfat::file_directory_entry_t*)(entity_start);
        exfat::stream_extension_entry_t *streamext = (exfat::stream_extension_entry_t*)(entity_start+32);
        exfat::file_name_entry_t *n = (exfat::file_name_entry_t*)(entity_start+64);

        if (fde->type != exfat::FILE_DIR_ENTRY || streamext->type != exfat::STREAM_EXTENSION || n->type != exfat::FILE_NAME) {
            fprintf(stderr, "bad number of continuations at offset %016lx\n", entity_start - _mmap);
            return nullptr;
        }

        const int continuations = fde->continuations;
        if (continuations < 2 || continuations > 18) {
            fprintf(stderr, "bad number of continuations at offset %016lx\n", entity_start - _mmap);
            return nullptr;
        }

        int i;
        uint16_t chksum = 0;

        for (i = 0; i < 32; ++i) {
            if (i != 2 && i != 3) {
                chksum = ((chksum << 15) | (chksum >> 1)) + entity_start[i];
            }
        }

        for (; i < (continuations+1) * 32; ++i) {
            chksum = ((chksum << 15) | (chksum >> 1)) + entity_start[i];
        }

        if (chksum != fde->checksum) {
            fprintf(stderr, "bad file entry checksum at offset %016lx\n", offset);
            return nullptr;
        }

        return make_entity(offset, fde, suggested_name);
    }

    void load_directory(std::streamoff offset, Directory *d) {
        const uint32_t start_cluster = d->_streamext->first_cluster;
        const uint8_t num_entries = d->_fde->continuations + 1;
        exfat::metadata_entry_u *dirent = nullptr;
        if (start_cluster == 0) {
            // directory children immediately follow
            dirent = (exfat::metadata_entry_u *)(d->_fde + num_entries);
            offset += sizeof(d->_fde) * num_entries;
        } else {
            fprintf(stderr, "SKETCH!\n");
            //throw std::exception();
            const size_t cluster_heap_start_byte_offset = 0x283D8 * 512;
            my_cluster *c = (my_cluster*)(_mmap + cluster_heap_start_byte_offset);
            dirent = (exfat::metadata_entry_u*)(c+start_cluster);
            offset = (uint8_t*)dirent - _mmap;
        }

        const uint8_t num_secondary_entries = dirent->primary_directory_entry.secondary_count;
        for (uint8_t secondary_entry = 0; secondary_entry < num_secondary_entries; ++secondary_entry) {
            Entity *child = make_entity(offset, &dirent->file_directory_entry, std::string());
            d->add_child(child);
            dirent += child->_fde->continuations + 1;
            offset += sizeof(exfat::metadata_entry_u) * (child->_fde->continuations + 1);
        }
    }

    int      _fd;
    uint8_t *_mmap;
    uint8_t *_partition_start;
    uint8_t *_partition_end;
    std::vector<char16_t> _invalid_file_name_characters;
    std::unordered_map<Entity*, std::streamoff> _entities_to_offsets;
    std::unordered_map<std::streamoff, Entity*> _offsets_to_entities;
};

int main(int argc, char *argv[]) {
    FilesystemStub stub;
    stub.open(argc > 1 ? argv[1] : "/dev/sdb");
    stub.parseTextLog(argc > 2 ? argv[2] : "recovery.log");
    stub.log_results();
    stub.close();
    return 0;
}
