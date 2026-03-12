/*-
 * Copyright (C) 2014 Pietro Cerutti <gahr@gahr.ch>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef NEXCACHE_ADAPTERS_QT_H
#define NEXCACHE_ADAPTERS_QT_H
#include "../async.h"

#include <QSocketNotifier>

static void NexCacheQtAddRead(void *);
static void NexCacheQtDelRead(void *);
static void NexCacheQtAddWrite(void *);
static void NexCacheQtDelWrite(void *);
static void NexCacheQtCleanup(void *);

class NexCacheQtAdapter : public QObject {

    Q_OBJECT

    friend void NexCacheQtAddRead(void *adapter) {
        NexCacheQtAdapter *a = static_cast<NexCacheQtAdapter *>(adapter);
        a->addRead();
    }

    friend void NexCacheQtDelRead(void *adapter) {
        NexCacheQtAdapter *a = static_cast<NexCacheQtAdapter *>(adapter);
        a->delRead();
    }

    friend void NexCacheQtAddWrite(void *adapter) {
        NexCacheQtAdapter *a = static_cast<NexCacheQtAdapter *>(adapter);
        a->addWrite();
    }

    friend void NexCacheQtDelWrite(void *adapter) {
        NexCacheQtAdapter *a = static_cast<NexCacheQtAdapter *>(adapter);
        a->delWrite();
    }

    friend void NexCacheQtCleanup(void *adapter) {
        NexCacheQtAdapter *a = static_cast<NexCacheQtAdapter *>(adapter);
        a->cleanup();
    }

  public:
    NexCacheQtAdapter(QObject *parent = 0)
        : QObject(parent), m_ctx(0), m_read(0), m_write(0) {}

    ~NexCacheQtAdapter() {
        if (m_ctx != 0) {
            m_ctx->ev.data = NULL;
        }
    }

    int setContext(nexcacheAsyncContext *ac) {
        if (ac->ev.data != NULL) {
            return NEXCACHE_ERR;
        }
        m_ctx = ac;
        m_ctx->ev.data = this;
        m_ctx->ev.addRead = NexCacheQtAddRead;
        m_ctx->ev.delRead = NexCacheQtDelRead;
        m_ctx->ev.addWrite = NexCacheQtAddWrite;
        m_ctx->ev.delWrite = NexCacheQtDelWrite;
        m_ctx->ev.cleanup = NexCacheQtCleanup;
        return NEXCACHE_OK;
    }

  private:
    void addRead() {
        if (m_read)
            return;
        m_read = new QSocketNotifier(m_ctx->c.fd, QSocketNotifier::Read, 0);
        connect(m_read, SIGNAL(activated(int)), this, SLOT(read()));
    }

    void delRead() {
        if (!m_read)
            return;
        delete m_read;
        m_read = 0;
    }

    void addWrite() {
        if (m_write)
            return;
        m_write = new QSocketNotifier(m_ctx->c.fd, QSocketNotifier::Write, 0);
        connect(m_write, SIGNAL(activated(int)), this, SLOT(write()));
    }

    void delWrite() {
        if (!m_write)
            return;
        delete m_write;
        m_write = 0;
    }

    void cleanup() {
        delRead();
        delWrite();
    }

  private slots:
    void read() { nexcacheAsyncHandleRead(m_ctx); }
    void write() { nexcacheAsyncHandleWrite(m_ctx); }

  private:
    nexcacheAsyncContext *m_ctx;
    QSocketNotifier *m_read;
    QSocketNotifier *m_write;
};

#endif /* NEXCACHE_ADAPTERS_QT_H */
