/*
 * Copyright (c) 2009-2011, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2010-2014, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 * Copyright (c) 2015, Matt Stancliff <matt at genges dot com>,
 *                     Jan-Erik Rediger <janerik at fnordig dot com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of NexCache nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "fmacros.h"
#include "win32.h"

#include "nexcache.h"

#include "net.h"
#include "nexcache_private.h"

#include <sds.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static nexcacheReply *createReplyObject(int type);
static void *createStringObject(const nexcacheReadTask *task, char *str, size_t len);
static void *createArrayObject(const nexcacheReadTask *task, size_t elements);
static void *createIntegerObject(const nexcacheReadTask *task, long long value);
static void *createDoubleObject(const nexcacheReadTask *task, double value, char *str, size_t len);
static void *createNilObject(const nexcacheReadTask *task);
static void *createBoolObject(const nexcacheReadTask *task, int bval);

/* Default set of functions to build the reply. Keep in mind that such a
 * function returning NULL is interpreted as OOM. */
static nexcacheReplyObjectFunctions defaultFunctions = {
    createStringObject,
    createArrayObject,
    createIntegerObject,
    createDoubleObject,
    createNilObject,
    createBoolObject,
    freeReplyObject};

/* Create a reply object */
static nexcacheReply *createReplyObject(int type) {
    nexcacheReply *r = vk_calloc(1, sizeof(*r));

    if (r == NULL)
        return NULL;

    r->type = type;
    return r;
}

/* Free a reply object */
void freeReplyObject(void *reply) {
    nexcacheReply *r = reply;
    size_t j;

    if (r == NULL)
        return;

    switch (r->type) {
    case NEXCACHE_REPLY_INTEGER:
    case NEXCACHE_REPLY_NIL:
    case NEXCACHE_REPLY_BOOL:
        break; /* Nothing to free */
    case NEXCACHE_REPLY_ARRAY:
    case NEXCACHE_REPLY_MAP:
    case NEXCACHE_REPLY_ATTR:
    case NEXCACHE_REPLY_SET:
    case NEXCACHE_REPLY_PUSH:
        if (r->element != NULL) {
            for (j = 0; j < r->elements; j++)
                freeReplyObject(r->element[j]);
            vk_free(r->element);
        }
        break;
    case NEXCACHE_REPLY_ERROR:
    case NEXCACHE_REPLY_STATUS:
    case NEXCACHE_REPLY_STRING:
    case NEXCACHE_REPLY_DOUBLE:
    case NEXCACHE_REPLY_VERB:
    case NEXCACHE_REPLY_BIGNUM:
        vk_free(r->str);
        break;
    }
    vk_free(r);
}

static void *createStringObject(const nexcacheReadTask *task, char *str, size_t len) {
    nexcacheReply *r, *parent;
    char *buf;

    r = createReplyObject(task->type);
    if (r == NULL)
        return NULL;

    assert(task->type == NEXCACHE_REPLY_ERROR ||
           task->type == NEXCACHE_REPLY_STATUS ||
           task->type == NEXCACHE_REPLY_STRING ||
           task->type == NEXCACHE_REPLY_VERB ||
           task->type == NEXCACHE_REPLY_BIGNUM);

    /* Copy string value */
    if (task->type == NEXCACHE_REPLY_VERB) {
        buf = vk_malloc(len - 4 + 1); /* Skip 4 bytes of verbatim type header. */
        if (buf == NULL)
            goto oom;

        memcpy(r->vtype, str, 3);
        r->vtype[3] = '\0';
        memcpy(buf, str + 4, len - 4);
        buf[len - 4] = '\0';
        r->len = len - 4;
    } else {
        buf = vk_malloc(len + 1);
        if (buf == NULL)
            goto oom;

        memcpy(buf, str, len);
        buf[len] = '\0';
        r->len = len;
    }
    r->str = buf;

    if (task->parent) {
        parent = task->parent->obj;
        assert(parent->type == NEXCACHE_REPLY_ARRAY ||
               parent->type == NEXCACHE_REPLY_MAP ||
               parent->type == NEXCACHE_REPLY_ATTR ||
               parent->type == NEXCACHE_REPLY_SET ||
               parent->type == NEXCACHE_REPLY_PUSH);
        parent->element[task->idx] = r;
    }
    return r;

oom:
    freeReplyObject(r);
    return NULL;
}

static void *createArrayObject(const nexcacheReadTask *task, size_t elements) {
    nexcacheReply *r, *parent;

    r = createReplyObject(task->type);
    if (r == NULL)
        return NULL;

    if (elements > 0) {
        r->element = vk_calloc(elements, sizeof(nexcacheReply *));
        if (r->element == NULL) {
            freeReplyObject(r);
            return NULL;
        }
    }

    r->elements = elements;

    if (task->parent) {
        parent = task->parent->obj;
        assert(parent->type == NEXCACHE_REPLY_ARRAY ||
               parent->type == NEXCACHE_REPLY_MAP ||
               parent->type == NEXCACHE_REPLY_ATTR ||
               parent->type == NEXCACHE_REPLY_SET ||
               parent->type == NEXCACHE_REPLY_PUSH);
        parent->element[task->idx] = r;
    }
    return r;
}

