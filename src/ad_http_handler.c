#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>
#include <event2/buffer.h>
#include "macro.h"
#include "qlibc.h"
#include "ad_server.h"
#include "ad_http_handler.h"

#ifndef _DOXYGEN_SKIP
static ad_http_t *http_new(struct evbuffer *out);
static void http_free(ad_http_t *http);
static void http_free_cb(ad_conn_t *conn, void *userdata);
static size_t http_add_inbuf(struct evbuffer *buffer, ad_http_t *http, size_t maxsize);

static int http_parser(ad_http_t *http, struct evbuffer *in);
static int parse_requestline(ad_http_t *http, char *line);
static int parse_headers(ad_http_t *http, struct evbuffer *in);
static int parse_body(ad_http_t *http, struct evbuffer *in);

static bool isValidPathname(const char *path);
static void correctPathname(char *path);
#endif

int ad_http_handler(short event, ad_conn_t *conn, void *userdata) {
    if (event & AD_EVENT_INIT) {
        DEBUG("==> HTTP INIT");
        ad_http_t *http = http_new(conn->out);
        if (http == NULL) return AD_CLOSE;
        ad_conn_set_extra(conn, http, http_free_cb);
        return AD_OK;
    } else if (event & AD_EVENT_READ) {
        DEBUG("==> HTTP READ");
        return http_parser((ad_http_t *)ad_conn_get_extra(conn), conn->in);
    } else if (event & AD_EVENT_WRITE) {
        DEBUG("==> HTTP WRITE");
        return AD_OK;
    } else if (event & AD_EVENT_CLOSE) {
        DEBUG("==> HTTP CLOSE=%x (TIMEOUT=%d, SHUTDOWN=%d)",
              event, event & AD_EVENT_TIMEOUT, event & AD_EVENT_SHUTDOWN);
        return AD_OK;
    }

    BUG_EXIT();
}

enum ad_http_request_status_e ad_http_get_status(ad_conn_t *conn) {
    ad_http_t *http = (ad_http_t *)ad_conn_get_extra(conn);
    if (http == NULL) return AD_HTTP_ERROR;
    return http->request.status;
}

struct evbuffer *ad_http_get_inbuf(ad_conn_t *conn) {
    ad_http_t *http = (ad_http_t *)ad_conn_get_extra(conn);
    return http->request.inbuf;
}

struct evbuffer *ad_http_get_outbuf(ad_conn_t *conn) {
    ad_http_t *http = (ad_http_t *)ad_conn_get_extra(conn);
    return http->response.outbuf;
}

const char *ad_http_get_request_header(ad_conn_t *conn, const char *name) {
    ad_http_t *http = (ad_http_t *)ad_conn_get_extra(conn);
    return http->request.headers->getstr(http->request.headers, name, false);
}

off_t ad_http_get_content_length(ad_conn_t *conn) {
    ad_http_t *http = (ad_http_t *)ad_conn_get_extra(conn);
    return http->request.contentlength;
}

/**
 * @param maxsize maximum length of data to pull up. 0 to pull up everything.
 */
void *ad_http_get_content(ad_conn_t *conn, size_t maxsize, size_t *storedsize) {
    ad_http_t *http = (ad_http_t *)ad_conn_get_extra(conn);

    size_t inbuflen = evbuffer_get_length(http->request.inbuf);
    size_t readlen = (maxsize == 0) ? inbuflen : ((inbuflen < maxsize) ? inbuflen : maxsize);
    if (readlen == 0) return NULL;

    void *data = malloc(readlen);
    if (data == NULL) return NULL;

    size_t removedlen = evbuffer_remove(http->request.inbuf, data, readlen);
    if (storedsize) *storedsize = removedlen;

    return data;
}

/**
 * @param value value string to set. NULL to remove the header.
 * @return 0 on success, -1 if we already sent it out.
 */
int ad_http_add_response_header(ad_conn_t *conn, const char *name, const char *value) {
    ad_http_t *http = (ad_http_t *)ad_conn_get_extra(conn);
    if (http->response.frozen_header) {
        return -1;
    }

    if (value != NULL) {
        http->response.headers->putstr(http->response.headers, name, value);
    } else {
        http->response.headers->remove(http->response.headers, name);
    }

    return 0;
}

