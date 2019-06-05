//
//  journal.cpp - journal for filesystem changes capable of rollbacks
//  ExFATRestore
//
//  Created by Paul Ciarlo on 19 March 2019.
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

#include "journal.hpp"

#include <functional>
#include <algorithm>
#include <memory>
#include <string.h>

namespace io {
namespace github {
namespace paulyc {

TransactionJournal::TransactionJournal(uint8_t *base_ptr, const char *journal_filename) :
    _base_ptr(base_ptr),
    _journal_file(journal_filename, std::ios_base::in | std::ios_base::out | std::ios_base::binary)
{
}

TransactionJournal::~TransactionJournal()
{
}

void TransactionJournal::add_write(size_t ofs_write, size_t ofs_read, size_t byte_count)
{
    _changeset.push_back(std::make_unique<Diff>(_base_ptr + ofs_write, _base_ptr + ofs_read, byte_count));
}

void TransactionJournal::commit()
{
    for (std::deque<std::unique_ptr<Diff>>::iterator diffit = _changeset.begin();
         diffit != _changeset.end();
         ++diffit)
    {
        (*diffit)->apply();
    }
}

void TransactionJournal::rollback()
{
    for (std::deque<std::unique_ptr<Diff>>::reverse_iterator diffit = _changeset.rbegin();
         diffit != _changeset.rend();
         ++diffit)
    {
        (*diffit)->unapply();
    }
}

void TransactionJournal::serialize(std::basic_string<uint8_t> &serialized) const
{
}

void TransactionJournal::deserialize(const std::basic_string<uint8_t> &serialized)
{
}

TransactionJournal::Diff::Diff(uint8_t *to, const uint8_t *const from, size_t byte_count) :
    _commit_addr(to),
    _byte_count(byte_count),
    _commit_data(std::make_unique<uint8_t[]>(byte_count)),
    _rollback_data(std::make_unique<uint8_t[]>(byte_count))
{
    memcpy(_rollback_data.get(), to, byte_count);
    memcpy(_commit_data.get(), from, byte_count);
}

TransactionJournal::Diff::~Diff()
{
}

void TransactionJournal::Diff::apply()
{
    memcpy(_commit_addr, _commit_data.get(), _byte_count);
}

void TransactionJournal::Diff::unapply()
{
    memcpy(_commit_addr, _rollback_data.get(), _byte_count);
}

} /* namespace paulyc */
} /* namespace github */
} /* namespace io */