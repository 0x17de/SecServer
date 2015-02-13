#include "iniparser.h"

#include <fstream>
#include <sstream>


using namespace std;


IniParser::IniParser(const std::string &fileName) {
    ifstream in(fileName);
    if (!in) return;

    string sectionName = "";
    sections.insert(make_pair("", IniSection()));
    IniSection* section = &sections.at("");

    string line;
    while (!getline(in, line).eof()) {
        if (line[0] == '[') {
            sectionName = line.substr(1, line.length()-1);
            auto sectionIt = sections.find(sectionName);
            if (sectionIt == end(sections)) {
                sections.insert(make_pair(sectionName, IniSection()));
                section = &sections.at(sectionName);
            } else {
                section = &sectionIt->second;
            }
        }
        string key,value;
        istringstream is(line);
        getline(is, key, '=');
        getline(is, value);
        section->data.insert(make_pair(key, value));
    }
}
