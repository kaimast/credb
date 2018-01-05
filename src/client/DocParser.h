#pragma once

#include <json/json.h>

namespace credb
{

json::Document parse_document_file(const std::string &filename);
}
