//
// Created by Andrew Cox on 04/10/2017.
//
#include "pargrep.h"
#include <fstream>

using namespace std;

int main()
{
    using namespace Pargrep;

    std::string filename = "/tmp/pargrep.in";
    std::string pattern = "[qz]";

    ifstream in(filename);

    grep_stream(in, pattern, std::cout, true);
    in.close();
    cout.flush();

    ///@ToDo The moment the output file is closed, kill the process. There is no need for clean shutdown.
    return 0;
}