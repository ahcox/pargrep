//
// Copyright 2017 Andrew Cox.
// All rights reserved worldwide.
//
// Specialisations of template functions from the standard library to get a clean line in profiles.
//
#ifndef PARGREP_REGEX_FUNCTIONS_H
#define PARGREP_REGEX_FUNCTIONS_H

#include <regex>
#include <string>

namespace pargrep {

bool search(const std::string& s,
             const std::regex& e,
             std::regex_constants::match_flag_type flags = std::regex_constants::match_default);
}
#endif //PARGREP_REGEX_FUNCTIONS_H
