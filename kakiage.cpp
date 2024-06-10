#include "htmlencode.h"
#include "kakiage.h"
#include "urlencode.h"
#include <cstring>
#include <optional>
#include <vector>
#include "strformat.h"

#ifdef _WIN32
#include "Win32Process.h"
#else
#include "UnixProcess.h"
#endif

namespace {

static bool issymf(int c)
{
	return isalpha((unsigned char)c) || c == '_';
}

static bool issym(int c)
{
	return isalnum((unsigned char)c) || c == '_';
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

std::vector<char> to_vector(std::string_view const &view)
{
	std::vector<char> out;
	out.insert(out.end(), view.data(), view.data() + view.size());
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
				line = kakiage::trimmed(line);
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


std::string_view kakiage::trimmed(const std::string_view &s)
{
	size_t i = 0;
	size_t j = s.size();
	char const *p = s.data();
	while (i < j && isspace((unsigned char)p[i])) i++;
	while (i < j && isspace((unsigned char)p[j - 1])) j--;
	return s.substr(i, j - i);
}

std::string kakiage::string_literal(char const *begin, char const *end, char stop, char const **next)
{
	std::vector<char> vec;
	vec.reserve(256);
	size_t i = 0;
	for (i = 0; begin + i < end; i++) {
		char c = begin[i];
		if (c == stop) {
			break;
		}
		if (c == '\\') {
			i++;
			if (begin + i < end) {
				c = begin[i];
				if (c == 'n') {
					vec.push_back('\n');
				} else if (c == 'r') {
					vec.push_back('\r');
				} else if (c == 't') {
					vec.push_back('\t');
				} else if (c == '\"') {
					vec.push_back('\"');
				} else if (c == '\'') {
					vec.push_back('\'');
				} else if (c == '\\') {
					vec.push_back('\\');
				} else {
					vec.push_back(c);
				}
			}
		} else {
			vec.push_back(c);
		}
	}
	*next = begin + i;
	return std::string(to_string(vec));
}

static void parse_string_raw(char const *begin, char const *end, char const **next, std::vector<char> *out)
{
	*next = end;
	char const *right = begin;
	while (right < end) {
		char c = right[0];
		if (c == '}') {
			*next = right;
			break;
		}
		if (right + 1 < end && right[0] == '{' && right[1] == '{') {
			right += 2;
			std::vector<char> v;
			parse_string_raw(right, end, &right, &v);
			if (right + 1 < end && right[0] == '}' && right[1] == '}') {
				right += 2;
				std::string s = "{{" + std::string(to_string(v)) + "}}";
				append(out, s);
			}
		} else {
			if (isspace((unsigned char)c) && out->empty()) {
				// skip leading spaces
			} else {
				out->push_back(c);
			}
			right++;
		}
	}
}

std::vector<std::vector<char>> kakiage::parse_string(char const *begin, char const *end, char const *sep, char const *stop, std::map<std::string, std::string> const *map, char const **next)
{
	*next = end;

	std::vector<std::vector<char>> out;

	bool convert = true;
	auto Convert = [&](){
		if (convert) {
			if (map) {
				std::vector<char> const &v = out.back();
				std::string s(trimmed(std::string_view(v.data(), v.size())));
				if (issymf(s[0])) {
					auto it = map->find(s);
					if (it != map->end()) {
						s = it->second;
					} else {
						s = '?' + s + '?';
						fprintf(stderr, "undefined symbol '%s'\n", s.data());
					}
				}
				out.back().clear();
				out.back().insert(out.back().end(), s.begin(), s.end());
			}
		} else {
			convert = true;
		}
	};

	out.push_back({});
	out.back().reserve(256);
	char const *right = begin;
	while (right < end) {
		char c = right[0];
		if (strchr(stop, c)) {
			Convert();
			*next = right;
			break;
		}
		if (sep && strchr(sep, c)) {
			Convert();
			c = right[0];
			right++;
			out.push_back({});
			out.back().reserve(256);
			if (c == '(') {
				auto list = parse_string(right, end, nullptr, ")", nullptr, &right);
				for (auto const &vec : list) {
					out.back().insert(out.back().end(), vec.begin(), vec.end());
				}
				if (right < end) {
					right++;
				}
			}
			continue;
		}
		if (c == '\"' || c == '\'' || c == '`' || c == '<' || c == '[') {
			right++;
			char e = c;
			if (c == '<') {
				e = '>';
			} else if (c == '[') {
				e = ']';
				fprintf(stderr, "square bracket is reserved\n");
			}

			std::string s = string_literal(right, end, e, &right);
			if (right < end) {
				right++;
			}
			std::vector<char> v;
			if (c == '`') {
				auto r = run(s); // run command
				if (r) {
					append(&v, trimmed(*r)); // append result
				} else {
					fprintf(stderr, "command '%s' failed\n", s.data());
				}
			} else if (c == '<') {
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
				append(&v, s);
			}
			out.back() = v;
			convert = false;
		} else if (right + 1 < end && (c == '$' || c == '%') && right[1] == '(') { // $(ENV) or %(format, ...)
			right += 2;
			auto list = parse_string(right, end, ",", ")", nullptr, &right);
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
				strformat f;
				for (size_t i = 0; i < list.size(); i++) {
					std::string a(to_string(list[i]));
					if (i == 0) {
						f.append(a);
					} else {
						f.a(a);
					}
				}
				append(&out.back(), f.str());
			}
			convert = false;
		} else {
			if (isspace((unsigned char)c) && out.back().empty()) {
				// skip leading spaces
			} else {
				out.back().push_back(c);
			}
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
std::string kakiage::generate(const std::string &source, const std::map<std::string, std::string> &map, int include_depth)
{
	std::map<std::string, std::string> macro;
	defines.push_back(&macro);

	std::vector<char> out;
	out.reserve(4096);

	int comment_depth = 0;

	char const *begin = source.data();
	char const *end = begin + source.size();
	char const *ptr = begin;

	std::vector<unsigned char> condition_stack; // すべてtrueなら条件分岐が真として処理する。格納される値は 0 か 1 のみ。
	unsigned char condition = 1;
	enum {
		COND_FALSE,
		COND_TRUE,
		COND_DONE,
		COND_ELSE,
	};

	auto UpdateCondition = [&](){
		condition = COND_TRUE;
		for (char c : condition_stack) {
			if (c == COND_FALSE || c == COND_DONE) {
				condition = c;
				break;
			}
		}
	};
	auto outc = [&](char c){
		if (condition == COND_TRUE && comment_depth == 0) {
			out.push_back(c);
		}
	};
	auto outs = [&](std::string_view const &s){
		if (condition == COND_TRUE && comment_depth == 0) {
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

	condition_stack.push_back(COND_TRUE);
	UpdateCondition();

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
				UpdateCondition();
			}
		};
		auto EatNL = [&](){ // 改行を読み飛ばす
			if (ptr < end && *ptr == '\r') {
				ptr++;
				if (ptr < end && *ptr == '\n') {
					ptr++;
				}
				return;
			}
			if (ptr < end && *ptr == '\n') {
				ptr++;
				return;
			}
		};
		if (comment_depth > 0) {
			if (c == '{' && ptr + 1 < end && ptr[1] == '{') {
				comment_depth++;
				ptr += 2;
				continue;
			} else if (c == '}' && ptr + 1 < end && ptr[1] == '}') {
				comment_depth--;
				ptr += 2;
				continue;
			}
		}
		if (c == '{' && ptr + 4 < end && ptr[1] == '{' && ptr[2] == '.') {
			ptr += 3;
			if (ptr[0] == '}' && ptr[1] == '}') {
				// {{.}}
				ptr += 2;
				EatNL();
				END();
				continue;
			}

			if (*ptr == ';') { // {{.;comment}}
				comment_depth = 1;
				ptr++;
				continue;
			}

			enum class Directive {
				None,
				Raw,
				URL,
				HTML,
				Put,
				Define,
				Include,
				If,
				Ifn,
				Elif,
				Elifn,
				Else,
				End,
			} directive = Directive::None;

			if (*ptr == '#') {
				size_t i = 1;
				while (ptr + i < end && issym(ptr[i])) {
					i++;
				}
				std::string s = {ptr, i};
				if (s == "#raw") {
					directive = Directive::Raw;
				} else if (s == "#html") {
					directive = Directive::HTML;
				} else if (s == "#url") {
					directive = Directive::URL;
				} else if (s == "#put") {
					directive = Directive::Put;
				} else if (s == "#define") {
					directive = Directive::Define;
				} else if (s == "#include") {
					directive = Directive::Include;
				} else if (s == "#if") {
					directive = Directive::If;
				} else if (s == "#ifn") {
					directive = Directive::Ifn;
				} else if (s == "#elif") {
					directive = Directive::Elif;
				} else if (s == "#elifn") {
					directive = Directive::Elifn;
				} else if (s == "#else") {
					directive = Directive::Else;
				} else if (s == "#end") {
					directive = Directive::End;
				} else {
					fprintf(stderr, "unknown directive '%s'\n", s.data());
				}
				ptr += s.size();
			}

			auto ParseSymbol = [&](){
				size_t i = 0;
				while (ptr + i < end && ((i == 0) ? issymf(ptr[i]) : issym(ptr[i]))) {
					i++;
				}
				std::string s(ptr, i);
				ptr += i;
				return s;
			};

			std::string key;
			std::string value;
			std::vector<std::string> values;
			std::vector<std::vector<char>> vec;

			if (directive != Directive::None) {
				if (ptr < end) {
					bool keyflag = false;
					if (directive == Directive::Define || directive == Directive::Put) {
						keyflag = true;
						if (*ptr == '.') {
							ptr++;
							key = ParseSymbol();
						}
					}
					if (ptr < end) {
						if (*ptr == '(') {
							ptr++;
							vec = parse_string(ptr, end, ",", ")}", &map, &ptr);
							if (ptr < end && *ptr == ')') {
								ptr++;
							}
						} else if (directive == Directive::Define) {
							if (*ptr == '=' || isspace((unsigned char)*ptr)) {
								ptr++;
								std::vector<char> v;
								parse_string_raw(ptr, end, &ptr, &v);
								vec.push_back(v);
							}
						} else if (*ptr == '.') {
							ptr++;
							vec = parse_string(ptr, end, nullptr, "}", &map, &ptr);
						}
					}
					if (keyflag) {
						if (key.empty() && !vec.empty()) {
							key = to_string(vec[0]);
							vec.erase(vec.begin());
						}
					}
				}
			} else {
				vec = parse_string(ptr, end, nullptr, "}", &map, &ptr);
			}
			for (auto const &vec : vec) {
				values.emplace_back(to_string(vec));
			}
			if (ptr < end && *ptr == '}') {
				ptr++;
				if (ptr < end && *ptr == '}') {
					ptr++;
				}
			}

			if (!values.empty()) {
				value = values[0];
			}

			switch (directive) {
			case Directive::HTML: // {{.#html.foo}}
				outs(html_encode(value, true)); // output html encoded value
				break;
			case Directive::Raw: // {{.#raw.foo}}
				outs(value); // output raw value
				break;
			case Directive::URL: // {{.#url.foo}}
				outs(url_encode(value, true)); // output url encoded value
				break;
			case Directive::Define:
				if (!key.empty()) {
					if (value.empty()) {
						auto it = macro.find(key);
						if (it != macro.end()) {
							macro.erase(it);
						}
					} else {
						macro[key] = value;
					}
				} else {
					fprintf(stderr, "define name is empty\n");
				}
				EatNL();
				break;
			case Directive::Put:
				if (evaluator) {
					std::vector<std::string> args;
					for (size_t i = 0; i < values.size(); i++) {
						args.emplace_back(values[i]);
					}
					auto t = evaluator(key, {}, args);
					if (t) {
						std::string u = generate(*t, map);
						outs(u);
						break;
					}
				}
				{
					auto text = FindMacro(key);
					if (text) {
						outs(*text);
						break;
					}
				}
				fprintf(stderr, "undefined macro '%s'\n", key.data());
				outs(key);
				break;
			case Directive::Include:
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
				break;
			case Directive::If: // {{.#if.foo}}
				{
					auto v = atoi(value.data());
					condition_stack.push_back(v != 0 ? COND_TRUE : COND_FALSE);
					UpdateCondition();
				}
				break;
			case Directive::Ifn: // {{.#ifn.foo}} // if not
				{
					auto v = atoi(value.data());
					condition_stack.push_back(v == 0 ? COND_TRUE : COND_FALSE);
					UpdateCondition();
				}
				break;
			case Directive::Elif: // {{.#elif.foo}}
				if (condition_stack.size() < 2) {
					fprintf(stderr, "elif without if\n");
					break;
				}
				if (condition == COND_DONE) {
					// skip
				} else if (condition == COND_TRUE) {
					condition_stack.back() = COND_DONE;
				} else {
					auto v = atoi(value.data());
					condition_stack.back() = (v != 0 ? COND_TRUE : COND_FALSE);
				}
				UpdateCondition();
				break;
			case Directive::Elifn: // {{.#elifn.foo}}
				if (condition_stack.size() < 2) {
					fprintf(stderr, "elif without if\n");
					break;
				}
				if (condition == COND_DONE) {
					// skip
				} else if (condition == COND_TRUE) {
					condition_stack.back() = COND_DONE;
				} else {
					auto v = atoi(value.data());
					condition_stack.back() = (v == 0 ? COND_TRUE : COND_FALSE);
				}
				UpdateCondition();
				break;
			case Directive::Else: // {{.#else}}
				if (condition_stack.size() < 2) {
					fprintf(stderr, "else without if\n");
					break;
				}
				if (condition_stack.back() == COND_ELSE) {
					fprintf(stderr, "else after else\n");
					break;
				}
				if (condition == COND_FALSE) {
					condition_stack.back() = COND_ELSE;
				} else if (condition == COND_TRUE) {
					condition_stack.back() = COND_DONE;
				}
				UpdateCondition();
				break;
			case Directive::End: // {{.#end}}
				EatNL();
				END();
				break;
			default:
				if (key.empty()) { // {{.foo}}
					if (is_html_mode()) { // if html mode, output html encoded value
						html_encode(value, true);
					}
					outs(value);
				}
				break;
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
