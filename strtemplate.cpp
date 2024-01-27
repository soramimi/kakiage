#include "htmlencode.h"
#include "strtemplate.h"
#include "urlencode.h"
#include <cstring>
#include <numeric>
#include <optional>
#include <vector>

namespace {

void vecprint(std::vector<char> *out, std::string_view const &s)
{
	out->insert(out->end(), s.begin(), s.end());
}

std::string_view trimmed(const std::string_view &s)
{
	size_t i = 0;
	size_t j = s.size();
	char const *p = s.data();
	while (i < j && isspace((unsigned char)p[i])) i++;
	while (i < j && isspace((unsigned char)p[j - 1])) j--;
	return s.substr(i, j - i);
}

std::string_view to_string(std::vector<char> const &vec)
{
	if (!vec.empty()) {
		return {vec.data(), vec.size()};
	}
	return {};
}

size_t find_any(const std::string_view &str, const char *chrs)
{
	size_t i = 0;
	while (i < str.size()) {
		if (strchr(chrs, str[i])) {
			return i;
		}
		i++;
	}
	return std::string::npos;
}

struct split_opt_t {
	unsigned char sep_ch = 0; // 区切り文字
	char const *sep_any = nullptr; // 区切り文字の集合
	std::function<bool(int c)> sep_fn; // 区切り文字の判定関数
	bool keep_empty_lines = true; // 空行を保持するかどうか
	bool trim_spaces = true; // 空白文字を削除するかどうか
};

/**
 * @brief 文字列を分割する
 * @param begin
 * @param end
 * @param opt
 */
static inline std::vector<std::string_view> split_internal(char const *begin, char const *end, split_opt_t const &opt)
{
	std::vector<std::string_view> out;
	out.clear();
	char const *ptr = begin;
	while (isspace(*ptr)) {
		ptr++;
	}
	char const *left = ptr;
	while (true) {
		int c = -1;
		if (ptr < end) {
			c = *ptr & 0xff;
		}
		auto is_separator = [&](int c){
			if (opt.sep_ch && c == opt.sep_ch) return true;
			if (opt.sep_fn && opt.sep_fn(c)) return true;
			if (opt.sep_any && strchr(opt.sep_any, c)) return true;
			return false;
		};
		if (is_separator(c) || c < 0) {
			std::basic_string_view<char> line(left, ptr - left);
			if (opt.trim_spaces) {
				line = trimmed(line);
			}
			if (opt.keep_empty_lines || !line.empty()) {
				out.push_back(line);
			}
			if (c < 0) break;
			ptr++;
			left = ptr;
		} else {
			ptr++;
		}
	}
	return out;
}

/**
 * @brief 文字列を分割する
 * @param begin
 * @param end
 * @param out
 *
 * 文字列を指定した文字で分割する。
 */
std::vector<std::string_view> split_words(char const *begin, char const *end, char sep)
{
	split_opt_t opt;
	opt.sep_ch = sep;
	return split_internal(begin, end, opt);
}

/**
 * @brief 文字列を分割する
 * @param str
 * @param c
 * @param out
 *
 * 文字列を空白文字で分割する。
 */
std::vector<std::string_view> split_words(std::string_view const &str, char sep)
{
	char const *begin = str.data();
	char const *end = begin + str.size();
	return split_words(begin, end, sep);
}

} // namespace

/**
 * @brief ページを生成する（テンプレートエンジン）
 * @param source テンプレートテキスト
 * @param map 置換マップ
 * @return ページテキスト
 */
