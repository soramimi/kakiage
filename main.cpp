
#include "kakiage.h"
#include <map>
#include <stdio.h>
#include <cstring>
#include <string_view>
#include <optional>
#include "webclient.h"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <WinSock2.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#define O_BINARY 0
#endif

// #include <curl/curl.h>

#define PROGRAM_NAME "kakiage"

#define VERSION "0.0.0"

std::optional<std::string> inet_checkip_cache;
std::map<std::string, std::string> inet_resolve_cache;

/**
 * @brief inet_resolve
 * @param name
 * @return
 */
std::string inet_resolve(std::string const &name)
{
	auto it = inet_resolve_cache.find(name);
	if (it == inet_resolve_cache.end()) {
#if defined(_WIN32) || defined(__APPLE__)
		struct hostent *he = nullptr;
		he = ::gethostbyname(name.c_str());
		if (!he) return {};
		auto a = inet_ntoa(*(in_addr *)he->h_addr_list[0]);
#elif 0
		gethostbyname_r(name, &tmp, buf, sizeof(buf), &he, &err);
#else
		struct addrinfo hints, *res;
		struct in_addr addr;
		memset(&hints, 0, sizeof(hints));
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_family = AF_INET;
		auto err = getaddrinfo(name.c_str(), nullptr, &hints, &res);
		if (err) return {};
		addr.s_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr;
		freeaddrinfo(res);
		auto a = inet_ntoa(addr);
#endif
		it = inet_resolve_cache.insert(inet_resolve_cache.end(), std::make_pair(name, a));
	}
	return it->second;
}

#if 0
/**
 * @brief libcurlのコールバック関数
 * @param[in] contents 受信したデータ
 * @param[in] size 受信したデータのサイズ
 * @param[in] nmemb 受信したデータの個数
 * @param[in] userp ユーザーポインタ
 * @return 受信したデータのサイズ
 */
static size_t _write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
	std::vector<char> *out = (std::vector<char> *)userp;
	size_t realsize = size * nmemb;
	char const *begin = (char const *)contents;
	char const *end = begin + realsize;
	out->insert(out->end(), begin, end);
	return realsize;
}
#endif

/**
 * @brief libcurlの初期化済みフラグ
 */
static bool _curl_global_initialized = false;

/**
 * @brief libcurlを初期化する
 */
void initialize_curl()
{
	if (!_curl_global_initialized) {
		_curl_global_initialized = true;
#if 0
		curl_global_init(CURL_GLOBAL_ALL);
#else
#ifdef _WIN32
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
#endif
	}
}

/**
 * @brief libcurlを終了する
 */
void finalize_curl()
{
	if (_curl_global_initialized) {
		_curl_global_initialized = false;
#if 0
		curl_global_cleanup();
#else
#ifdef _WIN32
		WSACleanup();
#endif
#endif
	}
}

/**
 * @brief ファイルを読み込む
 * @param[in] path ファイルパス
 * @return ファイルの内容
 */
std::optional<std::string> readfile(char const *path)
{
	std::vector<char> vec;
	int fd = open(path, O_RDONLY | O_BINARY);
	if (fd < 0) return std::nullopt;
	vec.clear();
	while (true) {
		char buf[65536];
		int r = read(fd, buf, sizeof(buf));
		if (r <= 0) break;
		vec.insert(vec.end(), buf, buf + r);
	}
	close(fd);
	return std::string(vec.begin(), vec.end());
}

void parseConfigFile(char const *path, std::map<std::string, std::string> *map)
{
	map->clear();
	auto rules = readfile(path);
	if (!rules) {
		fprintf(stderr, "Failed to open definition file: %s\n", path);
	}
	if (!rules->empty()){
		char const *begin = rules->data();
		char const *end = begin + rules->size();
		char const *line = begin;
		char const *endl = begin;
		char const *eq = nullptr;
		char const *comment = nullptr;
		int linenum = 0;
		while (endl < end) {
			int c = endl < end ? (unsigned char)*endl : -1;
			if (c == '\n' || c == '\r' || c == -1) {
				char const *left = line;
				char const *right = endl;
				if (comment && comment < right) {
					right = comment;
				}
				while (left < right && isspace((unsigned char)*left)) left++;
				while (left < right && isspace((unsigned char)right[-1])) right--;
				if (left < right) {
					if (eq) {
						std::string name(left, eq);
						std::string value(eq + 1, right);
						(*map)[name] = value;
					} else {
						std::string s(line, endl);
						fprintf(stderr, "Syntax error (%d): %s\n", linenum + 1, s.c_str());
					}
				}
				if (c < 0) break;
				comment = nullptr;
				eq = nullptr;
				line = ++endl;
				linenum++;
			} else if (c == '=') {
				if (!eq && !comment) {
					eq = endl;
				}
				endl++;
			} else if (c == '#' || c == ';') {
				if (!comment) {
					comment = endl;
				}
				endl++;
			} else {
				endl++;
			}
		}
	}
}

