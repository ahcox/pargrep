/**
 * Parallel grep.
 * Copyright Andrew Helge Cox 2017.
 * All rights reserved worldwide.
 *
 * # ToDos #
 * See bottom of file.
 */
#include "pargrep.h"
#include <regex>
#include <thread>
#include <cassert>
#include <random>

namespace Pargrep
{
    using namespace std;

    constexpr bool DEBUG_CODE_ON            = false;

    using LineNumber = std::uint64_t;

    // See pargrep.h
    void grep_stream(istream &input, const string pattern, ostream &output, bool lineNumbers)
    {
      const regex toFind {pattern};

      string line;
      int lineNumber = 1;
      while(std::getline(input, line))
      {
        const bool found = regex_search(line, toFind);
        if(found)
        {
          if(lineNumbers)
          {
            output << lineNumber << ':'<< line << std::endl;
          } else {
            output << line << std::endl;
          }
        }
        ++lineNumber;
      }
    }
}

///@ToDo - Benchmark against grep using these locale options: http://www.inmotionhosting.com/support/website/ssh/speed-up-grep-searches-with-lc-all
///@ToDo - Add simple string matching without regex and compare to fgrep.
