
#include "htmlencode.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <string_view>

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

/**
 * @brief html_encode_
 * @param ptr
 * @param end
 * @param utf8lazy 非ASCII文字を &# エンコードするなら false 、そのまま出力するなら true
 * @param vec
 */
static void html_encode_(char const *ptr, char const *end, bool utf8lazy, std::vector<char> *vec)
{
	while (ptr < end) {
		int c = *ptr & 0xff;
		ptr++;
		switch (c) {
		case '&':
			append(vec, "&amp;");
			break;
		case '<':
			append(vec, "&lt;");
			break;
		case '>':
			append(vec, "&gt;");
			break;
		case '\"':
			append(vec, "&quot;");
			break;
		case '\'':
			append(vec, "&apos;");
			break;
		case '\t':
		case '\n':
			append(vec, c);
			break;
		default:
			if (c < 0x80 ? (c < 0x20 || c == '\'') : !utf8lazy) {
				char tmp[10];
				sprintf(tmp, "&#%u;", c);
				append(vec, tmp);
			} else {
				append(vec, c);
			}
		}
	}
}

static void html_decode_(char const *ptr, char const *end, std::vector<char> *vec)
{
	while (ptr < end) {
		int c = *ptr & 0xff;
		ptr++;
		if (c == '&') {
			char const *next = strchr(ptr, ';');
			if (!next) {
				break;
			}
			std::string t(ptr, next);
			if (t[0] == '#') {
				c = atoi(t.c_str() + 1);
				append(vec, c);
			} else if (t == "amp") {
				append(vec, '&');
			} else if (t == "lt") {
				append(vec, '<');
			} else if (t == "gt") {
				append(vec, '>');
			} else if (t == "quot") {
				append(vec, '\"');
			} else if (t == "apos") {
				append(vec, '\'');
			}
			ptr = next + 1;
		} else {
			append(vec, c);
		}
	}
}

std::string html_encode(char const *ptr, char const *end, bool utf8lazy)
{
	std::vector<char> vec;
	vec.reserve((end - ptr) * 2);
	html_encode_(ptr, end, utf8lazy, &vec);
	return (std::string)to_string(vec);
}

std::string html_decode(char const *ptr, char const *end)
{
	std::vector<char> vec;
	vec.reserve((end - ptr) * 2);
	html_decode_(ptr, end, &vec);
	return (std::string)to_string(vec);
}

std::string html_encode(char const *ptr, size_t len, bool utf8lazy)
{
	return html_encode(ptr, ptr + len, utf8lazy);
}

std::string html_decode(char const *ptr, size_t len)
{
	return html_decode(ptr, ptr + len);
}

std::string html_encode(char const *ptr, bool utf8lazy)
{
	return html_encode(ptr, strlen(ptr), utf8lazy);
}

std::string html_decode(char const *ptr)
{
	return html_decode(ptr, strlen(ptr));
}

std::string html_encode(std::string const &str, bool utf8lazy)
{
	char const *begin = str.c_str();
	char const *end = begin + str.size();
	char const *ptr = begin;
	while (ptr < end) {
		int c = *ptr & 0xff;
		if (isspace(c) || strchr("&<>\"\'", c)) {
			break;
		}
		ptr++;
	}
	if (ptr == end) {
		return str;
	}
	std::vector<char> vec;
	vec.reserve(str.size() * 2);
	vec.insert(vec.end(), begin, ptr);
	html_encode_(ptr, end, utf8lazy, &vec);
	begin = &vec[0];
	end = begin + vec.size();
	return std::string(begin, end);
}

std::string html_decode(std::string const &str)
{
	char const *begin = str.c_str();
	char const *end = begin + str.size();
	char const *ptr = begin;
	while (ptr < end) {
		int c = *ptr & 0xff;
		if (c == '&') {
			break;
		}
		ptr++;
	}
	if (ptr == end) {
		return str;
	}
	std::vector<char> vec;
	vec.reserve(str.size() * 2);
	vec.insert(vec.end(), begin, ptr);
	html_decode_(ptr, end, &vec);
	begin = &vec[0];
	end = begin + vec.size();
	return std::string(begin, end);
}

