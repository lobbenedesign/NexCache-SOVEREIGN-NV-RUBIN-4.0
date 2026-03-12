#ifndef NEXCACHE_EXAMPLE_QT_H
#define NEXCACHE_EXAMPLE_QT_H

#include <nexcache/adapters/qt.h>

class ExampleQt : public QObject {

    Q_OBJECT

  public:
    ExampleQt(const char *value, QObject *parent = 0)
        : QObject(parent), m_value(value) {}

  signals:
    void finished();

  public slots:
    void run();

  private:
    void finish() { emit finished(); }

  private:
    const char *m_value;
    nexcacheAsyncContext *m_ctx;
    NexCacheQtAdapter m_adapter;

    friend void getCallback(nexcacheAsyncContext *, void *, void *);
};

#endif /* NEXCACHE_EXAMPLE_QT_H */
