# Multi-GET

This app uses cURL and native C++11 Threading support.

Usage:

    multi-get -u|--url URL [OPTIONS]

    Option for URL is mandatory. Other supported options are:

    -o, --ofile=**filename**
        Downloaded file will be named **filename**
    -t, --threads=**num**
        File will be downloaded in **num** chunks
    -b, --total=**bytes**
        Total download size will be **bytes" bytes. **bytes** can end with k, K, M or G, representing kilo-, mega-, giga- bytes respectively.
    -h, --help
        Print a short help message