struct TestCase {
	char const *source;
	char const *expected;
};

TestCase testcases[] = {

	// 0
	{ "{{.#put.inet_checkip}}"
	  , "14.3.142.77" },

	// 1
	{ "{{.#put('inet_checkip'}}"
	  , "14.3.142.77" },

	// 2
	{ "{{.#put.inet_resolve(\"a.root-servers.net\")}}"
	  , "198.41.0.4" },

	// 3
	{ "{{.#put(\"inet_resolve\", \"a.root-servers.net\")}}"
	  , "198.41.0.4" },

	// 4
	{ "{{.#url(<test.txt>)}}"
	  , "%3Cspan%3E%7Bcopyright%7D%3C%2Fspan%3E" },

	// 5
	{ "{{.#url(\"<test.txt>\")}}"
	  , "%3Ctest.txt%3E" },

	// 6
	{ "{{.#html(<test.txt>)}}"
	  , "&lt;span&gt;{copyright}&lt;/span&gt;" },

	// 7
	{ "{{.#html(\"<test.txt>\")}}"
	  , "&lt;test.txt&gt;" },

	// 8
	{ "{{.#include.\"test.txt\"}}"
	  , "<span>{copyright}</span>" },

	// 9
	{ "{{.#include(\"test.txt\")}}"
	  , "<span>{copyright}</span>" },

	// 10
	{ "name = {{.name}}"
	  , "name = Taro" },

	// 11
	{ "age = {{.age}}"
	  , "age = 24" },

	// 12
	{ "{{.#raw.name}}"
	  , "Taro" },

	// 13
	{ "{{.#raw(name)}}"
	  , "Taro" },

	// 14
	{ "{{.`uname`}}"
	  , "Linux" },

	// 15
	{ "{{.<test.txt>}}"
	  , "<span>{copyright}</span>" },

	// 16
	{ "{{.$(SHELL)}}"
	  , "/bin/bash" },

	// 17
	{ "{{.#define.hoge=fuga}}{{.#put.hoge}}"
	  , "fuga" },

	// 18
	{ "{{.#define.hoge fuga}}{{.#put.hoge}}"
	  , "fuga" },

	// 19
	{ "{{.#define.hoge  fuga}}{{.#put.hoge}}"
	  , "fuga" },

	// 20
	{ "{{.#define('hoge','fuga')}}{{.#put.hoge}}"
	  , "fuga" },

	// 21
	{ "{{.#define('hoge', 'fuga')}}{{.#put.hoge}}"
	  , "fuga" },

	// 22
	{ "{{.#define(\"hoge\",\"fuga\")}}{{.#put.hoge}}"
	  , "fuga" },

	// 23
	{ "{{.#define(\"hoge\", \"fuga\")}}{{.#put.hoge}}"
	  , "fuga" },

	// 24
	{ "{{.#define.hoge=fuga}}{{.#put('hoge')}}"
	  , "fuga" },

	// 25
	{ "{{.#define.hoge fuga}}{{.#put(\"hoge\")}}"
	  , "fuga" },

	// 26
	{ "{{.#define.hoge fuga}}{{.#put(hoge)}}" // put(hoge) は間違い。put('hoge') が正しい。
	  , "?hoge?" },

	// 27
	{ "{{.#define.hoge={{.'fuga'}}}}{{.#put.hoge}}"
	  , "{{.'fuga'}}" },

	// 28
	{ "{{.#define.hoge {{.'{{.'piyo}}'}}}}{{.#put.hoge}}"
	  , "{{.'{{.'piyo}}'}}" },

	// 29
	{ "({{.#if.1}}foo{{.#else}}bar{{.}})"
	  , "(foo)" },

	// 30
	{ "({{.#if.0}}foo{{.#else}}bar{{.}})"
	  , "(bar)" },

	// 31
	{ "({{.#if.1}}foo{{.#else}}bar{{.#end}})" // #end は冗長だけど正しい文法
	  , "(foo)" },

	// 32
	{ "({{.#if.0}}foo{{.#else}}bar{{.#end}})"
	  , "(bar)" },

	// 33
	{ "(&&.{};)"
	  , "(&.{})" },

	// 34
	{ "(&&&&&;)"
	  , "(&&&&)" },

	// 35
	{ "a&{;b&{{;c&{{{;d"
	  , "a{b{{c{{{d" },

	// 36
	{ ";a&{{&b.}};;c;"
	  , ";a{{&b.}};c;" },
};
static const int testcase_count = sizeof(testcases) / sizeof(testcases[0]);

kakiage st;