/**
 *
 * @return 0 on success, -1 if we already sent it out.
 */
int ad_http_set_response_code(ad_conn_t *conn, int code, const char *reason) {
    ad_http_t *http = (ad_http_t *)ad_conn_get_extra(conn);
    if (http->response.frozen_header) {
        return -1;
    }

    http->response.code = code;
    if (reason) http->response.reason = strdup(reason);

    return 0;
}

/**
 *
 * @param size content size. -1 for chunked transfer encoding.
 * @return 0 on success, -1 if we already sent it out.
 */
int ad_http_set_response_content(ad_conn_t *conn, const char *contenttype, size_t size) {
    ad_http_t *http = (ad_http_t *)ad_conn_get_extra(conn);
    DEBUG("ddddddd %d", http->response.frozen_header);
    if (http->response.frozen_header) {
        return -1;
    }

    // Set Content-Type header.
    if (size > 0) {
        char clenval[20+1];
        sprintf(clenval, "%zu", size);
        ad_http_add_response_header(conn, "Content-Type", (contenttype) ? contenttype : HTTP_DEF_CONTENTTYPE);
        ad_http_add_response_header(conn, "Content-Length", clenval);
        http->response.contentlength = size;
    } else if (contenttype != NULL) {
        ad_http_add_response_header(conn, "Transfer-Encoding", "chunked");
        http->response.contentlength = 0;
    }

    return 0;
}

/**
 * @return total bytes sent, 0 on error.
 */
size_t ad_http_response(ad_conn_t *conn, int code, const char *contenttype, const void *data, size_t size) {
    ad_http_t *http = (ad_http_t *)ad_conn_get_extra(conn);
    if (http->response.frozen_header) {
        return 0;
    }
    ad_http_set_response_code(conn, code, ad_http_get_reason(code));
    ad_http_set_response_content(conn, contenttype, size);
    return ad_http_send_data(conn, data, size);
}

/**
 *
 * @return 0 total bytes put in out buffer, -1 if we already sent it out.
 */
size_t ad_http_send_header(ad_conn_t *conn) {
    ad_http_t *http = (ad_http_t *)ad_conn_get_extra(conn);
    if (http->response.frozen_header) {
        return 0;
    }
    http->response.frozen_header = true;

    // Send status line.
    const char *reason = (http->response.reason) ? http->response.reason : ad_http_get_reason(http->response.code);
    evbuffer_add_printf(http->response.outbuf, "%s %d %s" HTTP_CRLF, http->request.httpver, http->response.code, reason);

    // Send headers.
    qdlnobj_t obj;
    bzero((void*)&obj, sizeof(obj));
    qlisttbl_t *tbl = http->response.headers;
    tbl->lock(tbl);
    while(tbl->getnext(tbl, &obj, NULL, false)) {
        evbuffer_add_printf(http->response.outbuf, "%s: %s" HTTP_CRLF, (char*)obj.name, (char*)obj.data);
    }
    tbl->unlock(tbl);

    // Send empty line, indicator of end of header.
    evbuffer_add(http->response.outbuf, HTTP_CRLF, CONST_STRLEN(HTTP_CRLF));

    return evbuffer_get_length(http->response.outbuf);
}

/**
 *
 * @return 0 on success, -1 if we already sent it out.
 */
size_t ad_http_send_data(ad_conn_t *conn, const void *data, size_t size) {
    ad_http_t *http = (ad_http_t *)ad_conn_get_extra(conn);

    if (http->response.contentlength <= 0) {
        WARN("Content-Length is not set.  Invalid usage.");
        return 0;
    }

    if ((http->response.bodyout + size) > http->response.contentlength) {
        WARN("Trying to send more data than supposed to");
        return 0;
    }

    size_t beforesize = evbuffer_get_length(http->response.outbuf);
    if (! http->response.frozen_header) {
        ad_http_send_header(conn);
    }

    if (evbuffer_add(http->response.outbuf, data, size)) {
        return 0;
    }

    http->response.bodyout += size;
    return (evbuffer_get_length(http->response.outbuf) - beforesize);
}

