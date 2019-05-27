/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information

#pragma once

#include <json/json.h>

namespace credb
{

json::Document parse_document_file(const std::string &filename);
}
