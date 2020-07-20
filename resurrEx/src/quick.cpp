//
//  quick.cpp
//  resurrExion
//
//  Created by Paul Ciarlo on 11/5/19
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
#include "quick.hpp"

#if 1
int main(int argc, char *argv[]) {
    Database d("root", "root", "/run/mysqld/mysqld.sock", "resurrex");
    d.rescue_photos();
    return 0;
}
#else

int main(int argc, char *argv[]) {
    FilesystemStub stub;
    stub.open("/dev/sdb");
#ifdef PARSE
    if (argc < 2) {
        fprintf(stderr, "Youll need to enter a directory offset\n");
	return 1;
    }

    int ret = 0;get_children()
    RootDirectory = std::make_shared<Directory>();


    if (stub.parseTextLog("recovery.log")) {
        stub.adopt_orphans();
        stub.log_sql("logfile.sql");
    } else {
        std::cerr << "parseTextLog failed or tree failed integrity check" << std::endl;
        ret = 1;
    }
#else DUMP
    Directory * d = reinterpret_cast<Directory*>(stub.loadEntityOffset(std::stoul(std::string(argv[1])), "Ethernaut"));
    stub.dump_directory(d, ".");
#endif
    stub.close();
    return ret;
}
#endif


#if 0
int fix_orphans_method(const std::vector<std::string> &args) {
    if (args.size() < 2) {
        return -1;
    }
    FilesystemStub stub;
    stub.open(args[0]);
    stub.parseTextLog(args[1]);
    stub.adopt_orphans();
    stub.dirty_writeback();
    stub.close();
    return 0;
}

int orphans_method(const std::vector<std::string> &args) {
    if (args.size() < 3) {
        return -1;
    }
    FilesystemStub stub;
    stub.open(args[0]);
    RecoveryLog<> log;
    //stub.parseTextLog(args[1]);
    //stub.log_results(args[2].c_str());
    stub.log_sql_results("logfile.sql");
    stub.close();
    return 0;
}


int main(int argc, char *argv[]) {
    return orphans_method

    if (argc == 1) {
        fprintf(stderr, "forcing defaults\n");
        return orphans_method({"/dev/sdb", "recovery.log", "orphan.log"});
    } else if (argc == 2) {
        return orphans_method({argv[1], "recovery.log", "orphan.log"});
    } else if (argc == 3) {
        return orphans_method({argv[1], argv[2], "orphan.log"});
    } else if (argc == 4) {
        return orphans_method({argv[1], argv[2], argv[3]});
    } else if (argc != 4) {
        fprintf(stderr, "usage: %s <device> <recovery_log> <orphan_log>\n\n", argv[0]);
        return -1;
    }
}
#endif