size_t ad_http_send_chunk(ad_conn_t *conn, const void *data, size_t size) {
    ad_http_t *http = (ad_http_t *)ad_conn_get_extra(conn);

    if (http->response.contentlength >= 0) {
        WARN("Content-Length is set. Invalid usage.");
        return 0;
    }

    if (! http->response.frozen_header) {
        ad_http_send_header(conn);
    }

    size_t beforesize = evbuffer_get_length(http->response.outbuf);
    int status = 0;
    if (size > 0) {
        status += evbuffer_add_printf(http->response.outbuf, "%zu" HTTP_CRLF, size);
        status += evbuffer_add(http->response.outbuf, data, size);
        status += evbuffer_add(http->response.outbuf, HTTP_CRLF, CONST_STRLEN(HTTP_CRLF));
    } else {
        status += evbuffer_add_printf(http->response.outbuf, "0" HTTP_CRLF HTTP_CRLF);
    }
    if (status != 0) {
        WARN("Failed to add data to out-buffer. (size:%jd)", size);
        return 0;
    }

    size_t bytesout = evbuffer_get_length(http->response.outbuf) - beforesize;
    http->response.bodyout += bytesout;
    return bytesout;
}

const char *ad_http_get_reason(int code) {
    switch (code) {
        case HTTP_CODE_CONTINUE : return "Continue";
        case HTTP_CODE_OK : return "OK";
        case HTTP_CODE_CREATED : return "Created";
        case HTTP_CODE_NO_CONTENT : return "No content";
        case HTTP_CODE_PARTIAL_CONTENT : return "Partial Content";
        case HTTP_CODE_MULTI_STATUS : return "Multi Status";
        case HTTP_CODE_MOVED_TEMPORARILY : return "Moved Temporarily";
        case HTTP_CODE_NOT_MODIFIED : return "Not Modified";
        case HTTP_CODE_BAD_REQUEST : return "Bad Request";
        case HTTP_CODE_UNAUTHORIZED : return "Authorization Required";
        case HTTP_CODE_FORBIDDEN : return "Forbidden";
        case HTTP_CODE_NOT_FOUND : return "Not Found";
        case HTTP_CODE_METHOD_NOT_ALLOWED : return "Method Not Allowed";
        case HTTP_CODE_REQUEST_TIME_OUT : return "Request Time Out";
        case HTTP_CODE_GONE : return "Gone";
        case HTTP_CODE_REQUEST_URI_TOO_LONG : return "Request URI Too Long";
        case HTTP_CODE_LOCKED : return "Locked";
        case HTTP_CODE_INTERNAL_SERVER_ERROR : return "Internal Server Error";
        case HTTP_CODE_NOT_IMPLEMENTED : return "Not Implemented";
        case HTTP_CODE_SERVICE_UNAVAILABLE : return "Service Unavailable";
    }

    WARN("Undefined code found. %d", code);
    return "-";
}



/******************************************************************************
 * Private internal functions.
 *****************************************************************************/
static ad_http_t *http_new(struct evbuffer *out) {
    // Create a new connection container.
    ad_http_t *http = NEW_STRUCT(ad_http_t);
    if (http == NULL) return NULL;

    // Allocate additional resources.
    http->request.inbuf = evbuffer_new();
    http->request.headers = qlisttbl(QLISTTBL_UNIQUE | QLISTTBL_CASEINSENSITIVE);
    http->response.headers = qlisttbl(QLISTTBL_UNIQUE | QLISTTBL_CASEINSENSITIVE);
    if(http->request.inbuf == NULL || http->request.headers == NULL || http->response.headers == NULL) {
        http_free(http);
        return NULL;
    }

    // Initialize structure.
    http->request.status = AD_HTTP_REQ_INIT;
    http->request.contentlength = -1;
    http->response.status = AD_HTTP_RES_INIT;
    http->response.contentlength = -1;
    http->response.outbuf = out;

    return http;
}

