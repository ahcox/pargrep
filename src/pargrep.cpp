/**
 * Parallel grep.
 * Copyright Andrew Helge Cox 2017.
 * All rights reserved worldwide.
 *
 * # ToDos #
 * See bottom of file.
 */
#include "pargrep.h"
#include "regex_functions.h"
#include <thread>
#include <cassert>
#include <random>
#include <cstdint>

namespace pargrep
{
    using std::regex;
    using std::string;
    using std::endl;
    using std::istream;
    using std::ostream;

    using LineNumber = std::uint64_t;

    // See pargrep.h
    void grep_stream(istream &input, const string pattern, ostream &output, bool lineNumbers)
    {
        const regex toFind {pattern};
        //cerr << pattern << endl;

        string line;
        int lineNumber = 1;
        while(std::getline(input, line))
        {
            //std::cerr << "LINE: \"" << line << "\"" << std::endl;
            const bool found = pargrep::search(line, toFind);
            if(found)
            {
                if(lineNumbers)
                {
                    output << lineNumber << ':'<< line << std::endl;
                } else {
                    output << line << std::endl;
                }
                // std::cerr << "MATCH: " << line << std::endl;
            }
            ++lineNumber;
        }
    }
}

///@ToDo - Benchmark against grep using these locale options: http://www.inmotionhosting.com/support/website/ssh/speed-up-grep-searches-with-lc-all
///@ToDo - Add simple string matching without regex and compare to fgrep.