static void *createIntegerObject(const nexcacheReadTask *task, long long value) {
    nexcacheReply *r, *parent;

    r = createReplyObject(NEXCACHE_REPLY_INTEGER);
    if (r == NULL)
        return NULL;

    r->integer = value;

    if (task->parent) {
        parent = task->parent->obj;
        assert(parent->type == NEXCACHE_REPLY_ARRAY ||
               parent->type == NEXCACHE_REPLY_MAP ||
               parent->type == NEXCACHE_REPLY_ATTR ||
               parent->type == NEXCACHE_REPLY_SET ||
               parent->type == NEXCACHE_REPLY_PUSH);
        parent->element[task->idx] = r;
    }
    return r;
}

static void *createDoubleObject(const nexcacheReadTask *task, double value, char *str, size_t len) {
    nexcacheReply *r, *parent;

    if (len == SIZE_MAX) // Prevents vk_malloc(0) if len equals SIZE_MAX
        return NULL;

    r = createReplyObject(NEXCACHE_REPLY_DOUBLE);
    if (r == NULL)
        return NULL;

    r->dval = value;
    r->str = vk_malloc(len + 1);
    if (r->str == NULL) {
        freeReplyObject(r);
        return NULL;
    }

    /* The double reply also has the original protocol string representing a
     * double as a null terminated string. This way the caller does not need
     * to format back for string conversion, especially since NexCache does efforts
     * to make the string more human readable avoiding the classical double
     * decimal string conversion artifacts. */
    memcpy(r->str, str, len);
    r->str[len] = '\0';
    r->len = len;

    if (task->parent) {
        parent = task->parent->obj;
        assert(parent->type == NEXCACHE_REPLY_ARRAY ||
               parent->type == NEXCACHE_REPLY_MAP ||
               parent->type == NEXCACHE_REPLY_ATTR ||
               parent->type == NEXCACHE_REPLY_SET ||
               parent->type == NEXCACHE_REPLY_PUSH);
        parent->element[task->idx] = r;
    }
    return r;
}

static void *createNilObject(const nexcacheReadTask *task) {
    nexcacheReply *r, *parent;

    r = createReplyObject(NEXCACHE_REPLY_NIL);
    if (r == NULL)
        return NULL;

    if (task->parent) {
        parent = task->parent->obj;
        assert(parent->type == NEXCACHE_REPLY_ARRAY ||
               parent->type == NEXCACHE_REPLY_MAP ||
               parent->type == NEXCACHE_REPLY_ATTR ||
               parent->type == NEXCACHE_REPLY_SET ||
               parent->type == NEXCACHE_REPLY_PUSH);
        parent->element[task->idx] = r;
    }
    return r;
}

static void *createBoolObject(const nexcacheReadTask *task, int bval) {
    nexcacheReply *r, *parent;

    r = createReplyObject(NEXCACHE_REPLY_BOOL);
    if (r == NULL)
        return NULL;

    r->integer = bval != 0;

    if (task->parent) {
        parent = task->parent->obj;
        assert(parent->type == NEXCACHE_REPLY_ARRAY ||
               parent->type == NEXCACHE_REPLY_MAP ||
               parent->type == NEXCACHE_REPLY_ATTR ||
               parent->type == NEXCACHE_REPLY_SET ||
               parent->type == NEXCACHE_REPLY_PUSH);
        parent->element[task->idx] = r;
    }
    return r;
}

/* Return the number of digits of 'v' when converted to string in radix 10.
 * Implementation borrowed from link in nexcache/src/util.c:string2ll(). */
static uint32_t countDigits(uint64_t v) {
    uint32_t result = 1;
    for (;;) {
        if (v < 10)
            return result;
        if (v < 100)
            return result + 1;
        if (v < 1000)
            return result + 2;
        if (v < 10000)
            return result + 3;
        v /= 10000U;
        result += 4;
    }
}

/* Helper that calculates the bulk length given a certain string length. */
static size_t bulklen(size_t len) {
    return 1 + countDigits(len) + 2 + len + 2;
}

