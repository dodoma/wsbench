#include "reef.h"

char* http_request_build(const char *url)
{
    MSTR str;

    mstr_init(&str);

    mstr_appendf(&str, "GET %s HTTP/1.1\r\n\r\n\r\n", url);

    return str.buf;
}

#if 0
    static char *s = "GET /socket.io/?EIO=3&transport=polling&t=Lw4ZgLo HTTP/1.1\r\n"
        "Host: 127.0.0.1:3950\r\n"
        "Connection: keep-alive\r\n"
        "Pragma: no-cache\r\n"
        "Cache-Control: no-cache\r\n"
        "Accept: */*\r\n"
        "Origin: http://ddz-dev.pkgame.net\r\n"
        "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_12_2) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/63.0.3216.0 Safari/537.36\r\n"
        "Referer: http://ddz-dev.pkgame.net/?invoker=ddz&ticket=3309BU9BWOAILXGK7823&uuid=736bc9635dc57a77851459b9d5215371&roomid=&_=1505462404753\r\n"
        "Accept-Encoding: gzip, deflate, br\r\n"
        "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8\r\n"
        "\r\n"
        "\r\n";

    return s;
#endif
