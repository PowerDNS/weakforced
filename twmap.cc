#include "twmap.hh"
#include <iostream>
#include <unistd.h>

TWStatsTypeMap g_field_types{};

std::mutex dbMap_mutx;
std::map<std::string, TWStringStatsDBWrapper> dbMap;