int nexcachevFormatCommand(char **target, const char *format, va_list ap) {
    const char *c = format;
    char *cmd = NULL;   /* final command */
    int pos;            /* position in final command */
    sds curarg, newarg; /* current argument */
    int touched = 0;    /* was the current argument touched? */
    char **curargv = NULL, **newargv = NULL;
    int argc = 0;
    int totlen = 0;
    int error_type = 0; /* 0 = no error; -1 = memory error; -2 = format error */
    int j;

    /* Abort if there is not target to set */
    if (target == NULL)
        return -1;

    /* Build the command string accordingly to protocol */
    curarg = sdsempty();
    if (curarg == NULL)
        return -1;

    while (*c != '\0') {
        if (*c != '%' || c[1] == '\0') {
            if (*c == ' ') {
                if (touched) {
                    newargv = vk_realloc(curargv, sizeof(char *) * (argc + 1));
                    if (newargv == NULL)
                        goto memory_err;
                    curargv = newargv;
                    curargv[argc++] = curarg;
                    totlen += bulklen(sdslen(curarg));

                    /* curarg is put in argv so it can be overwritten. */
                    curarg = sdsempty();
                    if (curarg == NULL)
                        goto memory_err;
                    touched = 0;
                }
            } else {
                newarg = sdscatlen(curarg, c, 1);
                if (newarg == NULL)
                    goto memory_err;
                curarg = newarg;
                touched = 1;
            }
        } else {
            char *arg;
            size_t size;

            /* Set newarg so it can be checked even if it is not touched. */
            newarg = curarg;

            switch (c[1]) {
            case 's':
                arg = va_arg(ap, char *);
                if (arg == NULL)
                    goto format_err;
                size = strlen(arg);
                if (size > 0)
                    newarg = sdscatlen(curarg, arg, size);
                break;
            case 'b':
                arg = va_arg(ap, char *);
                if (arg == NULL)
                    goto format_err;
                size = va_arg(ap, size_t);
                if (size > 0)
                    newarg = sdscatlen(curarg, arg, size);
                break;
            case '%':
                newarg = sdscat(curarg, "%");
                break;
            default:
                /* Try to detect printf format */
                {
                    static const char intfmts[] = "diouxX";
                    static const char flags[] = "#0-+ ";
                    char _format[16];
                    const char *_p = c + 1;
                    size_t _l = 0;
                    va_list _cpy;

                    /* Flags */
                    while (*_p != '\0' && strchr(flags, *_p) != NULL)
                        _p++;

                    /* Field width */
                    while (*_p != '\0' && isdigit((int)*_p))
                        _p++;

                    /* Precision */
                    if (*_p == '.') {
                        _p++;
                        while (*_p != '\0' && isdigit((int)*_p))
                            _p++;
                    }

                    /* Copy va_list before consuming with va_arg */
                    va_copy(_cpy, ap);

                    /* Make sure we have more characters otherwise strchr() accepts
                     * '\0' as an integer specifier. This is checked after above
                     * va_copy() to avoid UB in fmt_invalid's call to va_end(). */
                    if (*_p == '\0')
                        goto fmt_invalid;

                    /* Integer conversion (without modifiers) */
                    if (strchr(intfmts, *_p) != NULL) {
                        va_arg(ap, int);
                        goto fmt_valid;
                    }

                    /* Double conversion (without modifiers) */
                    if (strchr("eEfFgGaA", *_p) != NULL) {
                        va_arg(ap, double);
                        goto fmt_valid;
                    }

                    /* Size: char */
                    if (_p[0] == 'h' && _p[1] == 'h') {
                        _p += 2;
                        if (*_p != '\0' && strchr(intfmts, *_p) != NULL) {
                            va_arg(ap, int); /* char gets promoted to int */
                            goto fmt_valid;
                        }
                        goto fmt_invalid;
                    }

                    /* Size: short */
                    if (_p[0] == 'h') {
                        _p += 1;
                        if (*_p != '\0' && strchr(intfmts, *_p) != NULL) {
                            va_arg(ap, int); /* short gets promoted to int */
                            goto fmt_valid;
                        }
                        goto fmt_invalid;
                    }

                    /* Size: long long */
                    if (_p[0] == 'l' && _p[1] == 'l') {
                        _p += 2;
                        if (*_p != '\0' && strchr(intfmts, *_p) != NULL) {
                            va_arg(ap, long long);
                            goto fmt_valid;
                        }
                        goto fmt_invalid;
                    }

                    /* Size: long */
                    if (_p[0] == 'l') {
                        _p += 1;
                        if (*_p != '\0' && strchr(intfmts, *_p) != NULL) {
                            va_arg(ap, long);
                            goto fmt_valid;
                        }
                        goto fmt_invalid;
                    }

                fmt_invalid:
                    va_end(_cpy);
                    goto format_err;

                fmt_valid:
                    _l = (_p + 1) - c;
                    if (_l < sizeof(_format) - 2) {
                        memcpy(_format, c, _l);
                        _format[_l] = '\0';
                        newarg = sdscatvprintf(curarg, _format, _cpy);

                        /* Update current position (note: outer blocks
                         * increment c twice so compensate here) */
                        c = _p - 1;
                    }

                    va_end(_cpy);
                    break;
                }
            }

            if (newarg == NULL)
                goto memory_err;
            curarg = newarg;

            touched = 1;
            c++;
            if (*c == '\0')
                break;
        }
        c++;
    }

    /* Add the last argument if needed */
    if (touched) {
        newargv = vk_realloc(curargv, sizeof(char *) * (argc + 1));
        if (newargv == NULL)
            goto memory_err;
        curargv = newargv;
        curargv[argc++] = curarg;
        totlen += bulklen(sdslen(curarg));
    } else {
        sdsfree(curarg);
    }

    /* Clear curarg because it was put in curargv or was free'd. */
    curarg = NULL;

    /* Add bytes needed to hold multi bulk count */
    totlen += 1 + countDigits(argc) + 2;

    /* Build the command at protocol level */
    cmd = vk_malloc(totlen + 1);
    if (cmd == NULL)
        goto memory_err;

    pos = sprintf(cmd, "*%d\r\n", argc);
    for (j = 0; j < argc; j++) {
        pos += sprintf(cmd + pos, "$%zu\r\n", sdslen(curargv[j]));
        memcpy(cmd + pos, curargv[j], sdslen(curargv[j]));
        pos += sdslen(curargv[j]);
        sdsfree(curargv[j]);
        cmd[pos++] = '\r';
        cmd[pos++] = '\n';
    }
    assert(pos == totlen);
    cmd[pos] = '\0';

    vk_free(curargv);
    *target = cmd;
    return totlen;

format_err:
    error_type = -2;
    goto cleanup;

memory_err:
    error_type = -1;
    goto cleanup;

cleanup:
    if (curargv) {
        while (argc--)
            sdsfree(curargv[argc]);
        vk_free(curargv);
    }

    sdsfree(curarg);
    vk_free(cmd);

    return error_type;
}

