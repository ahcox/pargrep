//
// Copyright Andrew Cox 2017.
// All rights reserved worldwide.
//

#include "regex_functions.h"


namespace pargrep {
    using namespace std;

    bool
    search(const string &s,
                 const regex &e,
                 regex_constants::match_flag_type flags)
    {
        const bool found = std::regex_search(s, e);
        return found;
    }
}