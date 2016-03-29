#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include <unistd.h>
#include <getopt.h>

const char *helpMSG = "Usage: multi-get --url|-u URL [--ofile|-o OUTFILE]\n";

int init(int argc, char *argv[], char **url, char **filename, FILE **fp);
int getChunks(char *url, FILE *fp);

int main(int argc, char *argv[])
{
    char *url = NULL;
    char *filename = NULL;
    FILE* fp = NULL;

    int retCode = 0;

    retCode = init(argc, argv, &url, &filename, &fp);
    // Init is successful
    if (retCode == 0)
        retCode = getChunks(url, fp);

    if (fp)
        fclose(fp);
    free(filename);
    free(url);

    return retCode;
}

int init(int argc, char *argv[], char **url, char **filename, FILE **fp)
{
    struct option longOpts[] = {
        {"url", required_argument, NULL, 'u'},
        {"ofile", required_argument, NULL, 'o'},
        {"help", no_argument, NULL, 'h'},
        {0,0,0,0}};

    int opt;
    size_t urlLen = 0, fileNameLen = 0;
    while ((opt = getopt_long(argc, argv, "u:o:h", longOpts, NULL)) != -1) {
        switch (opt) {
            case 'u':
                urlLen = strlen(optarg)+1;
                *url = (char*)malloc(urlLen);
                if (*url == NULL) {
                    fprintf(stderr, "Failed to allocate memory for URL\n");
                    return (1);
                }
                strncpy(*url, optarg, urlLen);
                break;
            case 'o':
                fileNameLen = strlen(optarg)+1;
                *filename = (char*)malloc(fileNameLen);
                if (*filename == NULL) {
                    fprintf(stderr, "Failed to allocate memory for output filename. Default to \"first4MB\"\n");
                }
                else {
                    strncpy(*filename, optarg, fileNameLen);
                }
                break;
            case 'h':
                fprintf(stderr, "%s", helpMSG);
                return (2);
            default:
                fprintf(stderr, "%s", helpMSG);
                return (1);
        }
    }

    if (*url == NULL) {
        fprintf(stderr, "URL option is mandatory\n%s", helpMSG);
        return (1);
    }

    const char *ofile = *filename ? *filename : "first4MB";
    *fp = fopen(ofile, "wb");
    if (*fp == NULL) {
        fprintf(stderr, "Failed to open file %s for write\n", ofile);
        return (1);
    }

    return 0;
}

int getChunks(char *url, FILE *fp)
{
    char errbuf[CURL_ERROR_SIZE] = {0};
    CURLcode ret;
    CURL *hnd;

    hnd = curl_easy_init();
    curl_easy_setopt(hnd, CURLOPT_URL, url); // "http://ca5d20d0.bwtest-aws.pravala.com/384MB.jar"
    curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(hnd, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(hnd, CURLOPT_ERRORBUFFER, errbuf);

    // 1MB per chunk
    const size_t chunkSize = 1<<20;
    int totalChunk = 4;

    for (int i = 0; i < totalChunk; ++i) {
        size_t start = i*chunkSize;
        size_t end = start+chunkSize-1;
#define MAX_LEN_NUM 24
        char numstr[MAX_LEN_NUM] = {0};
        snprintf(numstr, MAX_LEN_NUM, "%zu-%zu", start, end);
#if defined(DEBUG)
        printf("Working on range: %s\n", numstr);
#endif

        // Range GET
        curl_easy_setopt(hnd, CURLOPT_RANGE, numstr);
        ret = curl_easy_perform(hnd);
        if (ret != CURLE_OK) {
            size_t len = strlen(errbuf);
            fprintf(stderr, "\nlibcurl: (%d) ", ret);
            if (len)
                fprintf(stderr, "%s%s", errbuf, ((errbuf[len-1] != '\n')? "\n" : ""));
            else
                fprintf(stderr, "%s\n", curl_easy_strerror(ret));

            break;
        }
    }

    curl_easy_cleanup(hnd);
    hnd = NULL;

    return (int)ret;
}