static void http_free(ad_http_t *http) {
    if (http) {
        if (http->request.inbuf) evbuffer_free(http->request.inbuf);
        if (http->request.method) free(http->request.method);
        if (http->request.uri) free(http->request.uri);
        if (http->request.httpver) free(http->request.httpver);
        if (http->request.path) free(http->request.path);
        if (http->request.query) free(http->request.query);

        if (http->request.headers) http->request.headers->free(http->request.headers);
        if (http->request.host) free(http->request.host);
        if (http->request.domain) free(http->request.domain);

        if (http->response.headers) http->response.headers->free(http->response.headers);
        if (http->response.reason) free(http->response.reason);

        free(http);
    }
}

static void http_free_cb(ad_conn_t *conn, void *userdata) {
    http_free((ad_http_t *)userdata);
}

static size_t http_add_inbuf(struct evbuffer *buffer, ad_http_t *http, size_t maxsize) {
    if (maxsize == 0 || evbuffer_get_length(buffer) == 0) {
        return 0;
    }

    return evbuffer_remove_buffer(buffer, http->request.inbuf, maxsize);
}

static int http_parser(ad_http_t *http, struct evbuffer *in) {
    ASSERT(http != NULL && in != NULL);

    if (http->request.status == AD_HTTP_REQ_INIT) {
        char *line = evbuffer_readln(in, NULL, EVBUFFER_EOL_CRLF);
        if (line == NULL) return http->request.status;
        http->request.status = parse_requestline(http, line);
        free(line);
        // Do not call user callbacks until I reach the next state.
        if (http->request.status == AD_HTTP_REQ_INIT) {
            return AD_TAKEOVER;
        }
    }

    if (http->request.status == AD_HTTP_REQ_REQUESTLINE_DONE) {
        http->request.status = parse_headers(http, in);
        // Do not call user callbacks until I reach the next state.
        if (http->request.status == AD_HTTP_REQ_REQUESTLINE_DONE) {
            return AD_TAKEOVER;
        }
    }

    if (http->request.status == AD_HTTP_REQ_HEADER_DONE) {
        http->request.status = parse_body(http, in);
        // Do not call user callbacks until I reach the next state.
        if (http->request.status == AD_HTTP_REQ_HEADER_DONE) {
            return AD_TAKEOVER;
        }
    }

    if (http->request.status == AD_HTTP_REQ_DONE) {
        return AD_OK;
    }

    if (http->request.status == AD_HTTP_ERROR) {
        return AD_CLOSE;
    }

    BUG_EXIT();
    return AD_CLOSE;
}


static int parse_requestline(ad_http_t *http, char *line) {
    // Parse request line.
    char *saveptr;
    char *method = strtok_r(line, " ", &saveptr);
    char *uri = strtok_r(NULL, " ", &saveptr);
    char *httpver = strtok_r(NULL, " ", &saveptr);
    char *tmp = strtok_r(NULL, " ", &saveptr);

    if (method == NULL || uri == NULL || httpver == NULL || tmp != NULL) {
        DEBUG("Invalid request line. %s", line);
        return AD_HTTP_ERROR;
    }

    // Set request method
    http->request.method = qstrupper(strdup(method));

    // Set HTTP version
    http->request.httpver = qstrupper(strdup(httpver));
    if (strcmp(http->request.httpver, HTTP_PROTOCOL_09)
        && strcmp(http->request.httpver, HTTP_PROTOCOL_10)
        && strcmp(http->request.httpver, HTTP_PROTOCOL_11)
       ) {
        DEBUG("Unknown protocol: %s", http->request.httpver);
        return AD_HTTP_ERROR;
    }

    // Set URI
    if (uri[0] == '/') {
        http->request.uri = strdup(uri);
    } else if ((tmp = strstr(uri, "://"))) {
        // divide URI into host and path
        char *path = strstr(tmp + CONST_STRLEN("://"), "/");
        if (path == NULL) {  // URI has no path ex) http://domain.com:80
            http->request.headers->putstr(http->request.headers, "Host", tmp  + CONST_STRLEN("://"));
            http->request.uri = strdup("/");
        } else {  // URI has path, ex) http://domain.com:80/path
            *path = '\0';
            http->request.headers->putstr(http->request.headers, "Host", tmp  + CONST_STRLEN("://"));
            *path = '/';
            http->request.uri = strdup(path);
        }
    } else {
        DEBUG("Invalid URI format. %s", uri);
        return AD_HTTP_ERROR;
    }

    // Set request path. Only path part from URI.
    http->request.path = strdup(http->request.uri);
    tmp = strstr(http->request.path, "?");
    if (tmp) {
        *tmp ='\0';
        http->request.query = strdup(tmp + 1);
    } else {
        http->request.query = strdup("");
    }
    qurl_decode(http->request.path);

    // check path
    if (isValidPathname(http->request.path) == false) {
        DEBUG("Invalid URI format : %s", http->request.uri);
        return AD_HTTP_ERROR;
    }
    correctPathname(http->request.path);

    DEBUG("Method=%s, URI=%s, VER=%s", http->request.method, http->request.uri, http->request.httpver);

    return AD_HTTP_REQ_REQUESTLINE_DONE;
}