/* Format a command according to the RESP protocol. This function
 * takes a format similar to printf:
 *
 * %s represents a C null terminated string you want to interpolate
 * %b represents a binary safe string
 *
 * When using %b you need to provide both the pointer to the string
 * and the length in bytes as a size_t. Examples:
 *
 * len = nexcacheFormatCommand(target, "GET %s", mykey);
 * len = nexcacheFormatCommand(target, "SET %s %b", mykey, myval, myvallen);
 */
int nexcacheFormatCommand(char **target, const char *format, ...) {
    va_list ap;
    int len;
    va_start(ap, format);
    len = nexcachevFormatCommand(target, format, ap);
    va_end(ap);

    /* The API says "-1" means bad result, but we now also return "-2" in some
     * cases.  Force the return value to always be -1. */
    if (len < 0)
        len = -1;

    return len;
}

/* Format a command according to the RESP protocol using an sds string and
 * sdscatfmt for the processing of arguments. This function takes the
 * number of arguments, an array with arguments and an array with their
 * lengths. If the latter is set to NULL, strlen will be used to compute the
 * argument lengths.
 */
long long nexcacheFormatSdsCommandArgv(sds *target, int argc, const char **argv,
                                     const size_t *argvlen) {
    sds cmd, aux;
    unsigned long long totlen, len;
    int j;

    /* Abort on a NULL target */
    if (target == NULL)
        return -1;

    /* Calculate our total size */
    totlen = 1 + countDigits(argc) + 2;
    for (j = 0; j < argc; j++) {
        len = argvlen ? argvlen[j] : strlen(argv[j]);
        totlen += bulklen(len);
    }

    /* Use an SDS string for command construction */
    cmd = sdsempty();
    if (cmd == NULL)
        return -1;

    /* We already know how much storage we need */
    aux = sdsMakeRoomFor(cmd, totlen);
    if (aux == NULL) {
        sdsfree(cmd);
        return -1;
    }

    cmd = aux;

    /* Construct command */
    cmd = sdscatfmt(cmd, "*%i\r\n", argc);
    for (j = 0; j < argc; j++) {
        len = argvlen ? argvlen[j] : strlen(argv[j]);
        cmd = sdscatfmt(cmd, "$%U\r\n", len);
        cmd = sdscatlen(cmd, argv[j], len);
        cmd = sdscatlen(cmd, "\r\n", sizeof("\r\n") - 1);
    }

    assert(sdslen(cmd) == totlen);

    *target = cmd;
    return totlen;
}

/* Format a command according to the RESP protocol. This function takes the
 * number of arguments, an array with arguments and an array with their
 * lengths. If the latter is set to NULL, strlen will be used to compute the
 * argument lengths.
 */
long long nexcacheFormatCommandArgv(char **target, int argc, const char **argv, const size_t *argvlen) {
    char *cmd = NULL; /* final command */
    size_t pos;       /* position in final command */
    size_t len, totlen;
    int j;

    /* Abort on a NULL target */
    if (target == NULL)
        return -1;

    /* Calculate number of bytes needed for the command */
    totlen = 1 + countDigits(argc) + 2;
    for (j = 0; j < argc; j++) {
        len = argvlen ? argvlen[j] : strlen(argv[j]);
        totlen += bulklen(len);
    }

    /* Build the command at protocol level */
    cmd = vk_malloc(totlen + 1);
    if (cmd == NULL)
        return -1;

    pos = sprintf(cmd, "*%d\r\n", argc);
    for (j = 0; j < argc; j++) {
        len = argvlen ? argvlen[j] : strlen(argv[j]);
        pos += sprintf(cmd + pos, "$%zu\r\n", len);
        memcpy(cmd + pos, argv[j], len);
        pos += len;
        cmd[pos++] = '\r';
        cmd[pos++] = '\n';
    }
    assert(pos == totlen);
    cmd[pos] = '\0';

    *target = cmd;
    return totlen;
}

void nexcacheFreeCommand(char *cmd) {
    vk_free(cmd);
}

void nexcacheSetError(nexcacheContext *c, int type, const char *str) {
    size_t len;

    c->err = type;
    if (str != NULL) {
        len = strlen(str);
        len = len < (sizeof(c->errstr) - 1) ? len : (sizeof(c->errstr) - 1);
        memcpy(c->errstr, str, len);
        c->errstr[len] = '\0';
    } else {
        /* Only NEXCACHE_ERR_IO may lack a description! */
        assert(type == NEXCACHE_ERR_IO);
        strerror_r(errno, c->errstr, sizeof(c->errstr));
    }
}

