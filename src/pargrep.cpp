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
    using std::cerr;
    using std::endl;
    using std::istream;
    using std::ostream;
    using std::vector;

    constexpr bool LOGGING_DIAGNOSTIC_ON    = true;
    constexpr bool DEBUG_CODE_ON            = false;
    constexpr bool DEBUG_CODE_SLEEPS_ON     = DEBUG_CODE_ON && false;
    constexpr bool DEBUG_CODE_DELETE_ARRAYS = DEBUG_CODE_ON && false;

    constexpr LineNumber END_OF_LINES = LineNumber(0) - 1;

    std::mutex outputMutex;

    // See pargrep.h
    void grep_stream(istream &input, const string pattern, ostream &output, bool lineNumbers)
    {
      const regex toFind {pattern};

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

    /**
     * A reusable bundle of per-line data.
     * These are passed from input thread to worker and writer threads and then
     * recirculated back to reader thread to minimise allocations.
     */
    struct Line {
        Line (const LineNumber n, const LineNumber s) :
                number(n),
                skipped(s),
                matched(false)
        {}
        /**
         * Get ready to reuse an old Line object for a new line.
         * @param number The number of this new line.
         * @param skipped The count of completely empty lines that have been
         */
        void reset(const LineNumber number, const LineNumber skipped)
        {
            this->number = number;

            this->text.clear();
            this->skipped = skipped;
            this->matched = false;
        }
        LineNumber number;
        LineNumber skipped;
        std::string text;
        bool matched = false;
    };

    Line* createLine(const LineNumber n, const LineNumber s)
    {
        ///@ToDo Support aligned allocation when compiler incs to C++17 / Clang 5.
        return new Line(n, s);
    }

    /**
     * A set of pointers to Lines which are owned _elsewhere_ (**if at all**).
     */
    class LineSet
    {
    public:

        /**
         * Add a pointer to a Line to the back of the set.
         * The Line passed may not be used by the caller after the call returns.
         * @param line A reference to a pointer to a line. This will be null on return
         * to force the caller to segfault if it uses it.
         */
        void push(Line*& line)
        {
            std::lock_guard<std::mutex> lock(m_);
            {
                s_.push_back(line);
            }
            line = nullptr;
        }

        /**
         * Retrieve all Line objects in the set under a single lock.
         * If the set is empty, the function will return immediately and outLines will be empty.
         * @param outLines A buffer to hold popped Lines. Contents will be overwritten not appended-to.
         */
        void popAll(std::vector<Line*>& outLines)
        {
            std::lock_guard<std::mutex> lock(m_);
            {
                outLines.swap(s_);
                // Start with a completely fresh buffer:
                if constexpr (DEBUG_CODE_DELETE_ARRAYS){
                    vector<Line*> clean;
                    s_ = clean;
                }
                s_.clear();

            }
        }

        /**
         * Test whether there are any Lines in the set. In concurrent use, this is of course only a hint.
         * @return True if the set has some Lines in it, else false.
         */
        bool empty() const {
            bool e = false;
            std::lock_guard<std::mutex> lock(m_);
            {
                e = s_.empty();
            }
            return e;
        }

    private:
        mutable std::mutex m_;
        std::vector<Line*> s_;
    };

   /**
    * A set of pointers to Lines which are owned _elsewhere_ (**if at all**).
    * Popping Lines blocks and puts the calling thread into a waiting state if none
    * are available.
    */
    class BlockingLineSet
    {
    public:
       /**
        * Add a pointer to a Line to the back of the set.
        * The Line passed may not be used by the caller after the call returns.
        * Any blocked threads waiting for data to be available in the set will be woken.
        * @param line A reference to a pointer to a line. This will be null on return
        * to force the caller to segfault if it uses it.
        */
        void push(Line*& line)
        {
            std::unique_lock<std::mutex> lock(m_);
            {
                s_.push_back(line);
            }
            c_.notify_all();
            //c_.notify_one();
            // Force callers to segfault if they use the thing they just threw away:
            line = nullptr;
        }

        /**
         * Retrieve all Line objects in the set under a single lock.
         * Only returns when there is data available, otherwise it waits for some.
         * @param outLines A buffer to hold popped Lines. Contents will be overwritten not appended-to.
         */
        void popAll(std::vector<Line*>& inOutLines)
        {
            std::unique_lock<std::mutex> lock(m_);
            {
                while(s_.empty()) {
                    c_.wait(lock);
                }

                inOutLines.swap(s_);
                if(DEBUG_CODE_DELETE_ARRAYS){
                    vector<Line*> clean;
                    s_ = clean;
                }
                s_.clear();
            }
        }

        /**
         * Test whether there are any Lines in the set. In concurrent use, this is of course only a hint.
         * @return True if the set has some Lines in it, else false.
         */
        bool empty() const {
            bool e = false;
            std::unique_lock<std::mutex> lock(m_);
            {
                e = s_.empty();
            }
            return e;
        }

    private:
        mutable std::mutex m_;
        std::condition_variable c_;
        std::vector<Line*> s_;
    };

    class GrepThreadState
    {
    public:
        GrepThreadState(const std::regex& regex, BlockingLineSet& results, unsigned workerId) :
            regex(regex), results(results), workerId(workerId)
        {}
        const std::regex& regex;
        BlockingLineSet input;
        // Wired up to the output thread for in-order retirement:
        BlockingLineSet& results;
        unsigned workerId = 0;
    };

    void grepThreadFunc(GrepThreadState* state)
    {
        if constexpr (LOGGING_DIAGNOSTIC_ON) {
            std::lock_guard<std::mutex> lock(outputMutex);
            cerr << "Starting a Grep Thread " << state->workerId << " with state pointer: " << (uint64_t) state << endl;
        }
        const std::regex& regex = state->regex;
        BlockingLineSet& input = state->input;
        BlockingLineSet& results = state->results;
        std::vector<Line*> inputBuffer;

        bool running = true;
        while(running)
        {
            input.popAll(inputBuffer);
            for(auto line : inputBuffer)
            {
                if(line->skipped != END_OF_LINES)
                {
                    const bool found = regex_search(line->text, regex);
                    line->matched = found;
                    results.push(line);
                } else {
                    running = false;
                    if constexpr(LOGGING_DIAGNOSTIC_ON){
                        std::lock_guard<std::mutex> lock(outputMutex);
                        cerr << "Grep Thread # " << state->workerId << " quiting." << endl;
                    }
                }
            }
            inputBuffer.clear();
        }
    }

    class WriterThreadState
    {
    public:
        WriterThreadState(ostream& output, LineSet& recycler, bool outputLineNumbers = false) :
            output(output),
            recycler(recycler),
            outputLineNumbers(outputLineNumbers)
        {}
        // Lines to be reordered into original order and output if they match:
        BlockingLineSet input;
        // A text stream to write to:
        ostream& output;
        // Wired up to the main thread to reuse for future lines:
        LineSet& recycler;
        // whether to prefix lines with line numbers:
        bool outputLineNumbers = false;
    };

    void writerThreadFunc(WriterThreadState* const state)
    {
        if constexpr(LOGGING_DIAGNOSTIC_ON){
            std::lock_guard<std::mutex> lock(outputMutex);
            cerr << "Writer thread started with state at address: " << (uint64_t) state << endl;
        }
        ostream& output = state->output;
        LineSet& recycler = state->recycler;
        // A place to grab lines in a batch while entering a mutex just once:
        vector<Line*> inputBuffer;
        // A place to sort lines into their original order, oldest/lowest lines at the front:
        vector<Line*> reorderBuffer;
        // A high-tide mark showing how far line processing has reached:
        LineNumber lastOutput = 0;
        const bool outputLineNumbers = state->outputLineNumbers;

        bool running = true;
        while(running) {
            if constexpr (DEBUG_CODE_SLEEPS_ON) { std::this_thread::sleep_for(std::chrono::milliseconds(100)); }

            // Grab a batch of lines, waiting if there are none:
            assert(inputBuffer.empty());
            state->input.popAll(inputBuffer);
            assert(inputBuffer.size() > 0UL);

            // Sort the new lines into the ordered buffer, oldest, lowest at the back:
            reorderBuffer.reserve(reorderBuffer.size() + inputBuffer.size());
            reorderBuffer.insert(reorderBuffer.end(), inputBuffer.begin(), inputBuffer.end());
            inputBuffer.clear();

            std::sort(reorderBuffer.begin(), reorderBuffer.end(), [](const Line *l, const Line *r) -> bool {
                assert(l);
                assert(r);
                const auto ln = l->number;
                const auto rn = r->number;
                bool result = false;
                if (ln > rn) {
                    result = true;
                }
                return result;
            });

            unsigned numLinesProcessed = 0;
            for(auto it = reorderBuffer.rbegin(), end = reorderBuffer.rend(); it != end; ++it)
            {
                Line* line = *it;
                assert(lastOutput < line->number); ///@Note This fired when the loop above hadn't. [TEMP]
                // Look out for thread quit signal:
                if(line->skipped == END_OF_LINES) {

                    if constexpr (LOGGING_DIAGNOSTIC_ON) {
                        std::lock_guard<std::mutex> lock(outputMutex);
                        cerr << "Writer thread quiting at line # " << line->number << endl;
                    }
                    running = false;
                    line->skipped = 0;
                    line->matched = false;
                }
                else if(lastOutput + line->skipped + 1 == line->number)
                {
                    if (line->matched) {
                        if (outputLineNumbers) {
                            output << line->number << ": ";
                        }
                        output << line->text << endl;
                    }

                    lastOutput = line->number;
                    if constexpr (DEBUG_CODE_ON) { *it = 0; } // < Null the array entry.
                    // Send the line back to the main thread to be reused:
                    ++numLinesProcessed;
                    recycler.push(line); ///< You may never access line again on this thread until you give it a new value.

                } else {
                    // We have a gap in the order we are waiting to fill:
                    break;
                }
            }
            reorderBuffer.resize(reorderBuffer.size() - numLinesProcessed);
        }
        // Flush and close the output on this thread since we have its data structures in cache:
        output.flush();
        ///@ToDo: - caller passes a policy which we invoke here. It could close the output file and do an immediate process exit without cleanup.
    }

    // See pargrep.h
    void pargrep_stream_par1(istream& input, const string pattern, ostream& output, bool lineNumbers)
    {
        constexpr unsigned MAX_LINES_IN_FLIGHT = 256;
        const regex toFind {pattern};

        // Writer thread:
        // Returned lines after output by writer thread:
        LineSet recycled;
        std::vector<Line*> recycledBuffer;
        WriterThreadState writerState {
                output,
                recycled,
                lineNumbers
        };
        std::thread writerThread(writerThreadFunc, &writerState);

        LineNumber lineNumber = 0;
        LineNumber skipped = 0;
        unsigned linesCreated = 0;


        ///@ToDo Lower priority of current thread so background threads starve it from generating new work as long as there is existing work to do in background.

        Line *line = nullptr;
        while(true)
        {
            ++lineNumber;

            // Get a Line struct:
            if(!skipped) {
                unsigned yieldCount = 0;
                get_a_line:
                if (recycledBuffer.empty()) {
                    if(DEBUG_CODE_DELETE_ARRAYS){
                        vector<Line*> temp;
                        recycledBuffer = temp;
                    }
                    recycled.popAll(recycledBuffer);
                }
                if (recycledBuffer.empty()) {
                    if(linesCreated < MAX_LINES_IN_FLIGHT) {
                        line = createLine(lineNumber, skipped);
                        ++linesCreated;
                        if constexpr(LOGGING_DIAGNOSTIC_ON)
                        {
                            std::lock_guard<std::mutex> l(outputMutex);
                            cerr << "Lines created: " << linesCreated << endl;
                        }
                    } else {
                        std::this_thread::yield();
                        if(yieldCount % 32 == 0){
                            if constexpr(LOGGING_DIAGNOSTIC_ON)
                            {
                                std::lock_guard<std::mutex> l(outputMutex);
                                cerr << '.';
                            }
                        }
                        ++yieldCount;
                        goto get_a_line;
                    }
                } else {
                    line = recycledBuffer.back();
                    recycledBuffer.resize(recycledBuffer.size() - 1);
                }
            }
            assert(line);
            line->reset(lineNumber, skipped);
            std::string& lineBuffer = line->text;
            if(!std::getline(input, lineBuffer)){
                line->number = lineNumber;
                line->skipped = END_OF_LINES;
                writerState.input.push(line);
                break;
            }

            if(lineBuffer.length() < 1) {
                ++skipped;
                continue;
            }

            if constexpr (false && LOGGING_DIAGNOSTIC_ON) {
                std::lock_guard<std::mutex> lock(outputMutex);
                cerr << "Push #" << line->number << " (" << line << "." << endl;
            }
            const bool found = regex_search(lineBuffer, toFind);
            line->matched = found;
            assert(lineNumber == line->number);
            assert(skipped == line->skipped);
            writerState.input.push(line);
            if constexpr (DEBUG_CODE_SLEEPS_ON) { if(lineNumber % 16 == 0) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }}

            skipped = 0;
        }
        writerThread.join();
    }

    // See pargrep.h
    void pargrep_stream_par2(istream& input, const string pattern, ostream& output, bool lineNumbers)
    {
        Line endSentinel = Line(0, END_OF_LINES);
        const regex toFind {pattern};

        // Worker threads:
        const unsigned numThreads = std::thread::hardware_concurrency();
        if constexpr(LOGGING_DIAGNOSTIC_ON)
        {
            std::lock_guard<std::mutex> l(outputMutex);
            cerr << "Hardware concurrency: " << numThreads << endl;
        }
        // Threads and thread states are pointed-to to avoid false sharing of cachelines.
        std::vector<std::thread*> workers;
        workers.reserve(numThreads);
        std::vector<GrepThreadState*> taskStates;
        taskStates.reserve(numThreads);

        // Writer thread:
        // Returned lines after output by writer thread:
        LineSet recycled;
        std::vector<Line*> recycledBuffer;
        WriterThreadState writerState {
                output,
                recycled,
                lineNumbers
        };
        std::thread writerThread(writerThreadFunc, &writerState);

        std::default_random_engine generator;
        std::uniform_int_distribution<unsigned> distribution(0, numThreads - 1);

        bool launchedThreads = false;
        LineNumber lineNumber = 0;
        LineNumber skipped = 0;

        ///@ToDo Lower priority of current thread so background threads starve it from generating new work as long as there is existing work to do in background.

        Line *line = nullptr;
        while(true)
        {
            ++lineNumber;

            // Get a Line struct:
            if(!skipped) {
                if (recycledBuffer.empty()) {
                    recycled.popAll(recycledBuffer);
                }
                if (recycledBuffer.empty()) {
                    line = createLine(lineNumber, skipped);
                } else {
                    line = recycledBuffer.back();
                    recycledBuffer.resize(recycledBuffer.size() - 1);
                }
            }
            line->reset(lineNumber, skipped);
            std::string& lineBuffer = line->text;
            if(!std::getline(input, lineBuffer)){
                line->number = lineNumber;
                line->skipped = END_OF_LINES;
                writerState.input.push(line);

                // Tell worker threads to stop:
                for(auto workerState : taskStates)
                {
                    auto* p = &endSentinel;
                    workerState->input.push(p);
                }

                break;
            }

            if(lineBuffer.length() < 1) {
                ++skipped;
                continue;
            }

            unsigned threadIndex = 0;
            // Build a background thread on demand to optimise for short inputs:
            if(!launchedThreads)
            {
                threadIndex = workers.size();
                taskStates.push_back(new GrepThreadState(toFind, writerState.input, threadIndex));
                workers.push_back(new std::thread(grepThreadFunc, taskStates.back()));

                if(threadIndex + 1 >= numThreads)
                {
                    launchedThreads = true;
                }
            }
            // Pick an existing thread to send the line to at random to avoid repeating patterns in input line length causing asymetric thread workloads:
            else
            {
                threadIndex = distribution(generator);
            }

            GrepThreadState* workerState = taskStates[threadIndex];
            workerState->input.push(line);

            skipped = 0;
        }
        // Wait for all background work to quit:
        writerThread.join();
        ///@ToDo: can do an immediate exit here assuming writer won't quit until all work from worker threads is output.
        for(auto thread : workers)
        {
            thread->join();
        }
    }
}

///@ToDo - Limit the number of Line structs in flight for the worker threads version (say 32 * num threads).
///@ToDo - Special case matches for zero length lines ("^$", ".*", "^.*", "^", "$", etc.) or this skipping empty lines optimisation is a bug. [On first empty line, apply regex on reader thread: if it matches, send all empty lines to writer directly as matches without running any regex, if it doesn't: do as we do now: skip them completely.]
///@ToDo - Wrap the cerr usage in a locking mechanism.
///@ToDo - Docopt command line parser: https://github.com/docopt/docopt.cpp
///@ToDo - Analyse file size and avoid spawning threads if a file is small.
///@ToDo - Benchmark against grep using these locale options: http://www.inmotionhosting.com/support/website/ssh/speed-up-grep-searches-with-lc-all
///@ToDo - Add simple string matching without regex and compare to fgrep.
///@ToDo - Aligned allocation of threads and thread state structs to avoid false sharing.     constexpr bool USE_ALIGNED_ALLOC        = true;
///@ToDo - Aligned allocation of Line structures.

