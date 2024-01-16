#ifndef STRTEMPLATE_H
#define STRTEMPLATE_H

#include <string>
#include <map>
#include <functional>
#include <vector>

class strtemplate {
public:

	std::vector<std::map<std::string, std::string> *> defines;
	std::function<std::string (std::string const &, std::string const &, std::string const &)> evaluate;

	std::string generate(const std::string &source, const std::map<std::string, std::string> &map);

};

#endif // STRTEMPLATE_H
