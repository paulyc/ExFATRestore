//
//  entity.hpp - File/Directory Entity on Disk
//  resurrExion
//
//  Created by Paul Ciarlo on 9 March 2019.
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

#ifndef _github_paulyc_entity_hpp_
#define _github_paulyc_entity_hpp_

#include "exfat_structs.hpp"

#include <list>
#include <string>
#include <memory>

namespace github {
namespace paulyc {
namespace resurrExion {

class BaseEntity
{
public:
    BaseEntity(void *entry_start, uint8_t num_entries, std::shared_ptr<BaseEntity> parent, const std::string &name);
    ~BaseEntity() = default;

    exfat::metadata_entry_u *get_entity_start() const {
        return _fs_entries;
    }
    uint8_t get_num_entries() const {
        return _num_entries;
    }
    size_t get_file_info_size() const {
        return _num_entries * sizeof(exfat::metadata_entry_u);
    }
    uint32_t get_start_cluster() const {
        return (_fs_entries + 1)->stream_extension_entry.first_cluster;
    }
    uint64_t get_size() const {
        return (_fs_entries + 1)->stream_extension_entry.size;
    }
    std::string get_name() const {
        return _name;
    }
    std::shared_ptr<BaseEntity> get_parent() const {
        return _parent;
    }
    void set_parent(std::shared_ptr<BaseEntity> parent) {
        _parent = parent;
    }
protected:
    exfat::metadata_entry_u *_fs_entries;
    uint8_t _num_entries;
    std::shared_ptr<BaseEntity> _parent;
    std::string _name;
};

class FileEntity : public BaseEntity
{
public:
    FileEntity(void *entry_start, uint8_t num_entries, std::shared_ptr<BaseEntity> parent, const std::string &name) :
        BaseEntity(entry_start, num_entries, parent, name) {}

    bool is_contiguous() const {
        return (this->_fs_entries + 1)->stream_extension_entry.flags & exfat::CONTIGUOUS;
    }
};

class DirectoryEntity : public BaseEntity
{
public:
    DirectoryEntity(void *entry_start, int num_entries, std::shared_ptr<BaseEntity> parent, const std::string &name);

    void add_child(std::shared_ptr<BaseEntity> child) { _children.push_back(child); }
    const std::list<std::shared_ptr<BaseEntity>> &get_children() const { return _children; }

protected:
    std::list<std::shared_ptr<BaseEntity>> _children;
};

class RootDirectoryEntity : public DirectoryEntity
{
public:
    RootDirectoryEntity(void *entry_start) : DirectoryEntity(entry_start, 0, nullptr, "ROOT") {}
};

} /* namespace resurrExion */
} /* namespace paulyc */
} /* namespace github */

#endif /* _github_paulyc_entity_hpp_ */