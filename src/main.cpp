#include "processpool.h"

#define MAX_CONFIG_FILE 1024

using std::vector;

static const char* version = "1.0";

static void usage(const char* arg) {
    log(LOG_INFO, __FILE__, __LINE__,  "usage: %s [-h] [-v] [-f config_file]", arg);
}

int main(int argc, char* argv[]) {
    char configFile[MAX_CONFIG_FILE];
    memset(configFile, '\0', MAX_CONFIG_FILE);
    int option;
    while ((option = getopt(argc, argv, "f:xvh")) != -1) {
        switch (option) {
            case 'x': {
                setLogLevel(LOG_DEBUG);
                break;
            }
            case 'v': {
                log(LOG_INFO, __FILE__, __LINE__, "%s %s", argv[0], version);
                return 0;
            }
            case 'h': {
                usage(basename(argv[0]));
                return 0;
            }
            case 'f': {
                memcpy(configFile, optarg, strlen(optarg));
                break;
            } case '?': {
                log(LOG_ERR, __FILE__, __LINE__, "un-recognized option %c", option);
                usage(basename(argv[0]));
                return 1;
            }
        }
    }    

    if (configFile[0] == '\0') {
        log(LOG_ERR, __FILE__, __LINE__, "%s", "Please specifiy the config file");
        return 1;
    }

    int cfg_fd = open(configFile, O_RDONLY);
    if (!cfg_fd) {
        log(LOG_ERR, __FILE__, __LINE__, "Read config file met error: %s", strerror(errno));
        return 1;
    }

    struct stat ret_stat;
    if (fstat(cfg_fd, &ret_stat) < 0) {
        log(LOG_ERR, __FILE__, __LINE__, "Read config file met error: %s", strerror(errno));
        return 1;
    }

    char* buf = new char[ret_stat.st_size + 1];
    memset(buf, '\0', ret_stat.st_size + 1);
    ssize_t read_sz = read(cfg_fd, buf, ret_stat.st_size);
    if (read_sz < 0) {
        log(LOG_ERR, __FILE__, __LINE__, "Read config file met error: %s", strerror(errno));
        return 1;
    }

    vector<Host> center_server;
    vector<Host> logical_server;
    Host tmp_host;
    memset(tmp_host.hostName, '\0', HOST_NAME_SIZE);
    char* tmp_hostname;
    char* tmp_port;
    char* tmp_conncnt;
    bool opentag = false;
    char* tmp = buf;
    char* tmp2 = NULL;
    char* tmp3 = NULL;
    char* tmp4 = NULL;
    while (tmp2 = strpbrk(tmp, "\n")) {
        *tmp2++ = '\0';
        if (strstr(tmp, "<logical_host>")) {
            if (opentag) {
                log(LOG_ERR, __FILE__, __LINE__, "%s", "Parse config file failed");
                return 1;
            }
            opentag = true;
        } else if (strstr(tmp, "</logical_host>")) {
            if (!opentag) {
                log(LOG_ERR, __FILE__, __LINE__, "%s", "Parse config file failed");
                return 1;
            }
            logical_server.push_back(tmp_host);
            memset(tmp_host.hostName, '\0', HOST_NAME_SIZE);
            opentag = false;
        } else if (tmp3 = strstr(tmp, "<name>")) {
            tmp_hostname = tmp3 + 6;
            tmp4 = strstr(tmp_hostname, "</name>");
            if (!tmp4) {
                log(LOG_ERR, __FILE__, __LINE__, "%s", "Parse config file failed");
                return 1;
            }
            *tmp4 = '\0';
            memcpy(tmp_host.hostName, tmp_hostname, strlen(tmp_hostname));
        } else if (tmp3 = strstr(tmp, "<port>")) {
            tmp_port = tmp3 + 6;
            tmp4 = strstr(tmp_port, "</port>");
            if (!tmp4) {
                log(LOG_ERR, __FILE__, __LINE__, "%s", "Parse config file failed");
                return 1;
            }
            *tmp4 = '\0';
            tmp_host.port = atoi(tmp_port);
        } else if (tmp3 = strstr(tmp, "<conns>")) {
            tmp_conncnt = tmp3 + 7;
            tmp4 = strstr(tmp_conncnt, "</conns>");
            if (!tmp4) {
                log(LOG_ERR, __FILE__, __LINE__, "%s", "Parse config file failed");
                return 1;
            }
            *tmp4 = '\0';
            tmp_host.connectCount = atoi(tmp_conncnt);
        } else if (tmp3 = strstr(tmp, "Listen")) {
            tmp_hostname = tmp3 + 6;
            tmp4 = strstr(tmp_hostname, ":");
            if (!tmp4) {
                log(LOG_ERR, __FILE__, __LINE__, "%s", "Parse config file failed");
                return 1;
            }
            *tmp4++ = '\0';
            tmp_host.port = atoi(tmp4);
            memcpy(tmp_host.hostName, tmp3, strlen(tmp3));
            center_server.push_back(tmp_host);
            memset(tmp_host.hostName, '\0', HOST_NAME_SIZE);
        }
        tmp = tmp2;
    }

    if (center_server.size() == 0 || logical_server.size() == 0) {
        log(LOG_ERR, __FILE__, __LINE__, "%s", "Parse config file failed");
        return 1;
    }
    const char* ip = center_server[0].hostName;
    int port = center_server[0].port;

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
 
    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

	g_processpool = processpool :: create(listenfd, logical_server.size());
    if (g_processpool) {
        g_processpool->run(logical_server);
        delete g_processpool;
    }

    close(listenfd);
    return 0;
}