std::string strtemplate::generate(const std::string &source, const std::map<std::string, std::string> &map, int include_depth)
{
	std::map<std::string, std::string> macro;
	defines.push_back(&macro);

	std::vector<char> out;
	out.reserve(4096);

	int comment_depth = 0;
	char const *begin = source.c_str();
	char const *end = begin + source.size();
	char const *ptr = begin;
	std::vector<char> condition_stack;
	bool emission = true;
	auto UpdateEmission = [&](){
		emission = std::accumulate(condition_stack.begin(), condition_stack.end(), 0) == condition_stack.size();
	};
	auto outc = [&](char c){
		if (emission && comment_depth == 0) {
			out.push_back(c);
		}
	};
	auto outs = [&](std::string_view const &s){
		if (emission && comment_depth == 0) {
			vecprint(&out, s);
		}
	};
	auto FindMacro = [&](std::string const &name)->std::optional<std::string>{
		size_t i = defines.size();
		while (i > 0) {
			i--;
			auto it = defines[i]->find(name);
			if (it != defines[i]->end()) {
				return it->second;
			}
		}
		return std::nullopt;
	};
	while (1) {
		comment_depth = 0;
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
			// {{.}}
			ptr += 5;
			END();
		} else if (c == '{' && ptr + 2 < end && ptr[1] == '{' && (ptr[2] == '.' || ptr[2] == ';')) {
			// {{. or {{;
			ptr += 2;
			if (*ptr == ';') { // {{;comment}}
				comment_depth = 1;
			}
			ptr++;
			std::string key;
			std::string value;
			char const *p = ptr;
			// parse {{.foo}} or {{.foo.bar}} or {{.foo(bar)}}
			while (p + 1 < end) {
				c = (unsigned char)*p;
				if (p + 1 < end && c == '{' && p[1] == '{') { // {{
					if (comment_depth > 0) { // inclease comment depth
						comment_depth++;
					}
					p += 2;
				} else if (c == '}' && p[1] == '}') { // }}
					if (isspace((unsigned char)*ptr)) {
						key = value = {};
					} else {
						value = {ptr, p - ptr};
						size_t i = find_any(value, ".(");
						if (i != std::string::npos) {
							if (value[i] == '.') { // {{.foo.bar}}
								key = value.substr(0, i);
								value = value.substr(i + 1);
							} else if (value[i] == '(') { // {{.foo(bar)}}
								size_t j = value.size();
								if (value[j - 1] == ')') {
									j--;
									key = value.substr(0, i++);
									value = value.substr(i, j - i);
								}
							} else {
								fprintf(stderr, "syntax error: %s\n", value.c_str());
							}
						} else {
							// keep key empty
						}
					}
					p += 2;
					if (comment_depth < 2) {
						break;
					}
					comment_depth--;
				} else {
					p++;
				}
			}
			auto iskey = [&key](char const *s)->bool{
				int i;
				for (i = 0; s[i]; i++) {
					if (i >= key.size()) return false;
					if (key[i] != s[i]) return false;
				}
				return i == key.size();
			};
			if (iskey("define")) { // {{.define.foo=bar}}
				size_t i = value.find('=');
				std::string name = value.substr(0, i);
				value = trimmed(std::string_view(value).substr(i + 1));
				if (!name.empty()) {
					if (value.empty()) {
						auto it = macro.find(name);
						if (it != macro.end()) {
							macro.erase(it);
						}
					} else {
						macro[name] = value;
					}
				} else {
					fprintf(stderr, "define name is empty\n");
				}
			} else if (iskey("put")) { // {{.put.foo}}
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
				auto text = FindMacro(value);
				if (text) {
					std::string u = generate(*text, map);
					outs(u);
				} else {
					if (evaluator) {
						std::string t = evaluator(value, arg);
						std::string u = generate(t, map);
						outs(u);
					} else {
						outs(value);
					}
				}
			} else if (iskey("include")) { // {{.include(foo)}}
				if (includer) {
					if (include_depth < 10) { // limit includer depth
						std::string t = includer(value); // load template
						std::string u = generate(t, map); // apply template
						outs(u);
					} else {
						fprintf(stderr, "include depth too deep\n");
					}
				} else {
					fprintf(stderr, "include function is not defined\n");
				}
			} else if (iskey("jsx")) {
				if (includer) {
					if (include_depth < 10) { // limit includer depth
						auto args = split_words(value, ',');
						if (args.size() >= 2) {
							auto name = args[0];
							auto el = args[1];
							std::string t = includer((std::string)name); // load template
							std::string u = generate(t, map); // apply template
							std::string s = "let " + (std::string)el + " = (" + (std::string)trimmed(u) + ')';
							outs(s);
						}
					} else {
						fprintf(stderr, "include depth too deep\n");
					}
				} else {
					fprintf(stderr, "include function is not defined\n");
				}
			} else {
				if (!value.empty()) { // {{.foo}}
					auto it = map.find(value);
					if (it != map.end()) {
						value = it->second; // replace value
					} else {
						if (emission) {
							fprintf(stderr, "undefined value '%s'\n", value.c_str());
						}
						value = {};
					}
				}
				if (key.empty()) {
					outs(html_encode(value, true)); // output escaped value
				} else if (iskey("raw")) { // {{.raw.foo}}
					outs(value); // output raw value
				} else if (iskey("url")) { // {{.url.foo}}
					outs(url_encode(value, true)); // output url encoded value
				} else if (iskey("if")) { // {{.if.foo}}
					bool f = atoi(value.c_str()) != 0;
					condition_stack.push_back(f);
					UpdateEmission();
				} else if (iskey("ifn")) { // {{.ifn.foo}} // if not
					bool f = atoi(value.c_str()) == 0;
					condition_stack.push_back(f);
					UpdateEmission();
				} else if (iskey("else")) { // {{.else}}
					emission = !emission;
				} else if (iskey("end")) { // {{.end}}
					END();
				}
			}
			ptr = p;
		} else if (comment_depth == 0 && c == '&' && ptr + 1 < end && strchr("&.{}", ptr[1])) { // &. or &{ or &} or &&
			ptr++;
			char const *p = ptr;
			while (p < end) {
				if (*p == ';') { // &c;
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

	return (std::string)to_string(out);
}
