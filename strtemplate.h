#ifndef STRTEMPLATE_H
#define STRTEMPLATE_H

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

class strtemplate {
private:
	bool html_mode_ = true;
public:

	bool is_html_mode() const
	{
		return html_mode_;
	}
	void set_html_mode(bool value)
	{
		html_mode_ = value;
	}

	std::vector<std::map<std::string, std::string> *> defines;

	std::function<std::optional<std::string> (std::string const &name, std::string const &arg)> evaluator;
	std::function<std::string (std::string const &file)> includer;

	std::string generate(const std::string &source, const std::map<std::string, std::string> &map, int include_depth = 0);

};

#endif // STRTEMPLATE_H
