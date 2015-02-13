#ifndef INIPARSER_H
#define INIPARSER_H


#include <string>
#include <map>


class IniSection {
public:
    std::map<std::string,std::string> data;
};

class IniParser {
public:
    std::map<std::string,IniSection> sections;

    IniParser(const std::string& fileName);
};


#endif // INIPARSER_H