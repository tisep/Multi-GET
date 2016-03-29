#include <curl/curl.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <thread>
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <fstream>

// Const Declarations
constexpr size_t MB = 1<<20;
constexpr size_t NUM_WORKER_THREAD = 4;
const char *helpMSG = 
"Usage: multi-get -u|--url URL [OPTIONS]\n\
OPTIONS:\n\
-o, --ofile=OUTFILE\n\
-t, --threads=NUM_THREADS\n\
-b, --total=NUM_BYTES\n\
-h, --help\n";

// Class Definition
struct ClientReq {
    std::string url, ofile;
    size_t total_size = 0, chunks = 0, chunk_size = 0;
};

// Function Prototypes
int init(int argc, char *argv[], ClientReq&);
size_t CopyChunk(char *ptr, size_t size, size_t nmemb, void *userdata);
void GetChunk(const ClientReq &cli, int i, std::vector<char> &chunk);
size_t StoreHeader(char *ptr, size_t size, size_t nmemb, void *userdata);
size_t GetFileSize(const std::string &url, std::string &header);

int main(int argc, char *argv[])
{
    ClientReq client;
    int retCode = init(argc, argv, client);

    if (retCode)
        return retCode;

    std::vector<char> chunks[client.chunks];
    std::vector<std::thread> worker_threads(client.chunks-1);

    // Start worker threads
    for (int i = 0; i < client.chunks-1; ++i)
    {
        chunks[i].reserve(client.chunk_size);
        worker_threads[i] = std::thread(GetChunk, std::ref(client), i, std::ref(chunks[i]));
    }
    chunks[client.chunks-1].reserve(client.chunk_size);
    GetChunk(client, client.chunks-1, chunks[client.chunks-1]);

    // Join each thread until they all finish.
    std::for_each(worker_threads.begin(), worker_threads.end(), std::mem_fn(&std::thread::join));
    std::ofstream ofile(client.ofile, std::ofstream::binary);
    if (ofile) {
        for (int i = 0; i < client.chunks; ++i)
            ofile.write(&chunks[i][0], chunks[i].size());
    }
    else {
        std::cerr << "Failed to open file " << client.ofile << " for write\n";
        return 1;
    }

    return 0;
}

int init(int argc, char *argv[], ClientReq &cli)
{
    struct option longOpts[] = {
        {"url", required_argument, NULL, 'u'},
        {"ofile", required_argument, NULL, 'o'},
        {"threads", required_argument, NULL, 't'},
        {"total", required_argument, NULL, 'b'},
        {"help", no_argument, NULL, 'h'},
        {0,0,0,0}};

    int opt;
    while ((opt = getopt_long(argc, argv, "u:o:ht:b:", longOpts, NULL)) != -1) {
        size_t arg_len;
        switch (opt) {
            case 'u':
                cli.url = optarg;
                break;
            case 'o':
                cli.ofile = optarg;
                break;
            case 't':
                cli.chunks = atoll(optarg);
                break;
            case 'b':
                cli.total_size = atoll(optarg);
                arg_len = strlen(optarg);
                switch (optarg[arg_len-1]) {
                    case 'k':
                    case 'K':
                        cli.total_size <<= 10;
                        break;
                    case 'M':
                        cli.total_size <<= 20;
                        break;
                    case 'G':
                        cli.total_size <<= 30;
                        break;
                }
                break;
            case 'h':
                std::cerr << helpMSG;
                return 0;
            default:
                std::cerr << helpMSG;
                return 1;
        }
    }

    if (cli.url == "") {
        std::cerr << "URL option is mandatory\n" << helpMSG;
        return 1;
    }

    std::string header;
    auto content_len = GetFileSize(cli.url, header);
#if defined(DEBUG)
    std::cerr << content_len << "\n";
#endif
    if (content_len == 0) {
        std::cerr << "Content-Length unknown\n";
        return 1;
    }
    if (cli.total_size == 0)
        cli.total_size = NUM_WORKER_THREAD * MB;
    if (cli.total_size > content_len)
        cli.total_size = content_len;

    if (cli.chunks == 0) {
        cli.chunks = NUM_WORKER_THREAD;
        std::cerr << "Default " << NUM_WORKER_THREAD << " threads\n";
    }

    if (cli.ofile == "") {
        auto ind = cli.url.find_last_of('/');
        if (ind != std::string::npos && ind != cli.url.size()-1)
            cli.ofile = cli.url.substr(ind+1) + "_chunk=" + std::to_string(cli.chunks) + "_total=" + std::to_string(cli.total_size);
        std::cerr << "Default output file \"" << cli.ofile << "\"\n";
    }

    cli.chunk_size = cli.total_size / cli.chunks + (cli.total_size % cli.chunks != 0);

    return 0;
}

