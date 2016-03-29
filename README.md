# Multi-GET

This app uses cURL and native C++11 Threading support.

**Usage**:

multi-get -u|--url URL [**OPTIONS**]

Option for URL is mandatory. Other supported options are:

* -o, --ofile=*filename* : 
    Downloaded file will be named *filename*. Default is requested file name in URL + chunknumber + totalsize.
    
* -t, --threads=*num* : File will be downloaded in *num* chunks. Default is 4.
    
* -b, --total=*bytes* : Total download size will be *bytes* bytes. *bytes* can end with *k*|*K*, *M* or *G*, representing kilo-, mega-, giga- bytes respectively
    
* -h, --help :
        Print a short help message

Examples:

    multi-get --url some-link --thread=4 --total=4MB
    multi-get --u some-link -o outname -b 4096