static int parse_headers(ad_http_t *http, struct evbuffer *in) {
    char *line;
    while ((line = evbuffer_readln(in, NULL, EVBUFFER_EOL_CRLF))) {
        if (IS_EMPTY_STR(line)) {
            const char *clen = http->request.headers->getstr(http->request.headers, "Content-Length", false);
            http->request.contentlength = (clen) ? atol(clen) : -1;
            return AD_HTTP_REQ_HEADER_DONE;
        }
        // Parse
        char *name, *value;
        char *tmp = strstr(line, ":");
        if (tmp) {
            *tmp = '\0';
            name = qstrtrim(line);
            value = qstrtrim(tmp + 1);
        } else {
            name = qstrtrim(line);
            value = "";
        }
        // Add
        http->request.headers->putstr(http->request.headers, name, value);

        free(line);
    }

    return http->request.status;
}

static int parse_body(ad_http_t *http, struct evbuffer *in) {
    // Handle static data case.
    if (http->request.contentlength == 0) {
        return AD_HTTP_REQ_DONE;
    } else if (http->request.contentlength > 0) {
        if (http->request.contentlength > http->request.bodyin) {
            size_t maxread = http->request.contentlength - http->request.bodyin;
            if (maxread > 0 && evbuffer_get_length(in) > 0) {
                http->request.bodyin += http_add_inbuf(in, http, maxread);
            }
        }
        if (http->request.contentlength == http->request.bodyin) {
            return AD_HTTP_REQ_DONE;
        }
    } else {
        // Check if Transfer-Encoding is chunked.
        const char *tranenc = http->request.headers->getstr(http->request.headers, "Transfer-Encoding", false);
        if (tranenc != NULL && !strcmp("chunked", tranenc)) {
            // TODO: handle chunked encoding
        } else {
            return AD_HTTP_REQ_DONE;
        }
    }

    return http->request.status;
}

/******************************************************************************
 * Private internal functions.
 *****************************************************************************/

/**
 * validate file path
 */
static bool isValidPathname(const char *path) {
    if (path == NULL) return false;

    int len = strlen(path);
    if (len == 0 || len >= PATH_MAX) return false;
    else if (path[0] != '/') return false;
    else if (strpbrk(path, "\\:*?\"<>|") != NULL) return false;

    // check folder name length
    int n;
    char *t;
    for (n = 0, t = (char *)path; *t != '\0'; t++) {
        if (*t == '/') {
            n = 0;
            continue;
        }
        if (n >= FILENAME_MAX) {
            DEBUG("Filename too long.");
            return false;
        }
        n++;
    }

    return true;
}

/**
 * Correct pathname.
 *
 * @note
 *    remove :  heading & tailing white spaces, double slashes, tailing slash
 */
static void correctPathname(char *path) {
    // Take care of head & tail white spaces.
    qstrtrim(path);

    // Take care of double slashes.
    while (strstr(path, "//") != NULL) qstrreplace("sr", path, "//", "/");

    // Take care of tailing slash.
    int len = strlen(path);
    if (len <= 1) return;
    if (path[len - 1] == '/') path[len - 1] = '\0';
}

