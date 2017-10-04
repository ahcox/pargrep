//
// Created by Andrew Cox on 13/09/2017.
//


namespace old_version_002 {
using namespace std;

namespace Pargrep
{
    using std::string;
    using std::istream;
    using std::ifstream;
    using std::ostream;
    using std::regex;
    using std::regex_search;

    void randomScrap()
    {
        if constexpr (DEBUG_CODE_ON)
        {
            LineNumber lastLineNo = LineNumber(0) - 1;
            for(unsigned i = 0; i < reorderBuffer.size() - numLinesProcessed; ++i)
            {
                const Line* line = reorderBuffer[i];
                const LineNumber lineNumber = line->number;
                assert(lineNumber < lastLineNo);
                lastLineNo = lineNumber;
            }
        }


        if constexpr (DEBUG_CODE_ON)
        {
                if constexpr(false &&LOGGING_DIAGNOSTIC_ON){
                    std::lock_guard<std::mutex> lock(outputMutex);

                    for (auto line : inputBuffer) {
                        assert(line);
                        cerr << "Pop #" << line->number << " (" << line << ")" << endl;
                    }
                }
                for (auto line : inputBuffer) {
                    assert(line);
                    assert(line->number > lastOutput);
                }
                for (auto line : reorderBuffer) {
                    assert(line);
                    assert(line->number > lastOutput);
                }
        }

        // Check the order:
        if constexpr (DEBUG_CODE_ON)
                {
                        LineNumber lastLineNo = LineNumber(0) - 1;
                for (auto line : reorderBuffer) {
                    const LineNumber lineNumber = line->number;
                    assert(lineNumber < lastLineNo);
                    lastLineNo = lineNumber;
                }
                }

        // Check no doubles for line numbers:
        if constexpr (DEBUG_CODE_ON)
                {
                        std::unordered_set<LineNumber> reorderedLineNumbers;
                for(auto line : reorderBuffer)
                {
                    const LineNumber lineNumber = line->number;
                    assert(reorderedLineNumbers.count(lineNumber) == 0);
                    reorderedLineNumbers.insert(lineNumber);
                    assert(reorderedLineNumbers.count(lineNumber) == 1);
                }
                }




    }

/**
 * Grep for an expression on a single stream and output results on an outstream
 * provided.
 * A simple single threaded reference implementation.
 **/
    void pargrep_stream(istream& input, const string pattern, ostream& output, bool lineNumbers = true)
    {
        const regex toFind {pattern};
        //std::smatch matches;

        string line;
        int lineNumber = 1;
        while(std::getline(input, line))
        {
            //const bool found = regex_search(line, matches, toFind);
            const bool found = regex_search(line, toFind);
            if(found)
            {
                //std::cerr << "Found " << pattern << " on line: " << lineNumber << std::endl;
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
    struct Line {
        Line (const uint64_t n, std::string&& b, const uint32_t s, const bool m) :
                number(n),
                text(b),
                skipped(s),
                matched(m)
        {}
        void reset(const uint64_t number, std::string& buffer, const uint32_t skipped, const bool matched)
        {
            this->number = number;
            this->text.swap(buffer);
            this->skipped = skipped;
            this->matched = matched;
        }
        uint64_t number;
        std::string text;
        uint32_t skipped;
        bool matched = false;
    };

    /**
     * A queue of pointers to Lines which are owned _elsewhere_.
     */
    class LineQueue
    {
    public:

        void push(Line& line)
        {
            std::lock_guard<std::mutex> lock(m_);
            {
                q_.emplace_back(&line);
            }
        }

        void popAll(std::vector<Line*>& inOutLines)
        {
            std::lock_guard<std::mutex> lock(m_);
            {
                inOutLines.swap(q_);
                q_.resize(0);
            }
        }

        bool empty() const {
            return q_.empty();
        }

        void deleteAll()
        {
            std::lock_guard<std::mutex> lock(m_);
            {
                for (auto ptr : q_) {
                    delete ptr;
                }
                q_.resize(0);
            }
        }
    private:
        std::mutex m_;
        // We may yield and spin instead of waiting: std::condition_variable c_;
        std::vector<Line*> q_;
    };

    class GrepTask
    {
    public:
        GrepTask(const std::regex& regex, LineQueue& results) : regex(regex), results(results) {}
        void operator()()
        {
            cerr << "GrepTask operator()()" << endl;
        }
        LineQueue work;
    private:
        std::regex regex;
        LineQueue& results;
    };

    class GrepThreadState
    {
    public:
        std::regex regex;
        LineQueue input;
        LineQueue& results;
    };

    void grepThreadFunc(GrepThreadState* state)
    {
        cerr << (uint64_t) state << endl;
    }


    void pargrep_stream_par1(istream& input, const string pattern, ostream& output, bool lineNumbers = true)
    {
        const regex toFind {pattern};
        const unsigned numThreads = std::thread::hardware_concurrency();
        std::vector<std::thread> workers;
        workers.reserve(numThreads);
        std::vector<GrepTask*> tasks;
        tasks.reserve(numThreads);
        LineQueue results;
        //std::smatch matches;

        bool launchedThreads = false;
        string line;
        uint64_t lineNumber = 0;
        uint32_t skipped = 0;

        ///@ToDo Lower priority of current thread so background threads starve it from generating new work as long as there is existing work to do in background.
        while(std::getline(input, line))
        {
            ++lineNumber;
            if(line.length() < 1) {
                ++skipped;
                continue;
            }
            if(line.length() < 5) {
                cerr << line.length() << "<" << line << ">" << endl;
            }

            unsigned threadIndex = 0;
            // Build a background thread on demand to optimise for short inputs:
            if(!launchedThreads)
            {
                GrepTask* task = new GrepTask(toFind, results);
                (*task)();
                //auto* thread = new std::thread(*task);
                auto* thread = new std::thread(grepThreadFunc, nullptr);
                //tasks.push_back(new GrepTask(toFind, results));
                const auto newThreadIndex = tasks.size();
                //workers.emplace_back(tasks[newThreadIndex]);

                if(newThreadIndex + 1 >= numThreads)
                {
                    launchedThreads = true;
                }
            }
                // Pick an existing thread to send the line to at random to avoid repeating patterns in input line length causing asymetric thread workloads:
            else
            {

            }
            //const bool found = regex_search(line, matches, toFind);
            const bool found = regex_search(line, toFind);
            if(found)
            {
                //std::cerr << "Found " << pattern << " on line: " << lineNumber << std::endl;
                if(lineNumbers)
                {
                    output << lineNumber << ':'<< line << std::endl;
                } else {
                    output << line << std::endl;
                }
            }
        }
    }

}

int main()
{
    using namespace Pargrep;

    std::string filename = "/tmp/pargrep.in";
    std::string pattern = "[qz]";

    ifstream in(filename);

    pargrep_stream(in, pattern, std::cout, true);

    return 0;
}

}
namespace old_version_001 {

        using namespace std;

        namespace Pargrep
        {
            using std::string;
            using std::istream;
            using std::ifstream;
            using std::ostream;
            using std::regex;
            using std::regex_search;

/**
 * Grep for an expression on a single stream and output results on an outstream
 * provided.
 * A simple single threaded reference implementation.
 **/
            void pargrep_stream(istream& input, const string pattern, ostream& output, bool lineNumbers = true)
            {
                const regex toFind {pattern};
                //std::smatch matches;

                string line;
                int lineNumber = 1;
                while(std::getline(input, line))
                {
                    //const bool found = regex_search(line, matches, toFind);
                    const bool found = regex_search(line, toFind);
                    if(found)
                    {
                        //std::cerr << "Found " << pattern << " on line: " << lineNumber << std::endl;
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
            struct Line {
                Line (const uint64_t n, std::string&& b, const bool m) :
                        number(n),
                        text(b),
                        matched(m)
                {}
                void reset(const uint64_t number, std::string& buffer, const bool matched)
                {
                    this->number = number;
                    this->text.swap(buffer);
                    this->matched = matched;
                }
                uint64_t number;
                std::string text;
                bool matched = false;
            };

            class LineQueue
            {
            public:
                void push(Line& line)
                {
                    //q_.emplace_back(line.number, line.text, line.matched);

                }
            private:
                std::condition_variable c_;
                std::vector<Line> q_;
            };


            void pargrep_stream_par1(istream& input, const string pattern, ostream& output, bool lineNumbers = true)
            {
                const regex toFind {pattern};
                const unsigned numThreads = std::thread::hardware_concurrency();
                std::vector<std::thread> workers;
                workers.reserve(numThreads);
                //std::smatch matches;

                bool launchedThreads = false;
                string line;
                int lineNumber = 1;
                ///@ToDo Lower priority of current thread so background threads starve it from generating new work as long as there is existing work to do in background.
                while(std::getline(input, line))
                {
                    // Build a background thread on demand to optimise for short inputs:
                    if(!launchedThreads)
                    {

                        std::thread thread { [toFind] () {

                        }};

                        if(workers.size() >= numThreads)
                        {
                            launchedThreads = true;
                        }
                    }
                        // Pick an existing thread to send the line to at random to avoid repeating patterns in input line length causing asymetric thread workloads:
                    else
                    {

                    }
                    //const bool found = regex_search(line, matches, toFind);
                    const bool found = regex_search(line, toFind);
                    if(found)
                    {
                        //std::cerr << "Found " << pattern << " on line: " << lineNumber << std::endl;
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

        int main()
        {
            using namespace Pargrep;

            std::string filename = "/tmp/pargrep.in";
            std::string pattern = "[qz]";

            ifstream in(filename);

            pargrep_stream(in, pattern, std::cout);

            return 0;
        }
};