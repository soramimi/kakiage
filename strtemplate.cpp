#include "htmlencode.h"
#include "strtemplate.h"
#include "urlencode.h"
#include <cstring>
#include <numeric>
#include <optional>
#include <vector>
#include "strformat.h"

#ifdef _WIN32
#include "Win32Process.h"
#else
#include "UnixProcess.h"
#endif

namespace {

static bool issym(int c)
{
	return isalnum(c) || c == '_';
}

static inline void append(std::vector<char> *out, char const *begin, char const *end)
{
	out->insert(out->end(), begin, end);
}

static inline void append(std::vector<char> *out, std::string_view const &s)
{
	append(out, s.data(), s.data() + s.size());
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
#ifdef _WIN32
	Win32Process proc;
#else
	UnixProcess proc;
#endif
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


std::vector<std::vector<char>> strtemplate::build_string(char const *begin, char const *end, char const *sep, char const *stop, std::map<std::string, std::string> const *map, char const **next)
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
						fprintf(stderr, "command '%s' failed\n", s.data());
					}
				} else if (j >= 2 && a[0] == '<' && a[j - 1] == '>') {
					std::string s(a.substr(1, j - 2));
					if (includer) {
						auto t = includer(s); // load template
						if (t) {
							f.a(trimmed(*t)); // append result
						} else {
							fprintf(stderr, "include file '%s' not found\n", s.data());
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
	char const *right = begin;
	while (right < end) {
		char c = right[0];
		if (strchr(stop, c)) {
			if (map) {
				std::string key(to_string(out.back()));
				auto it = map->find(key);
				if (it != map->end()) {
					char const *begin = it->second.data();
					char const *end = begin + it->second.size();
					out.back() = std::vector<char>(begin, end);
				} else {
					out.back().insert(out.back().begin(), '?');
					out.back().push_back('?');
					fprintf(stderr, "undefined symbol '%s'\n", key.data());
				}
			}
			*next = right;
			break;
		}
		if (sep && strchr(sep, c)) {
			c = right[0];
			right++;
			out.push_back({});
			out.back().reserve(256);
			if (c == '(') {
				auto list = build_string(right, end, nullptr, ")", nullptr, &right);
				for (auto const &vec : list) {
					out.back().insert(out.back().end(), vec.begin(), vec.end());
				}
				if (right < end) {
					right++;
				}
			}
			continue;
		}
		if (c == '#') {
			right++;
			auto list = build_string(right, end, nullptr, ".(}", nullptr, &right);
			c = *right;
			if (strchr(stop, c)) {
				*next = right;
				break;
			}
			if (right < end) {
				right++;
			}
			for (auto const &vec : list) {
				out.back().insert(out.back().end(), vec.begin(), vec.end());
			}
			char const *se;
			char const *st;
			if (c == '(') {
				se = ",";
				st = ")}";
			} else {
				se = nullptr;
				st = "}";
			}
			auto list2 = build_string(right, end, se, st, nullptr, &right);
			for (auto const &vec : list2) {
				out.emplace_back(vec.begin(), vec.end());
			}
			if (strchr(stop, *right)) {
				*next = right;
				break;
			}
			if (right < end && *right != '}') {
				right++;
			}
			map = nullptr;
		} else if (c == '\"' || c == '\'' || c == '`' || c == '<') {
			right++;
			char e[2];
			if (c == '<') {
				e[0] = '>';
			} else {
				e[0] = c;
			}
			e[1] = 0;
			auto list = build_string(right, end, nullptr, e, nullptr, &right);
			if (right < end) {
				right++;
			}
			std::vector<char> v;
			for (auto const &vec : list) {
				if (c == '`') {
					std::string s(to_string(vec));
					auto r = run(s); // run command
					if (r) {
						append(&v, trimmed(*r)); // append result
					} else {
						fprintf(stderr, "command '%s' failed\n", s.data());
					}
				} else if (c == '<') {
					std::string s(to_string(vec));
					if (includer) {
						auto t = includer(s); // load template
						if (t) {
							append(&v, trimmed(*t)); // append result
						} else {
							fprintf(stderr, "include file '%s' not found\n", s.data());
						}
					} else {
						fprintf(stderr, "include function is not defined\n");
					}
				} else {
					v.insert(v.end(), vec.begin(), vec.end());
				}
			}
			out.back() = v;
			map = nullptr;
		} else if (right + 1 < end && (c == '$' || c == '%') && right[1] == '(') { // $(ENV) or %(format, ...)
			right += 2;
			auto list = build_string(right, end, ",", ")", nullptr, &right);
			if (right < end) {
				right++;
			}
			if (c == '$') { // $(ENV)
				std::string v = to_string(list);
				char *text = getenv(v.data()); // get environment variable
				if (text) {
					append(&out.back(), text);
				} else {
					fprintf(stderr, "environment variable '%s' not found\n", v.data());
				}
			} else if (c == '%') { // %(format, ...)
				std::vector<std::string> args;
				for (auto const &vec : list) {
					args.push_back((std::string)to_string(vec));
				}
				std::string text = Format(args);
				append(&out.back(), text);
			}
			map = nullptr;
		} else {
			out.back().push_back(c);
			right++;
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

	char const *begin = source.data();
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
		if (c == '{' && ptr + 2 < end && ptr[1] == '{' && ptr[2] == '.') {
			ptr += 3;
			if (ptr + 1 < end && ptr[0] == '}' && ptr[1] == '}') {
				// {{.}}
				ptr += 5;
				EatNL();
				END();
				continue;
			}

			if (ptr[0] == ';') { // {{.;comment}}
				comment_depth = 1;
				ptr++;
			}

			std::string key;
			std::string value;
			std::vector<std::string> values;

			char leader = *ptr;
			std::vector<std::vector<char>> v = build_string(ptr, end, ".(", "}", &map, &ptr);
			if (ptr < end && *ptr == '}') {
				ptr++;
				if (ptr < end && *ptr == '}') {
					ptr++;
				}
			}
			if (v.size() == 1) {
				value = to_string(v[0]);
				values.emplace_back(to_string(v[0]));
			} else if (v.size() > 1) {
				key = to_string(v[0]);
				value = to_string(v[1]);
				for (size_t i = 1; i < v.size(); i++) {
					values.emplace_back(to_string(v[i]));
				}
			} else {
				continue;
			}

			auto IsKey = [&](char const *s)->bool{
				int i;
				for (i = 0; s[i]; i++) {
					if (i >= key.size()) return false;
					if (key[i] != s[i]) return false;
				}
				return i == key.size();
			};
			auto IsDirective = [&](char const *s)->bool{
				return leader == '#' && IsKey(s);
			};
			auto Output = [&](std::string const &v){
				if (key.empty()) { // {{.foo}}
					if (is_html_mode()) { // if html mode, output html encoded value
						html_encode(v, true);
					}
					outs(v);
					return true;
				}
				if (IsDirective("html")) { // {{.#html.foo}}
					outs(html_encode(v, true)); // output html encoded value
					return true;
				}
				if (IsDirective("raw")) { // {{.#raw.foo}}
					outs(v); // output raw value
					return true;
				}
				if (IsDirective("url")) { // {{.#url.foo}}
					outs(url_encode(v, true)); // output url encoded value
					return true;
				}
				return false;
			};

			if (IsDirective("define")) { // {{.#define.foo=bar}} or {{.#define.foo bar}}
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
			} else if (IsDirective("raw")) { // {{.#raw.foo}}
				auto it = map.find(value);
				if (it != map.end()) {
					value = it->second; // replace value
				} else {
					value = '?' + value + '?';
					fprintf(stderr, "undefined symbol '%s'\n", value.data());
				}
				Output(value);
			} else if (IsDirective("put")) { // {{.#put.foo}}
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
						fprintf(stderr, "undefined macro '%s'\n", value.data());
					}
				}
			} else if (IsDirective("include")) { // {{.#include(foo)}}
				if (includer) {
					if (include_depth < 10) { // limit includer depth
						auto t = includer(value); // load template
						if (t) {
							std::string u = generate(*t, map); // apply template
							outs(trimmed(u));
						} else {
							fprintf(stderr, "include file '%s' not found\n", value.data());
						}
					} else {
						fprintf(stderr, "include depth too deep\n");
					}
				} else {
					fprintf(stderr, "include function is not defined\n");
				}
			} else if (IsDirective("jsx")) { // {{.#jsx(foo)}}
				if (includer) {
					if (include_depth < 10) { // limit includer depth
						if (values.size() >= 2) {
							auto name = trimmed(values[0]);
							auto el = trimmed(values[1]);
							auto t = includer((std::string)name); // load template
							if (t) {
								std::string u = generate(*t, map); // apply template
								std::string s = "let " + (std::string)el + " = (" + (std::string)trimmed(u) + ')';
								outs(s);
							} else {
								fprintf(stderr, "include file '%s' not found\n", value.data());
							}
						}
					} else {
						fprintf(stderr, "include depth too deep\n");
					}
				} else {
					fprintf(stderr, "include function is not defined\n");
				}
			} else {
				if (Output(value)) {
					// ok
				} else if (IsDirective("if")) { // {{.#if.foo}}
					bool f = atoi(value.data()) != 0;
					condition_stack.push_back(f);
					UpdateCondition();
				} else if (IsDirective("ifn")) { // {{.#ifn.foo}} // if not
					bool f = atoi(value.data()) == 0;
					condition_stack.push_back(f);
					UpdateCondition();
				} else if (IsDirective("else")) { // {{.#else}}
					condition = !condition;
				} else if (IsDirective("end")) { // {{.#end}}
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
