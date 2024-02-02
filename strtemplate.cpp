#include "htmlencode.h"
#include "strtemplate.h"
#include "urlencode.h"
#include <cstring>
#include <numeric>
#include <optional>
#include <vector>
#include "strformat.h"


#ifdef _WIN32
#else
#include "UnixProcess.h"
#endif

namespace {

static inline void append(std::vector<char> *out, char const *begin, char const *end)
{
	out->insert(out->end(), begin, end);
}

static inline void append(std::vector<char> *out, std::string_view const &s)
{
	append(out, s.begin(), s.end());
}

std::string_view to_string(std::vector<char> const &vec)
{
	if (!vec.empty()) {
		return {vec.data(), vec.size()};
	}
	return {};
}

std::string to_string(std::vector<std::vector<char>> const &list)
{
	std::string out;
	for (auto const &vec : list) {
		out.append(vec.data(), vec.size());
	}
	return out;
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
			std::string_view line(left, ptr - left);
			if (opt.trim_spaces) {
				line = strtemplate::trimmed(line);
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

std::optional<std::string> run(std::string const &command)
{
	UnixProcess proc;
	proc.start(command, false);
	if (proc.wait() == 0) {
		return proc.outstring();
	}
	return std::nullopt;
}

} // namespace

std::string_view strtemplate::trimmed(const std::string_view &s)
{
	size_t i = 0;
	size_t j = s.size();
	char const *p = s.data();
	while (i < j && isspace((unsigned char)p[i])) i++;
	while (i < j && isspace((unsigned char)p[j - 1])) j--;
	return s.substr(i, j - i);
}


std::vector<std::vector<char>> strtemplate::build_string(char const *begin, char const *end, char stop, char const **next)
{
	auto Format = [&](std::vector<std::string> const &args){
		std::string result;
		if (!args.empty()) {
			auto Arg = [&](int i){
				std::string_view a = args[i];
				a = trimmed(a);
				size_t j = a.size();
				if (j >= 2 && a[0] == '\"' && a[j - 1] == '\"') { // remove double quote
					a = a.substr(1, j - 2);
				}
				return a;
			};
			strformat f((std::string)Arg(0));
			for (size_t i = 1; i < args.size(); i++) {
				std::string_view a = Arg(i);
				size_t j = a.size();
				if (j >= 2 && a[0] == '`' && a[j - 1] == '`') {
					std::string s(a.substr(1, j - 2)); // remove back quote
					auto r = run(s); // run command
					if (r) {
						f.a(trimmed(*r)); // append result
					} else {
						fprintf(stderr, "command '%s' failed\n", s.c_str());
					}
				} else if (j >= 2 && a[0] == '<' && a[j - 1] == '>') {
					std::string s(a.substr(1, j - 2));
					if (includer) {
						auto t = includer(s); // load template
						if (t) {
							f.a(trimmed(*t)); // append result
						} else {
							fprintf(stderr, "include file '%s' not found\n", s.c_str());
						}
					} else {
						fprintf(stderr, "include function is not defined\n");
					}
				} else {
					f.a(a); // append string
				}
			}
			result = f.str(); // formatted string
		}
		return result;
	};

	*next = end;

	std::vector<std::vector<char>> out;
	out.push_back({});
	out.back().reserve(256);
	char const *left = begin;
	char const *right = begin;
	char quote = 0;
	while (1) {
		if (right < end) {
			char c = right[0];
			if (quote == 0) {
				if (c == stop) {
					*next = right;
					break;
				}
				if (stop == ')' && c == ',') {
					right++;
					out.push_back({});
					out.back().reserve(256);
					continue;
				}
				if (c == '\"' || c == '\'') {
					out.back().push_back(c);
					quote = c;
					right++;
				} else if (c == '<') {
					out.back().push_back(c);
					quote = '>';
					right++;
				} else if (c == '`') {
					right++;
					char const *p = end;
					std::string t(trimmed(to_string(build_string(right, end, '`', &p))));
					if (left < p && *p == '`') {
						p++;
						auto r = run(t);
						if (r) {
							append(&out.back(), *r);
						} else {
							append(&out.back(), left, right);
						}
					}
					left = right = p;
				} else if (right + 1 < end && (c == '$' || c == '%') && right[1] == '(') { // $(ENV) or %(format, ...)
					left = right;
					right += 2;
					std::optional<std::string> text;
					char const *p = end;
					auto list = build_string(right, end, ')', &p);
					if (left < p && *p == ')') {
						if (c == '$') { // $(ENV)
							std::string v = to_string(list);
							text = getenv(v.data()); // get environment variable
						} else if (c == '%') { // %(format, ...)
							std::vector<std::string> args;
							for (auto const &vec : list) {
								args.push_back((std::string)to_string(vec));
							}
							text = Format(args);
						}
						p++;
					}
					if (text) {
						append(&out.back(), *text);
					} else {
						append(&out.back(), left, right);
					}
					left = right = p;
				} else {
					out.back().push_back(c);
					right++;
				}
			} else {
				out.back().push_back(c);
				if (c == quote) {
					quote = 0;
				}
				right++;
			}
		} else {
			break;
		}
	}
	return out;
}

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

	std::vector<char> condition_stack; // すべてtrueなら条件分岐が真として処理する。格納される値は 0 か 1 のみ。
	bool condition = true;

	auto UpdateCondition = [&](){
		condition = std::accumulate(condition_stack.begin(), condition_stack.end(), 0) == condition_stack.size();
	};
	auto outc = [&](char c){
		if (condition && comment_depth == 0) {
			out.push_back(c);
		}
	};
	auto outs = [&](std::string_view const &s){
		if (condition && comment_depth == 0) {
			append(&out, s);
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
				UpdateCondition();
			}
		};
		auto EatNL = [&](){ // 改行を読み飛ばす
			while (ptr < end) {
				if (*ptr == '\r') {
					ptr++;
					if (ptr < end && *ptr == '\n') {
						ptr++;
					}
					break;
				}
				if (*ptr == '\n') {
					ptr++;
					break;
				}
				ptr++;
			}
		};
		bool extraflag = false;
		char leader = 0;
		if (c == '{' && ptr + 2 < end && ptr[1] == '{') {
			leader = ptr[2];
			if (leader == '.' || leader == '#' || leader == ';') {
				// {{. or {{# or {{;
			} else {
				leader = 0;
			}
		}
		if (ptr + 4 < end && (leader != 0 && leader != ';') && ptr[3] == '}' && ptr[4] == '}') {
			// {{.}} or {{#}}
			ptr += 5;
			EatNL();
			END();
			continue;
		}
		if (leader != 0) { // valid leader or comment
			if (leader == ';' || (ptr + 3 < end && leader != 0 && ptr[3] == ';')) { // {{; or {{.; or {{#; is comment
				comment_depth = 1;
			}
			ptr += 3;

			std::string key;
			std::string value;

			// parse {{.foo}} or {{.foo.bar}} or {{.foo(bar)}}
			char const *p = ptr;
			while (p + 1 < end) {
				c = (unsigned char)*p;
				if (c == '{' && p[1] == '{') { // {{
					if (comment_depth > 0) { // inclease comment depth
						comment_depth++;
					}
					p += 2;
				} else if (c == '$' && p[1] == '(') { // $(ENV)
					extraflag = true;
					p += 2;
				} else if (c == '%' && p[1] == '(') { // %(format)
					extraflag = true;
					p += 2;
				} else if (c == '}' && p[1] == '}') { // }}
					if (isspace((unsigned char)*ptr)) {
						key = value = {};
					} else {
						value = {ptr, p - ptr};
						size_t i = find_any(value, ".(<$%");
						if (i != std::string::npos) {
							int left = value[i];
							int right = value[value.size() - 1];
							if (left == '.') { // {{.foo.bar}}
								key = value.substr(0, i);
								value = value.substr(i + 1);
							} else if (left == '(' || (i == 0 && left == '<' && right == '>')) { // {{.foo(bar)}} or {{.<bar>}}
								key = value.substr(0, i);
								value = value.substr(i);
							} else {
								// keep key empty
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
			ptr = p;

			auto is_key = [&](char const *s)->bool{
				int i;
				for (i = 0; s[i]; i++) {
					if (i >= key.size()) return false;
					if (key[i] != s[i]) return false;
				}
				return i == key.size();
			};
			auto is_directive = [&](char const *s)->bool{
				return leader == '#' && is_key(s);
			};
			auto Output = [&](std::string const &v){
				if (key.empty()) { // {{.foo}}
					if (is_html_mode()) { // if html mode, output html encoded value
						html_encode(v, true);
					}
					outs(v);
					return true;
				}
				if (is_directive("html")) { // {{#html.foo}}
					outs(html_encode(v, true)); // output html encoded value
					return true;
				}
				if (is_directive("raw")) { // {{#raw.foo}}
					outs(v); // output raw value
					return true;
				}
				if (is_directive("url")) { // {{#url.foo}}
					outs(url_encode(v, true)); // output url encoded value
					return true;
				}
				return false;
			};

			if (leader == '#' && key.empty()) { // in case of {{#else}}, {{#end}}, ...
				END();
				std::swap(key, value);
			}

			if (extraflag) {
				char const *begin = value.data();
				char const *end = begin + value.size();
				char const *p;
				value = to_string(build_string(begin, end, 0, &p));
			}

			if (value.size() > 2 && value[0] == '(' && value[value.size() - 1] == ')') {
				value = value.substr(1, value.size() - 2);
			}
			if (value.size() > 2 && value[0] == '<' && value[value.size() - 1] == '>') {
				value = value.substr(1, value.size() - 2);
				if (includer) {
					if (include_depth < 10) { // limit includer depth
						auto t = includer(value); // load template
						if (t) {
							std::string u = generate(*t, map); // apply template
							outs(trimmed(u));
						} else {
							fprintf(stderr, "include file '%s' not found\n", value.c_str());
						}
					} else {
						fprintf(stderr, "include depth too deep\n");
					}
				} else {
					fprintf(stderr, "include function is not defined\n");
				}
			} else if (value.size() > 2 && value[0] == '`' && value[value.size() - 1] == '`') {
				value = value.substr(1, value.size() - 2);
				auto r = run(value);
				if (r) {
					value = *r;
					value = trimmed(value);
				}
				Output(value);
			} else if (is_directive("define")) { // {{#define.foo=bar}} or {{#define.foo bar}}
				size_t i = value.find('=');
				if (i == std::string::npos) { // '=' がない場合は空白で区切る
					i = 0;
					while (i < value.size() && !isspace((unsigned char)value[i])) i++;
				}
				std::string name = value.substr(0, i);
				value = trimmed(std::string_view(value).substr(i + 1));
				if (value.size() > 2 && value[0] == '`' && value[value.size() - 1] == '`') {
					value = value.substr(1, value.size() - 2);
					auto r = run(value);
					if (r) {
						value = *r;
						value = trimmed(value);
					}
				}
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
				EatNL();
			} else if (is_directive("put")) { // {{#put.foo}}
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
					(void)arg;
					std::string u = generate(*text, map);
					outs(u);
				} else {
					std::optional<std::string> t;
					if (evaluator) {
						t = evaluator(value, arg);
					}
					if (t) {
						std::string u = generate(*t, map);
						outs(u);
					} else {
						fprintf(stderr, "undefined macro '%s'\n", value.c_str());
					}
				}
			} else if (is_directive("include")) { // {{#include(foo)}}
				if (includer) {
					if (include_depth < 10) { // limit includer depth
						auto t = includer(value); // load template
						if (t) {
							std::string u = generate(*t, map); // apply template
							outs(trimmed(u));
						} else {
							fprintf(stderr, "include file '%s' not found\n", value.c_str());
						}
					} else {
						fprintf(stderr, "include depth too deep\n");
					}
				} else {
					fprintf(stderr, "include function is not defined\n");
				}
			} else if (is_directive("jsx")) { // {{#jsx(foo)}}
				if (includer) {
					if (include_depth < 10) { // limit includer depth
						auto args = split_words(value, ',');
						if (args.size() >= 2) {
							auto name = args[0];
							auto el = args[1];
							auto t = includer((std::string)name); // load template
							if (t) {
								std::string u = generate(*t, map); // apply template
								std::string s = "let " + (std::string)el + " = (" + (std::string)trimmed(u) + ')';
								outs(s);
							} else {
								fprintf(stderr, "include file '%s' not found\n", value.c_str());
							}
						}
					} else {
						fprintf(stderr, "include depth too deep\n");
					}
				} else {
					fprintf(stderr, "include function is not defined\n");
				}
			} else {
				if (!value.empty()) { // 値を置換
					auto it = map.find(value);
					if (it != map.end()) {
						value = it->second; // replace value
					}
				}
				if (Output(value)) {
					// ok
				} else if (is_directive("if")) { // {{#if.foo}}
					bool f = atoi(value.c_str()) != 0;
					condition_stack.push_back(f);
					UpdateCondition();
				} else if (is_directive("ifn")) { // {{#ifn.foo}} // if not
					bool f = atoi(value.c_str()) == 0;
					condition_stack.push_back(f);
					UpdateCondition();
				} else if (is_directive("else")) { // {{#else}}
					condition = !condition;
				} else if (is_directive("end")) { // {{#end}}
					EatNL();
					END();
				}
			}
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
