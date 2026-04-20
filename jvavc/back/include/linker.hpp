#ifndef LINKER_HPP
#define LINKER_HPP
#include "parser.hpp"
#include "encoder.hpp"
#include <string>
#include <vector>
#include <map>

class Linker {
public:
    bool addFile(const std::string &filename);
    bool link(const std::string &output);
    const std::string& getError() const { return error; }
private:
    struct FileUnit {
        std::string filename;
        std::vector<std::string> lines;
        Parser parser;
    };
    std::vector<FileUnit> units;
    std::map<std::string, __int128> globalEqu;
    std::string error;

    bool extractEqu(const std::vector<std::string> &lines, std::map<std::string, __int128> &out);
};

#endif
