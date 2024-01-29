
#include "urlencode.h"
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <vector>


#ifdef WIN32
#pragma warning(disable:4996)
#endif

namespace {

inline void append(std::vector<char> *out, char c)
{
	out->push_back(c);
}

inline void append(std::vector<char> *out, char const *s)
{
	out->insert(out->end(), s, s + strlen(s));
}

inline std::string_view to_string(std::vector<char> const &vec)
{
	if (!vec.empty()) {
		return {vec.data(), vec.size()};
	}
	return {};
}

} // namespace

static void url_encode_(char const *ptr, char const *end, std::vector<char> *out, bool utf8lazy)
{
	while (ptr < end) {
		int c = (unsigned char)*ptr;
		ptr++;
		if (isalnum(c) || strchr("_.-~", c)) {
			append(out, c);
		} else if (utf8lazy && c >= 0x80) {
			append(out, c);
		} else if (c == ' ') {
			append(out, '+');
		} else {
			char tmp[10];
			sprintf(tmp, "%%%02X", c);
			append(out, tmp[0]);
			append(out, tmp[1]);
			append(out, tmp[2]);
		}
	}
}

std::string url_encode(char const *str, char const *end, bool utf8lazy)
{
	if (!str) {
		return std::string();
	}

	std::vector<char> out;
	out.reserve(end - str + 10);

	url_encode_(str, end, &out, utf8lazy);

	return (std::string)to_string(out);
}

std::string url_encode(char const *str, size_t len, bool utf8lazy)
{
	return url_encode(str, str + len, utf8lazy);
}

std::string url_encode(char const *str, bool utf8lazy)
{
	return url_encode(str, strlen(str), utf8lazy);
}

std::string url_encode(std::string const &str, bool utf8lazy)
{
	char const *begin = str.c_str();
	char const *end = begin + str.size();
	char const *ptr = begin;

	while (ptr < end) {
		int c = (unsigned char)*ptr;
		if (isalnum(c) || strchr("_.-~", c)) {
			// thru
		} else {
			break;
		}
		ptr++;
	}
	if (ptr == end) {
		return str;
	}

	std::vector<char> out;
	out.reserve(str.size() + 10);

	out.insert(out.end(), begin, ptr);
	url_encode_(ptr, end, &out, utf8lazy);

	return (std::string)to_string(out);
}

static void url_decode_(char const *ptr, char const *end, std::vector<char> *out)
{
	while (ptr < end) {
		int c = (unsigned char)*ptr;
		ptr++;
		if (c == '+') {
			c = ' ';
		} else if (c == '%' && isxdigit((unsigned char)ptr[0]) && isxdigit((unsigned char)ptr[1])) {
			char tmp[3]; // '%XX'
			tmp[0] = ptr[0];
			tmp[1] = ptr[1];
			tmp[2] = 0;
			c = strtol(tmp, nullptr, 16);
			ptr += 2;
		}
		append(out, c);
	}
}

std::string url_decode(char const *str, char const *end)
{
	if (!str) {
		return std::string();
	}

	std::vector<char> out;
	out.reserve(end - str + 10);

	url_decode_(str, end, &out);

	return (std::string)to_string(out);
}

std::string url_decode(char const *str, size_t len)
{
	return url_decode(str, str + len);
}

std::string url_decode(char const *str)
{
	return url_decode(str, strlen(str));
}

std::string url_decode(std::string const &str)
{
	char const *begin = str.c_str();
	char const *end = begin + str.size();
	char const *ptr = begin;

	while (ptr < end) {
		int c = *ptr & 0xff;
		if (c == '+' || c == '%') {
			break;
		}
		ptr++;
	}
	if (ptr == end) {
		return str;
	}

	std::vector<char> out;
	out.reserve(str.size() + 10);

	out.insert(out.end(), begin, ptr);
	url_decode_(ptr, end, &out);

	return (std::string)to_string(out);
}
