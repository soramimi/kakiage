#ifndef STRTEMPLATE_H
#define STRTEMPLATE_H

#include <string>
#include <map>
#include <functional>
#include <vector>

class strtemplate {
public:
	bool html_mode = true;

	std::vector<std::map<std::string, std::string> *> defines;

	std::function<std::string (std::string const &name, std::string const &arg)> evaluator;
	std::function<std::string (std::string const &file)> includer;

	std::string generate(const std::string &source, const std::map<std::string, std::string> &map, int include_depth = 0);

};

#endif // STRTEMPLATE_H