size_t copy_chunk(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    auto tot_size = size * nmemb;
    auto vp = static_cast<std::vector<char> *>(userdata);
    auto prev_size = vp->size();

    vp->resize(prev_size + tot_size);
    memcpy(&((*vp)[0]) + prev_size, ptr, tot_size);

    return tot_size;
}

void GetChunk(const ClientReq &cli, int i, std::vector<char> &chunk)
{
    char errbuf[CURL_ERROR_SIZE] = {0};
    CURLcode ret;
    CURL *hnd;

    hnd = curl_easy_init();
    curl_easy_setopt(hnd, CURLOPT_URL, cli.url.data()); // "http://ca5d20d0.bwtest-aws.pravala.com/384MB.jar"
    curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, copy_chunk);
    curl_easy_setopt(hnd, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(hnd, CURLOPT_ERRORBUFFER, errbuf);

    size_t start = i*cli.chunk_size;
    size_t end = (i == cli.chunks-1)? cli.total_size-1 : start+cli.chunk_size-1;
    std::string numstr = std::to_string(start) + "-" + std::to_string(end);

#if defined(DEBUG)
    std::cerr << "Working on range: " << numstr << "\n";
#endif
    // Range GET
    curl_easy_setopt(hnd, CURLOPT_RANGE, numstr.data());
    ret = curl_easy_perform(hnd);
    if (ret != CURLE_OK) {
        size_t len = strlen(errbuf);
        std::cerr << "\nlibcurl: (" << ret << ") ";
        if (len)
            std::cerr << errbuf << ((errbuf[len-1] != '\n')? "\n" : "");
        else
            std::cerr << curl_easy_strerror(ret) << "\n";
    }

    curl_easy_cleanup(hnd);
    hnd = nullptr;
}

size_t StoreHeader(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    auto sp = static_cast<std::string*>(userdata);
    auto strlen = size * nmemb;
    (*sp) += std::string(ptr, strlen);

    return strlen;
}

size_t GetFileSize(const std::string &url, std::string &header)
{
    char errbuf[CURL_ERROR_SIZE] = {0};
    CURLcode ret;
    CURL *hnd;

    hnd = curl_easy_init();
    curl_easy_setopt(hnd, CURLOPT_URL, url.data());
    curl_easy_setopt(hnd, CURLOPT_HEADERFUNCTION, StoreHeader);
    curl_easy_setopt(hnd, CURLOPT_HEADERDATA, &header);
    curl_easy_setopt(hnd, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(hnd, CURLOPT_NOBODY, 1L);

    ret = curl_easy_perform(hnd);
    if (ret != CURLE_OK) {
        size_t len = strlen(errbuf);
        std::cerr << "\nlibcurl: (" << ret << ") ";
        if (len)
            std::cerr << errbuf << ((errbuf[len-1] != '\n')? "\n" : "");
        else
            std::cerr << curl_easy_strerror(ret) << "\n";
    }

    curl_easy_cleanup(hnd);
    hnd = NULL;

    std::string search_key("Content-Length:");
    auto ind = header.find(search_key);
    size_t file_size = 0;
    if (ind != std::string::npos)
    {
        ind += search_key.size();
        auto end_line = header.find_first_of('\n', ind);
        file_size = std::stoull(header.substr(ind, end_line-ind));
    }

    return file_size;
}


