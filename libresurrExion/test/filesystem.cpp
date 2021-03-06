//
//  filesystem.cpp - Filesystem tests
//  resurrExion
//
//  Created by Paul Ciarlo on 5 March 2019
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

#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include "../../config/fsconfig.hpp"
#include "../src/filesystem.hpp"

using github::paulyc::Loggable;
using namespace github::paulyc::resurrExion::exfat;

BOOST_AUTO_TEST_SUITE(FilesystemTestSuite)

BOOST_AUTO_TEST_CASE(TestStructs)
{
	Loggable l;

    l.logf(Loggable::INFO, "sizeof(struct fs_boot_region) == %d\n", sizeof(boot_region_t<512>));
    BOOST_STATIC_ASSERT(sizeof(boot_region_t<SectorSize>) % SectorSize == 0);
    l.logf(Loggable::INFO, "sizeof(struct fs_file_allocation_table) == %d\n", sizeof(file_allocation_table_t<SectorSize, SectorsPerCluster, NumSectors>));
    BOOST_STATIC_ASSERT(sizeof(file_allocation_table_t<SectorSize, SectorsPerCluster, NumSectors>) % 512 == 0);

	constexpr size_t fs_headers_size_bytes =
    sizeof(boot_region_t<SectorSize>) * 2 +
    sizeof(fat_region_t<SectorSize, SectorsPerCluster, ClustersInFat>);
	BOOST_ASSERT((fs_headers_size_bytes % SectorSize) == 0);
	constexpr size_t fs_headers_size_sectors = fs_headers_size_bytes / SectorSize;
	l.logf(Loggable::INFO, "ClustersInFat == %08x\n", ClustersInFat);
	l.logf(Loggable::INFO, "fs_headers_size_sectors == %08x\n", fs_headers_size_sectors);
	BOOST_ASSERT(fs_headers_size_sectors == ClusterHeapStartSector);
}

BOOST_AUTO_TEST_SUITE_END()
