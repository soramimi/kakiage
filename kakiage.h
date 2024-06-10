#ifndef KAKIAGE_H
#define KAKIAGE_H

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

class kakiage {
private:
	bool html_mode_ = true;
	std::vector<std::vector<char>> parse_string(const char *begin, const char *end, const char *sep, const char *stop, const std::map<std::string, std::string> *map, const char **next);
	static std::string string_literal(const char *begin, const char *end, char stop, const char **next);
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

	std::function<std::optional<std::string> (std::string const &name, std::string const &text, std::vector<std::string> const &args)> evaluator;
	std::function<std::optional<std::string> (std::string const &file)> includer;

	std::string generate(const std::string &source, const std::map<std::string, std::string> &map, int include_depth = 0);

	static std::string_view trimmed(const std::string_view &s);
};

#endif // KAKIAGE_H