nexcacheReader *nexcacheReaderCreate(void) {
    return nexcacheReaderCreateWithFunctions(&defaultFunctions);
}

static void nexcachePushAutoFree(void *privdata, void *reply) {
    (void)privdata;
    freeReplyObject(reply);
}

static nexcacheContext *nexcacheContextInit(void) {
    nexcacheContext *c;

    c = vk_calloc(1, sizeof(*c));
    if (c == NULL)
        return NULL;

    c->obuf = sdsempty();
    c->reader = nexcacheReaderCreate();
    c->fd = NEXCACHE_INVALID_FD;

    if (c->obuf == NULL || c->reader == NULL) {
        nexcacheFree(c);
        return NULL;
    }

    return c;
}

void nexcacheFree(nexcacheContext *c) {
    if (c == NULL)
        return;

    if (c->funcs && c->funcs->close) {
        c->funcs->close(c);
    }

    sdsfree(c->obuf);
    nexcacheReaderFree(c->reader);
    vk_free(c->tcp.host);
    vk_free(c->tcp.source_addr);
    vk_free(c->unix_sock.path);
    vk_free(c->connect_timeout);
    vk_free(c->command_timeout);
    vk_free(c->saddr);

    if (c->privdata && c->free_privdata)
        c->free_privdata(c->privdata);

    if (c->funcs && c->funcs->free_privctx)
        c->funcs->free_privctx(c->privctx);

    memset(c, 0xff, sizeof(*c));
    vk_free(c);
}

nexcacheFD nexcacheFreeKeepFd(nexcacheContext *c) {
    nexcacheFD fd = c->fd;
    c->fd = NEXCACHE_INVALID_FD;
    nexcacheFree(c);
    return fd;
}

int nexcacheReconnect(nexcacheContext *c) {
    nexcacheOptions options = {.connect_timeout = c->connect_timeout};

    c->err = 0;
    memset(c->errstr, '\0', strlen(c->errstr));

    assert(c->funcs);
    if (c->funcs && c->funcs->close)
        c->funcs->close(c);

    if (c->privctx && c->funcs && c->funcs->free_privctx) {
        c->funcs->free_privctx(c->privctx);
        c->privctx = NULL;
    }

    sdsfree(c->obuf);
    nexcacheReaderFree(c->reader);

    c->obuf = sdsempty();
    c->reader = nexcacheReaderCreate();

    if (c->obuf == NULL || c->reader == NULL) {
        nexcacheSetError(c, NEXCACHE_ERR_OOM, "Out of memory");
        return NEXCACHE_ERR;
    }

    switch (c->connection_type) {
    case NEXCACHE_CONN_TCP:
        /* FALLTHRU */
    case NEXCACHE_CONN_RDMA:
        options.endpoint.tcp.source_addr = c->tcp.source_addr;
        options.endpoint.tcp.ip = c->tcp.host;
        options.endpoint.tcp.port = c->tcp.port;
        break;
    case NEXCACHE_CONN_UNIX:
        options.endpoint.unix_socket = c->unix_sock.path;
        break;
    default:
        /* Something bad happened here and shouldn't have. There isn't
           enough information in the context to reconnect. */
        nexcacheSetError(c, NEXCACHE_ERR_OTHER, "Not enough information to reconnect");
        return NEXCACHE_ERR;
    }

    if (c->funcs && c->funcs->connect &&
        c->funcs->connect(c, &options) != NEXCACHE_OK) {
        return NEXCACHE_ERR;
    }

    if (c->command_timeout != NULL && (c->flags & NEXCACHE_BLOCK) &&
        c->fd != NEXCACHE_INVALID_FD && c->funcs && c->funcs->set_timeout) {
        c->funcs->set_timeout(c, *c->command_timeout);
    }

    return NEXCACHE_OK;
}

