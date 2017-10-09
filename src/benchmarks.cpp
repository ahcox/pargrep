//
// Copyright Andrew Cox 2017
// All rights reserved worldwide
//
#include "pargrep.h"
#include <benchmark/benchmark.h>
#include <regex>
#include <random>
#include <fstream>

namespace benchmark_helpers
{
    std::string RandomString(unsigned len) {
        std::default_random_engine generator;
        std::uniform_int_distribution<char> capitals('A', 'Z');
        std::uniform_int_distribution<char> lower('a', 'z');
        std::uniform_int_distribution<char> numbers('0', '9');
        std::uniform_int_distribution<char> choose(0, 2);


        std::string s;
        s.reserve(len);
        for (unsigned i = 0; i < len; ++i) {
            const auto choice = choose(generator);
            s.push_back(choice == 0 ? capitals(generator) : choice == 1 ? lower(generator) : numbers(generator));
        }
        return s;
    }

    static void RegexCreation(benchmark::State &state, const std::string& pattern) {
        while (state.KeepRunning()) {
            const std::regex aRegex{pattern};
            benchmark::DoNotOptimize(aRegex);
        }
    }

    static void RegexMatch(benchmark::State &state, const std::string &pattern) {
        const std::regex aRegex{pattern};
        const std::string s = RandomString(state.range(0));

        unsigned found = 0;
        while (state.KeepRunning()) {
            found += regex_search(s, aRegex);
        }
        benchmark::DoNotOptimize(found);
    }

    static void RegexMatchRepeated(benchmark::State &state, const std::string pattern) {
        auto dups = state.range(1);
        std::string dupedPattern;
        dupedPattern.reserve(pattern.size() * dups);
        for (int dup = 0; dup < dups; ++dup) {
            dupedPattern.append(pattern);
        }
        RegexMatch(state, dupedPattern);
    }

    /**
     * Make a text file which can be read in and grepped over by tests.
     */
    static std::string CreateTempFile(const std::vector<std::string>& prefixes, unsigned minLen, unsigned maxLen, unsigned numLines, const std::string& filename)
    {
        using namespace std;
        auto fullPath = "/tmp/" + filename; ///@Todo - Make portable to MS Windows.
        ofstream file(fullPath, ios_base::trunc);

        std::default_random_engine gen;
        std::uniform_int_distribution<unsigned> prefixSelect(0, prefixes.size() - 1);
        std::uniform_int_distribution<unsigned> lenSelect(minLen, maxLen);

        for(unsigned i = 0; i < numLines; ++i)
        {
            file << prefixes[prefixSelect(gen)] << RandomString(lenSelect(gen)) << endl;
        }

        file.close();
        return fullPath;
    }
}

namespace benchmarks
{
    using namespace benchmark_helpers;
#if 1
    static void BM_RegexCreationEmpty(benchmark::State &state) {
        RegexCreation(state, "");
    }
    BENCHMARK(BM_RegexCreationEmpty);

    static void BM_RegexCreationAnything(benchmark::State &state) {
        RegexCreation(state, ".");
    }
    BENCHMARK(BM_RegexCreationAnything);

    static void BM_RegexCreationModerate(benchmark::State &state) {
        RegexCreation(state, ".*\\(BM_RegexCreationEmpty.*:\\|\\tret.*\\)$");
    }
    BENCHMARK(BM_RegexCreationModerate);

    static void BM_RegexCreationModerate2(benchmark::State &state) {
        RegexCreation(state, "^\\[ERROR\\] *: *[[:digit:]]+.*[a-z]+.*[A-Z]+.*[[:digit:]]$");
    }
    BENCHMARK(BM_RegexCreationModerate2);

    static void BM_RegexMatchAnything(benchmark::State &state) {
        RegexMatch(state, ".");
    }
    // Arg is string length to match on:
    BENCHMARK(BM_RegexMatchAnything)->Arg(64)->Arg(512);

    static void BM_RegexMatchDigits(benchmark::State &state) {
        RegexMatchRepeated(state, "[[:digit:]]");
    }
    // Args are {string length, number of repetitions of match pattern}:
    BENCHMARK(BM_RegexMatchDigits)->Args({512, 2})->Args({512, 3})->Args({512, 4})->Args({512, 5});
#endif

    // Benchmark of simple grep over a file, writing to a second file:
    static void BM_RegexGrep(benchmark::State &state) {
        using namespace std;

        vector<string> prefixes {
                "[DEBUG]: ",
                "[WARNING]: ",
                "[INFO]: ",
                "[ERROR]: "
        };
        const auto numLines = state.range(0);
        const string fullPath = CreateTempFile(prefixes, 10, 120, numLines, "BM_RegexGrep_input.log");

        // Find errors that start and end with digits (the rest of the pattern is just to increase complexity):
        const string pattern {"^\\[ERROR\\] *: *[[:digit:]]+.*[a-z]+.*[A-Z]+.*[[:digit:]]$"};

        while (state.KeepRunning())
        {
            ifstream in(fullPath);
            ofstream out("/tmp/pargrep_benchmark_out.log", ios_base::trunc);
            pargrep::grep_stream(in, pattern, out, true);
            in.close();
            out.close();
        }
    }
#if 1
    BENCHMARK(BM_RegexGrep)->Unit(benchmark::kMillisecond)->Repetitions(3)->ReportAggregatesOnly(true)->Arg(100)->Arg(1000)->Arg(2000)->Arg(3000)->ComputeStatistics("max", [](const std::vector<double>& v) -> double {
        return *(std::max_element(std::begin(v), std::end(v)));
    })->ComputeStatistics("min", [](const std::vector<double>& v) -> double {
        return *(std::min_element(std::begin(v), std::end(v)));
    });
#endif
}

BENCHMARK_MAIN();
