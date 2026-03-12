#include <iostream>
using namespace std;

#include "async-qt.h"

#include <QCoreApplication>
#include <QTimer>

void getCallback(nexcacheAsyncContext *, void *r, void *privdata) {

    nexcacheReply *reply = static_cast<nexcacheReply *>(r);
    ExampleQt *ex = static_cast<ExampleQt *>(privdata);
    if (reply == nullptr || ex == nullptr)
        return;

    cout << "key: " << reply->str << endl;

    ex->finish();
}

void ExampleQt::run() {

    m_ctx = nexcacheAsyncConnect("localhost", 6379);

    if (m_ctx->err) {
        cerr << "Error: " << m_ctx->errstr << endl;
        nexcacheAsyncFree(m_ctx);
        emit finished();
    }

    m_adapter.setContext(m_ctx);

    nexcacheAsyncCommand(m_ctx, NULL, NULL, "SET key %s", m_value);
    nexcacheAsyncCommand(m_ctx, getCallback, this, "GET key");
}

int main(int argc, char **argv) {

    QCoreApplication app(argc, argv);

    ExampleQt example(argv[argc - 1]);

    QObject::connect(&example, SIGNAL(finished()), &app, SLOT(quit()));
    QTimer::singleShot(0, &example, SLOT(run()));

    return app.exec();
}
