//
//  database.cpp
//  resurrExion
//
//  Created by Paul Ciarlo on 2 July 2020
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

#include "database.hpp"
#include <unordered_set>

// "root", "root", "resurrex", 0, "/run/mysqld/mysqld.sock"
Database::Database(const std::string &user, const std::string &pass, const std::string &sock, const std::string &db) {
    mariadb::account_ref a = mariadb::account::create("", user.c_str(), pass.c_str(), db.c_str(), 0, sock.c_str());
    a->set_auto_commit(true);
    _conn = mariadb::connection::create(a);
    if (!_conn->connect()) {
        _conn = nullptr;
        throw new std::runtime_error("failed to connect");
    }
}
Database::~Database() {
    if (_conn != nullptr) {
        _conn->disconnect();
    }
}

void Database::rescue_music() {
    //dir = "/home/paulyc/elements";
    FilesystemStub stub;
    stub.open("/dev/sdb");
    mariadb::statement_ref stmt = _conn->create_statement("update file set is_copied_off = 1 where file.entity_offset = ?");
    std::function<void(File*)> onFileCopied = [&stmt](File *f){stmt->set_unsigned64(0, f->_offset); stmt->execute();};
    //stub.parseTextLog("recovery.log");
    mariadb::result_set_ref rs = _conn->query("select distinct(parent_directory_offset) from file where name like '%flac' or name like '%mp3' or name like '%wav' or name like '%m4a' or name like '%aiff' or name like '%aif'");
    while (rs->next()) {
        mariadb::transaction_ref txref = _conn->create_transaction();
        mariadb::u64 pdo = rs->get_unsigned64(0);
        Directory * d = reinterpret_cast<Directory*>(stub.loadEntityOffset(pdo, "temp"));
        if (d == nullptr) {
            continue;
        }
        //d->resolve_children(_conn);
        std::string name = std::string(d->_name.c_str());
        stub.dump_directory(d, name, onFileCopied);
        txref->commit();
    }
    //rs = _conn->query("SELECT id, parent_directory_offset FROM file");
}

void Database::rescue_photos() {
    FilesystemStub stub;
    stub.open("/dev/sdb");

    mariadb::statement_ref stmt = _conn->create_statement("update file set is_copied_off = 1 where file.entry_offset = ?");
    mariadb::statement_ref dir_stmt = _conn->create_statement("select entry_offset, parent_directory_offset from directory where entry_offset = ?");
    uint64_t count = 0;
    std::function<void(File*)> onFileCopied = [&stmt, &count](File *f){
        stmt->set_unsigned64(0, f->_offset);
        stmt->execute();
        if (++count % 1000 == 0) {
            std::cout << "counted " << count << std::endl;
        }
    };
    //stub.parseTextLog("recovery.log");
    //mariadb::result_set_ref rs = _conn->query("select distinct(parent_directory_offset) from file where name like '%jpg' or name like '%jpeg' or name like '%png' or name like '%mov' or name like '%mp4' or name like '%avi' or name like '%wmv' or name like '%mkv'");

    mariadb::result_set_ref orphan_dirs = _conn->query("select entry_offset from directory where parent_directory_offset = 0 order by entry_offset asc");

    while (orphan_dirs->next()) {
        mariadb::u64 pdo = orphan_dirs->get_unsigned64(0);
        Directory * d = reinterpret_cast<Directory*>(stub.loadEntityOffset(pdo, "temp"));
        if (d == nullptr) {
            std::cerr << "failed to load directory pdo = " << pdo << std::endl;
        }
        stub.dump_directory(d, std::string(d->_name.c_str()), onFileCopied);
        delete d;
    }
}

/*
void Database::migrate_to_sql_ids() {
    mariadb::result_set_ref rs = _conn->query("SELECT id, parent_directory_offset FROM file");
    while (rs->next()) {

    }
}
*/