nexcacheContext *nexcacheConnectWithOptions(const nexcacheOptions *options) {
    nexcacheContext *c;

    if (options->type >= NEXCACHE_CONN_MAX) {
        return NULL;
    }

    c = nexcacheContextInit();
    if (c == NULL) {
        return NULL;
    }
    if (!(options->options & NEXCACHE_OPT_NONBLOCK)) {
        c->flags |= NEXCACHE_BLOCK;
    }
    if (options->options & NEXCACHE_OPT_REUSEADDR) {
        c->flags |= NEXCACHE_REUSEADDR;
    }
    if (options->options & NEXCACHE_OPT_NOAUTOFREE) {
        c->flags |= NEXCACHE_NO_AUTO_FREE;
    }
    if (options->options & NEXCACHE_OPT_NOAUTOFREEREPLIES) {
        c->flags |= NEXCACHE_NO_AUTO_FREE_REPLIES;
    }
    if (options->options & NEXCACHE_OPT_PREFER_IPV4) {
        c->flags |= NEXCACHE_PREFER_IPV4;
    }
    if (options->options & NEXCACHE_OPT_PREFER_IPV6) {
        c->flags |= NEXCACHE_PREFER_IPV6;
    }

    if (options->options & NEXCACHE_OPT_MPTCP) {
        if (!nexcacheHasMptcp()) {
            nexcacheSetError(c, NEXCACHE_ERR_PROTOCOL, "MPTCP is not supported on this platform");
            return c;
        }
        c->flags |= NEXCACHE_MPTCP;
    }

    /* Set any user supplied RESP3 PUSH handler or use freeReplyObject
     * as a default unless specifically flagged that we don't want one. */
    if (options->push_cb != NULL)
        nexcacheSetPushCallback(c, options->push_cb);
    else if (!(options->options & NEXCACHE_OPT_NO_PUSH_AUTOFREE))
        nexcacheSetPushCallback(c, nexcachePushAutoFree);

    c->privdata = options->privdata;
    c->free_privdata = options->free_privdata;
    c->connection_type = options->type;
    /* Make sure we set a nexcacheContextFuncs before returning any context. */
    nexcacheContextSetFuncs(c);

    if (nexcacheContextUpdateConnectTimeout(c, options->connect_timeout) != NEXCACHE_OK ||
        nexcacheContextUpdateCommandTimeout(c, options->command_timeout) != NEXCACHE_OK) {
        nexcacheSetError(c, NEXCACHE_ERR_OOM, "Out of memory");
        return c;
    }

    c->funcs->connect(c, options);
    if (c->err == 0 && c->fd != NEXCACHE_INVALID_FD &&
        options->command_timeout != NULL && (c->flags & NEXCACHE_BLOCK)) {
        c->funcs->set_timeout(c, *options->command_timeout);
    }

    return c;
}

/* Connect to a server instance. On error the field error in the returned
 * context will be set to the return value of the error function.
 * When no set of reply functions is given, the default set will be used. */
nexcacheContext *nexcacheConnect(const char *ip, int port) {
    nexcacheOptions options = {0};
    NEXCACHE_OPTIONS_SET_TCP(&options, ip, port);
    return nexcacheConnectWithOptions(&options);
}

nexcacheContext *nexcacheConnectWithTimeout(const char *ip, int port, const struct timeval tv) {
    nexcacheOptions options = {0};
    NEXCACHE_OPTIONS_SET_TCP(&options, ip, port);
    options.connect_timeout = &tv;
    return nexcacheConnectWithOptions(&options);
}

nexcacheContext *nexcacheConnectNonBlock(const char *ip, int port) {
    nexcacheOptions options = {0};
    NEXCACHE_OPTIONS_SET_TCP(&options, ip, port);
    options.options |= NEXCACHE_OPT_NONBLOCK;
    return nexcacheConnectWithOptions(&options);
}

nexcacheContext *nexcacheConnectBindNonBlock(const char *ip, int port,
                                         const char *source_addr) {
    nexcacheOptions options = {0};
    NEXCACHE_OPTIONS_SET_TCP(&options, ip, port);
    options.endpoint.tcp.source_addr = source_addr;
    options.options |= NEXCACHE_OPT_NONBLOCK;
    return nexcacheConnectWithOptions(&options);
}

nexcacheContext *nexcacheConnectBindNonBlockWithReuse(const char *ip, int port,
                                                  const char *source_addr) {
    nexcacheOptions options = {0};
    NEXCACHE_OPTIONS_SET_TCP(&options, ip, port);
    options.endpoint.tcp.source_addr = source_addr;
    options.options |= NEXCACHE_OPT_NONBLOCK | NEXCACHE_OPT_REUSEADDR;
    return nexcacheConnectWithOptions(&options);
}

nexcacheContext *nexcacheConnectUnix(const char *path) {
    nexcacheOptions options = {0};
    NEXCACHE_OPTIONS_SET_UNIX(&options, path);
    return nexcacheConnectWithOptions(&options);
}

nexcacheContext *nexcacheConnectUnixWithTimeout(const char *path, const struct timeval tv) {
    nexcacheOptions options = {0};
    NEXCACHE_OPTIONS_SET_UNIX(&options, path);
    options.connect_timeout = &tv;
    return nexcacheConnectWithOptions(&options);
}

nexcacheContext *nexcacheConnectUnixNonBlock(const char *path) {
    nexcacheOptions options = {0};
    NEXCACHE_OPTIONS_SET_UNIX(&options, path);
    options.options |= NEXCACHE_OPT_NONBLOCK;
    return nexcacheConnectWithOptions(&options);
}

nexcacheContext *nexcacheConnectFd(nexcacheFD fd) {
    nexcacheOptions options = {0};
    options.type = NEXCACHE_CONN_USERFD;
    options.endpoint.fd = fd;
    return nexcacheConnectWithOptions(&options);
}

/* Set read/write timeout on a blocking socket. */
int nexcacheSetTimeout(nexcacheContext *c, const struct timeval tv) {
    if (!(c->flags & NEXCACHE_BLOCK))
        return NEXCACHE_ERR;

    if (nexcacheContextUpdateCommandTimeout(c, &tv) != NEXCACHE_OK) {
        nexcacheSetError(c, NEXCACHE_ERR_OOM, "Out of memory");
        return NEXCACHE_ERR;
    }

    return c->funcs->set_timeout(c, tv);
}

