//
// Copyright Andrew Cox 2017.
// All rights reserved worldwide.
//

#ifndef PARGREP_PARGREP_H
#define PARGREP_PARGREP_H
#include <iostream>

namespace pargrep {

    /**
     * Grep for an expression on a single stream and output results on an outstream
     * provided.
     * A simple single threaded reference implementation.
     *
     * @param input A stream to scan line by line for regex matches.
     * @param pattern The regex to scan for.
     * @param output A stream to output matching lines to.
     * @param lineNumbers If true, matching lines are prefixed with their line
     * numbers, else they are ouput exactly as read.
     */
    void grep_stream(std::istream &input, const std::string pattern, std::ostream &output, bool lineNumbers = true);
}

#endif //PARGREP_PARGREP_H
