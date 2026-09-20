#pragma once
#include "market.hpp"
namespace storage {
bool init();
bool load(bool host, bm::Market &);
bool save(bool host, bm::Market &);
} // namespace storage