int nexcacheEnableKeepAliveWithInterval(nexcacheContext *c, int interval) {
    return nexcacheKeepAlive(c, interval);
}

/* Enable connection KeepAlive. */
int nexcacheEnableKeepAlive(nexcacheContext *c) {
    return nexcacheKeepAlive(c, NEXCACHE_KEEPALIVE_INTERVAL);
}

/* Set the socket option TCP_USER_TIMEOUT. */
int nexcacheSetTcpUserTimeout(nexcacheContext *c, unsigned int timeout) {
    return nexcacheContextSetTcpUserTimeout(c, timeout);
}

/* Set a user provided RESP3 PUSH handler and return any old one set. */
nexcachePushFn *nexcacheSetPushCallback(nexcacheContext *c, nexcachePushFn *fn) {
    nexcachePushFn *old = c->push_cb;
    c->push_cb = fn;
    return old;
}

/* Use this function to handle a read event on the descriptor. It will try
 * and read some bytes from the socket and feed them to the reply parser.
 *
 * After this function is called, you may use nexcacheGetReplyFromReader to
 * see if there is a reply available. */
int nexcacheBufferRead(nexcacheContext *c) {
    char buf[1024 * 16];
    int nread;

    /* Return early when the context has seen an error. */
    if (c->err)
        return NEXCACHE_ERR;

    if (c->funcs->read_zc) {
        char *zc_buf;
        nread = c->funcs->read_zc(c, &zc_buf);
        if (nread < 0) {
            return NEXCACHE_ERR;
        }
        if (nread > 0 && nexcacheReaderFeed(c->reader, zc_buf, nread) != NEXCACHE_OK) {
            nexcacheSetError(c, c->reader->err, c->reader->errstr);
            return NEXCACHE_ERR;
        }
        return c->funcs->read_zc_done(c);
    }
    nread = c->funcs->read(c, buf, sizeof(buf));
    if (nread < 0) {
        return NEXCACHE_ERR;
    }
    if (nread > 0 && nexcacheReaderFeed(c->reader, buf, nread) != NEXCACHE_OK) {
        nexcacheSetError(c, c->reader->err, c->reader->errstr);
        return NEXCACHE_ERR;
    }
    return NEXCACHE_OK;
}

/* Write the output buffer to the socket.
 *
 * Returns NEXCACHE_OK when the buffer is empty, or (a part of) the buffer was
 * successfully written to the socket. When the buffer is empty after the
 * write operation, "done" is set to 1 (if given).
 *
 * Returns NEXCACHE_ERR if an unrecoverable error occurred in the underlying
 * c->funcs->write function.
 */
int nexcacheBufferWrite(nexcacheContext *c, int *done) {

    /* Return early when the context has seen an error. */
    if (c->err)
        return NEXCACHE_ERR;

    if (sdslen(c->obuf) > 0) {
        ssize_t nwritten = c->funcs->write(c);
        if (nwritten < 0) {
            return NEXCACHE_ERR;
        } else if (nwritten > 0) {
            if (nwritten == (ssize_t)sdslen(c->obuf)) {
                sdsfree(c->obuf);
                c->obuf = sdsempty();
                if (c->obuf == NULL)
                    goto oom;
            } else {
                /* No length check in NexCaches sdsrange() */
                if (sdslen(c->obuf) > SSIZE_MAX)
                    goto oom;
                sdsrange(c->obuf, nwritten, -1);
            }
        }
    }
    if (done != NULL)
        *done = (sdslen(c->obuf) == 0);
    return NEXCACHE_OK;

oom:
    nexcacheSetError(c, NEXCACHE_ERR_OOM, "Out of memory");
    return NEXCACHE_ERR;
}

/* Internal helper that returns 1 if the reply was a RESP3 PUSH
 * message and we handled it with a user-provided callback. */
static int nexcacheHandledPushReply(nexcacheContext *c, void *reply) {
    if (reply && c->push_cb && nexcacheIsPushReply(reply)) {
        c->push_cb(c->privdata, reply);
        return 1;
    }

    return 0;
}

/* Get a reply from our reader or set an error in the context. */
int nexcacheGetReplyFromReader(nexcacheContext *c, void **reply) {
    if (nexcacheReaderGetReply(c->reader, reply) == NEXCACHE_ERR) {
        nexcacheSetError(c, c->reader->err, c->reader->errstr);
        return NEXCACHE_ERR;
    }

    return NEXCACHE_OK;
}

/* Internal helper to get the next reply from our reader while handling
 * any PUSH messages we encounter along the way.  This is separate from
 * nexcacheGetReplyFromReader so as to not change its behavior. */
static int nexcacheNextInBandReplyFromReader(nexcacheContext *c, void **reply) {
    do {
        if (nexcacheGetReplyFromReader(c, reply) == NEXCACHE_ERR)
            return NEXCACHE_ERR;
    } while (nexcacheHandledPushReply(c, *reply));

    return NEXCACHE_OK;
}