int testmain()
{
	std::string input_text;
	std::map<std::string, std::string> map;
	auto file = readfile("test.in");
	if (!file) {
		fprintf(stderr, "Failed to open input file: test.in\n");
		return 1;
	}

	input_text = *file;
	map.clear();
	parseConfigFile("test.ka", &map);

	int passed = 0;
	int failed = 0;
	for (int i = 0; i < testcase_count; i++) {
		auto source = testcases[i].source;
		fprintf(stderr, "[%d] %s\n", i, source);
		std::string result = st.generate(source, map);
		if (result == testcases[i].expected) {
			passed++;
		} else {
			fprintf(stderr, "Test failed: %s\n", source);
			fprintf(stderr, "  expected: %s\n", testcases[i].expected);
			fprintf(stderr, "    result: %s\n", result.c_str());
			failed++;
		}
	}
	fprintf(stderr, "Passed: %d\n", passed);
	fprintf(stderr, "Failed: %d\n", failed);

	return 0;
}

//
int main(int argc, char **argv)
{
	st.set_html_mode(false);

	std::string source_path;
	std::string output_path;
	std::string input_text;

	std::map<std::string, std::string> map;

	bool help = false;
	bool test = false;

	int i = 1;
	while (i < argc) {
		char const *arg = argv[i++];
		if (arg[0] == '-') {
			auto IsArg = [&](char const *name)->bool{
				return strcmp(arg, name) == 0;
			};
			if (IsArg("-h") || IsArg("--help")) {
				help = true;
			} else if (IsArg("-d")) {
				if (i < argc) {
					parseConfigFile(argv[i++], &map);
				} else {
					fprintf(stderr, "Too few arguments\n");
				}
			} else if (strncmp(arg, "-D", 2) == 0) {
				if (arg[2] == 0) {
					if (i < argc) {
						std::string a = argv[i++];
						size_t p = a.find('=');
						if (p != std::string::npos) {
							std::string name = a.substr(0, p);
							std::string value = a.substr(p + 1);
							map[name] = value;
						} else {
							fprintf(stderr, "Syntax error: %s\n", a.c_str());
						}
					} else {
						fprintf(stderr, "Too few arguments\n");
					}
				} else {
					std::string a = arg + 2;
					size_t p = a.find('=');
					if (p != std::string::npos) {
						std::string name = a.substr(0, p);
						std::string value = a.substr(p + 1);
						map[name] = value;
					} else {
						fprintf(stderr, "Syntax error: %s\n", a.c_str());
					}
				}
			} else if (IsArg("-s")) {
				if (i < argc) {
					if (input_text.empty()) {
						input_text = argv[i++];
					} else {
						fprintf(stderr, "Too many arguments\n");
					}
				} else {
					fprintf(stderr, "Too few arguments\n");
				}
			} else if (IsArg("-o")) {
				if (i < argc) {
					output_path = argv[i++];
				} else {
					fprintf(stderr, "Too few arguments\n");
				}
			} else if (IsArg("--html")) {
				st.set_html_mode(true);
			} else if (IsArg("--test")) {
				test = true;
			} else {
				fprintf(stderr, "Unknown option: %s\n", arg);
			}
		} else {
			if (source_path.empty()) {
				source_path = arg;
			} else {
				fprintf(stderr, "Too many arguments\n");
			}
		}
	}

	st.evaluator = [&](std::string const &name, std::string const &text, std::vector<std::string> const &args)->std::optional<std::string>{
		if (name == "inet_resolve") { // inet_resolve("example.com")
			if (args.size() != 1) return {};
			return inet_resolve(args[0]);
		}
		if (name == "inet_checkip") { // my global ip address
			(void)args;
			return WebClient::checkip();
		}
		return std::nullopt;
	};
	st.includer = [&](std::string const &name)->std::optional<std::string>{
		if (name == "test.txt") {
			return "<span>{copyright}</span>";
		}
		return std::nullopt;
	};

	if (test) {
		return testmain();
	}

	if (source_path.empty()) {
		if (input_text.empty()) {
			help = true;
		}
	} else if (input_text.empty()) {
		auto file = readfile(source_path.c_str());
		if (file) {
			input_text = *file;
		} else {
			fprintf(stderr, "Failed to open input file: %s\n", source_path.c_str());
		}
	} else {
		help = true;
	}

	if (help) {
		fprintf(stderr, "%s %s\n", PROGRAM_NAME, VERSION);
		fprintf(stderr, "Usage: %s [options] (<input file> | -s <input text>)\n", PROGRAM_NAME);
		fprintf(stderr, "Options:\n");
		fprintf(stderr, "  -d <definision file>\n");
		fprintf(stderr, "  -D <name>=<value>\n");
		fprintf(stderr, "  -o <output file>\n");
		fprintf(stderr, "  -s <input text>\n");
		fprintf(stderr, "  --html\n");
		return 0;
	}

	std::string result = st.generate(input_text, map);

	FILE *fp;
	if (!output_path.empty()) {
		fp = fopen(output_path.c_str(), "w");
		if (!fp) {
			fprintf(stderr, "Failed to open output file: %s\n", output_path.c_str());
			return 1;
		}
		fwrite(result.data(), 1, result.size(), fp);
		fclose(fp);
	} else {
		fwrite(result.data(), 1, result.size(), stdout);
	}

	finalize_curl();

	return 0;
}
