#include "charvec.h"
#include "htmlencode.h"
#include "strtemplate.h"
#include "urlencode.h"
#include <cstring>
#include <numeric>
#include <vector>

static std::string trim(const std::string &s)
{
	size_t i = 0;
	size_t j = s.size();
	char const *p = s.c_str();
	while (i < j && isspace(p[i] & 0xff)) i++;
	while (i < j && isspace(p[j - 1] & 0xff)) j--;
	return s.substr(i, j - i);
}

/**
 * @brief ページを生成する（テンプレートエンジン）
 * @param source テンプレートテキスト
 * @param map 置換マップ
 * @return ページテキスト
 */
std::string strtemplate::generate(const std::string &source, const std::map<std::string, std::string> &map)
{
	std::map<std::string, std::string> macro;
	defines.push_back(&macro);

	std::vector<char> out;
	out.reserve(4096);

	int comment = 0;
	char const *begin = source.c_str();
	char const *end = begin + source.size();
	char const *ptr = begin;
	std::vector<char> condition_stack;
	bool emission = true;
	auto UpdateEmission = [&](){
		emission = std::accumulate(condition_stack.begin(), condition_stack.end(), 0) == condition_stack.size();
	};
	auto outc = [&](char c){
		if (emission && comment == 0) {
			out.push_back(c);
		}
	};
	auto outs = [&](std::string const &s){
		if (emission && comment == 0) {
			vecprint(&out, s);
		}
	};
	auto FindMacro = [&](std::string const &name, std::string *out)->bool{
		size_t i = defines.size();
		while (i > 0) {
			i--;
			auto it = defines[i]->find(name);
			if (it != defines[i]->end()) {
				*out = it->second;
				return true;
			}
		}
		return false;
	};
	while (1) {
		int c = 0;
		if (ptr < end) {
			c = (unsigned char)*ptr;
		}
		if (c == 0) {
			break;
		}
		auto END = [&](){
			if (!condition_stack.empty()) {
				condition_stack.pop_back();
				UpdateEmission();
			}
		};
		if (ptr + 4 < end && ptr[0] == '{' && ptr[1] == '{' && ptr[2] == '.' && ptr[3] == '}' && ptr[4] == '}') {
			ptr += 5;
			END();
		} else if (c == '{' && ptr + 2 < end && ptr[1] == '{' && (ptr[2] == '.' || ptr[2] == ';')) {
			ptr += 3;
			int depth = 0;
			if (ptr[2] == ';') {
				comment++;
			}
			std::string key;
			std::string value;
			char const *p = ptr;
			while (p + 1 < end) {
				c = (unsigned char)*p;
				if (p + 2 < end && c == '{' && p[1] == '{' && p[2] == '.') {
					depth++;
					if (comment > 0) {
						comment++;
					}
					p += 3;
				} else if (c == '}' && p[1] == '}') {
					if (depth > 0) {
						depth--;
						if (comment > 0) {
							comment--;
						}
						p += 2;
					} else {
						if (isspace((unsigned char)*ptr)) {
							key = value = {};
						} else {
							value.assign(ptr, p);
							size_t i = value.find('.', 1);
							if (i != std::string::npos && i > 0) {
								key = value.substr(0, i);
								value = value.substr(i + 1);
							} else if (value.c_str()[0] == '.') {
								std::swap(value, key);
							}
						}
						p += 2;
						break;
					}
				}
				p++;
			}
			if (key == "define") {
				size_t i;
				for (i = 0; i < value.size(); i++) {
					if (isspace((unsigned char)value.c_str()[i])) break;
				}
				std::string name = value.substr(0, i);
				value = trim(value.substr(i));
				if (!name.empty()) {
					if (value.empty()) {
						auto it = macro.find(name);
						if (it != macro.end()) {
							macro.erase(it);
						}
					} else {
						macro[name] = value;
					}
				}
			} else if (key == "put") {
				auto p = value.find('(');
				std::string arg;
				if (p != std::string::npos) {
					auto left = p + 1;
					auto right = value.find(')', left);
					if (right != std::string::npos) {
						arg = value.substr(left, right - left);
					}
					value = value.substr(0, p);
				}
#if 0
				auto it = macro.find(value);
				if (it != macro.end()) {
					std::string s = generate(it->second, map, eval);
					outs(s);
				} else {
					fprintf(stderr, "undefined macro '%s'\n", value.c_str());
				}
#else
				if (evaluate) {
					std::string text;
					FindMacro(value, &text);
					std::string t = evaluate(value, text, arg);
					std::string u = generate(t, map);
					outs(u);
				}
#endif
			} else {
				if (!value.empty()) {
					auto it = map.find(value);
					if (it != map.end()) {
						value = it->second;
					} else {
						if (emission) {
							fprintf(stderr, "undefined value '%s'\n", value.c_str());
						}
						value = {};
					}
				}
				if (key.empty()) {
					outs(html_encode(value, true));
				} else if (key == "raw") {
					outs(value);
				} else if (key == "url") {
					outs(url_encode(value, true));
				} else if (key == "if") {
					bool f = std::atoi(value.c_str()) != 0;
					condition_stack.push_back(f);
					UpdateEmission();
				} else if (key == "ifn") {
					bool f = std::atoi(value.c_str()) == 0;
					condition_stack.push_back(f);
					UpdateEmission();
				} else if (key == "else") {
					emission = !emission;
				} else if (key == "end") {
					END();
				}
			}
			ptr = p;
		} else if (comment == 0 && c == '&' && ptr + 1 < end && strchr("&.{}", ptr[1])) {
			ptr++;
			char const *p = ptr;
			while (p < end) {
				if (*p == ';') {
					std::string s(ptr, p);
					outs(s);
					ptr = p + 1;
					c = -1;
					break;
				} else if (*p == '\n') {
					break;
				}
				p++;
			}
			if (c != -1) {
				outc(c);
			}
		} else {
			outc(c);
			ptr++;
		}
	}

	defines.pop_back();

	return to_stdstr(out);
}