int nexcacheGetReply(nexcacheContext *c, void **reply) {
    int wdone = 0;
    void *aux = NULL;

    /* Try to read pending replies */
    if (nexcacheNextInBandReplyFromReader(c, &aux) == NEXCACHE_ERR)
        return NEXCACHE_ERR;

    /* For the blocking context, flush output buffer and read reply */
    if (aux == NULL && c->flags & NEXCACHE_BLOCK) {
        /* Write until done */
        do {
            if (nexcacheBufferWrite(c, &wdone) == NEXCACHE_ERR)
                return NEXCACHE_ERR;
        } while (!wdone);

        /* Read until there is a reply */
        do {
            if (nexcacheBufferRead(c) == NEXCACHE_ERR)
                return NEXCACHE_ERR;

            if (nexcacheNextInBandReplyFromReader(c, &aux) == NEXCACHE_ERR)
                return NEXCACHE_ERR;
        } while (aux == NULL);
    }

    /* Set reply or free it if we were passed NULL */
    if (reply != NULL) {
        *reply = aux;
    } else {
        freeReplyObject(aux);
    }

    return NEXCACHE_OK;
}

/* Helper function for the nexcacheAppendCommand* family of functions.
 *
 * Write a formatted command to the output buffer. When this family
 * is used, you need to call nexcacheGetReply yourself to retrieve
 * the reply (or replies in pub/sub).
 */
int nexcacheAppendCmdLen(nexcacheContext *c, const char *cmd, size_t len) {
    sds newbuf;

    newbuf = sdscatlen(c->obuf, cmd, len);
    if (newbuf == NULL) {
        nexcacheSetError(c, NEXCACHE_ERR_OOM, "Out of memory");
        return NEXCACHE_ERR;
    }

    c->obuf = newbuf;
    return NEXCACHE_OK;
}

int nexcacheAppendFormattedCommand(nexcacheContext *c, const char *cmd, size_t len) {

    if (nexcacheAppendCmdLen(c, cmd, len) != NEXCACHE_OK) {
        return NEXCACHE_ERR;
    }

    return NEXCACHE_OK;
}

int nexcachevAppendCommand(nexcacheContext *c, const char *format, va_list ap) {
    char *cmd;
    int len;

    len = nexcachevFormatCommand(&cmd, format, ap);
    if (len == -1) {
        nexcacheSetError(c, NEXCACHE_ERR_OOM, "Out of memory");
        return NEXCACHE_ERR;
    } else if (len == -2) {
        nexcacheSetError(c, NEXCACHE_ERR_OTHER, "Invalid format string");
        return NEXCACHE_ERR;
    }

    if (nexcacheAppendCmdLen(c, cmd, len) != NEXCACHE_OK) {
        vk_free(cmd);
        return NEXCACHE_ERR;
    }

    vk_free(cmd);
    return NEXCACHE_OK;
}

int nexcacheAppendCommand(nexcacheContext *c, const char *format, ...) {
    va_list ap;
    int ret;

    va_start(ap, format);
    ret = nexcachevAppendCommand(c, format, ap);
    va_end(ap);
    return ret;
}

int nexcacheAppendCommandArgv(nexcacheContext *c, int argc, const char **argv, const size_t *argvlen) {
    sds cmd;
    long long len;

    len = nexcacheFormatSdsCommandArgv(&cmd, argc, argv, argvlen);
    if (len == -1) {
        nexcacheSetError(c, NEXCACHE_ERR_OOM, "Out of memory");
        return NEXCACHE_ERR;
    }

    if (nexcacheAppendCmdLen(c, cmd, len) != NEXCACHE_OK) {
        sdsfree(cmd);
        return NEXCACHE_ERR;
    }

    sdsfree(cmd);
    return NEXCACHE_OK;
}

/* Helper function for the nexcacheCommand* family of functions.
 *
 * Write a formatted command to the output buffer. If the given context is
 * blocking, immediately read the reply into the "reply" pointer. When the
 * context is non-blocking, the "reply" pointer will not be used and the
 * command is simply appended to the write buffer.
 *
 * Returns the reply when a reply was successfully retrieved. Returns NULL
 * otherwise. When NULL is returned in a blocking context, the error field
 * in the context will be set.
 */
static void *nexcacheBlockForReply(nexcacheContext *c) {
    void *reply;

    if (c->flags & NEXCACHE_BLOCK) {
        if (nexcacheGetReply(c, &reply) != NEXCACHE_OK)
            return NULL;
        return reply;
    }
    return NULL;
}

void *nexcachevCommand(nexcacheContext *c, const char *format, va_list ap) {
    if (nexcachevAppendCommand(c, format, ap) != NEXCACHE_OK)
        return NULL;
    return nexcacheBlockForReply(c);
}

void *nexcacheCommand(nexcacheContext *c, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    void *reply = nexcachevCommand(c, format, ap);
    va_end(ap);
    return reply;
}

void *nexcacheCommandArgv(nexcacheContext *c, int argc, const char **argv, const size_t *argvlen) {
    if (nexcacheAppendCommandArgv(c, argc, argv, argvlen) != NEXCACHE_OK)
        return NULL;
    return nexcacheBlockForReply(c);
}
