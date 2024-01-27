
// ted: Template EDitor

#include "strtemplate.h"
#include <map>
#include <stdio.h>
#include <cstring>
#include <string_view>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
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

#include <curl/curl.h>

/**
 * @brief inet_resolve
 * @param name
 * @return
 */
std::string inet_resolve(std::string const &name)
{
	struct addrinfo hints, *res;
	struct in_addr addr;
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_INET;
	auto err = getaddrinfo(name.c_str(), nullptr, &hints, &res);
	if (err) return {};
	addr.s_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr;
	freeaddrinfo(res);
	return inet_ntoa(addr);
}

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
		curl_global_init(CURL_GLOBAL_ALL);
	}
}

/**
 * @brief libcurlを終了する
 */
void finalize_curl()
{
	if (_curl_global_initialized) {
		_curl_global_initialized = false;
		curl_global_cleanup();
	}
}

/**
 * @brief 現在のグローバルIPアドレスを取得する
 * @return IPアドレス
 */
std::string inet_checkip()
{
	std::vector<char> buffer;

	CURL *curl;
	CURLcode res;

	// libcurl を初期化
	initialize_curl();

	// ハンドルを作成
	curl = curl_easy_init();
	if (curl) {
		// URL を設定
		curl_easy_setopt(curl, CURLOPT_URL, "http://checkip.amazonaws.com");

		// データの受信先を設定
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _write_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&buffer);

		// リクエストを実行し、エラーがあれば処理
		res = curl_easy_perform(curl);
		if (res != CURLE_OK) {
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		}

		// ハンドルをクリーンアップ
		curl_easy_cleanup(curl);
	}

	// 受信したデータを文字列に変換
	char const *begin = buffer.data();
	char const *end = begin + buffer.size();
	while (begin < end && isspace((unsigned char)*begin)) begin++;
	while (begin < end && isspace((unsigned char)end[-1])) end--;
	return std::string(begin, end);
}

/**
 * @brief ファイルを読み込む
 * @param[in] path ファイルパス
 * @return ファイルの内容
 */
std::string readfile(char const *path)
{
	std::vector<char> vec;
	int fd = open(path, O_RDONLY | O_BINARY);
	if (fd < 0) return {};
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

//
int main(int argc, char **argv)
{
	std::string source_path;
	std::string template_path;
	int i = 1;
	while (i < argc) {
		char const *arg = argv[i++];
		if (arg[0] == '-') {
			if (strcmp(arg, "-t") == 0) {
				if (i < argc) {
					template_path = argv[i++];
				} else {
					fprintf(stderr, "Too few arguments\n");
				}
			}
		} else {
			if (source_path.empty()) {
				source_path = arg;
			} else {
				fprintf(stderr, "Too many arguments\n");
			}
		}
	}

	std::string source = readfile(source_path.c_str());
	std::string templ = readfile(template_path.c_str());
	std::map<std::string, std::string> map;
	{
		char const *begin = templ.c_str();
		char const *end = begin + templ.size();
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
						map[name] = value;
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
				comment = endl;
				endl++;
			} else {
				endl++;
			}
		}
	}

	strtemplate st;
	st.evaluator = [&](std::string const &name, std::string const &arg)->std::string{
		if (name == "inet_resolve") {
			return inet_resolve(arg);
		}
		if (name == "inet_checkip") {
			return inet_checkip();
		}
		return {};
	};
	std::string result = st.generate(source, map);
	puts(result.c_str());

	finalize_curl();

	return 0;
}
