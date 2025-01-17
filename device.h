#ifndef FRIDAQML_DEVICE_H
#define FRIDAQML_DEVICE_H

#include <frida-core.h>

#include "iconprovider.h"
#include "maincontext.h"
#include "script.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QQueue>

class SessionEntry;
class ScriptEntry;

class Device : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Device)
    Q_PROPERTY(QString id READ id NOTIFY idChanged)
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(Type type READ type NOTIFY typeChanged)
    Q_ENUMS(Type)

public:
    explicit Device(FridaDevice *handle, QObject *parent = nullptr);
private:
    void dispose();
public:
    ~Device();

    FridaDevice *handle() const { return m_handle; }
    QString id() const { return m_id; }
    QString name() const { return m_name; }
    QUrl icon() const { return m_icon.url(); }
    enum Type { Local, Tether, Remote };
    Type type() const { return m_type; }

    Q_INVOKABLE void inject(Script *script, unsigned int pid);

signals:
    void idChanged(QString newId);
    void nameChanged(QString newName);
    void typeChanged(Type newType);

private:
    void performInject(unsigned int pid, ScriptInstance *wrapper);
    void performLoad(ScriptInstance *wrapper, QString name, QString source);
    void performStop(ScriptInstance *wrapper);
    void performPost(ScriptInstance *wrapper, QJsonObject object);
    void performEnableDebugger(ScriptInstance *wrapper, quint16 port);
    void performDisableDebugger(ScriptInstance *wrapper);
    void scheduleGarbageCollect();
    static gboolean onGarbageCollectTimeoutWrapper(gpointer data);
    void onGarbageCollectTimeout();

    FridaDevice *m_handle;
    QString m_id;
    QString m_name;
    Icon m_icon;
    Type m_type;

    QHash<unsigned int, SessionEntry *> m_sessions;
    QHash<ScriptInstance *, ScriptEntry *> m_scripts;
    GSource *m_gcTimer;

    MainContext m_mainContext;
};

class SessionEntry : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(SessionEntry)

public:
    explicit SessionEntry(Device *device, unsigned int pid, QObject *parent = nullptr);
    ~SessionEntry();

    QList<ScriptEntry *> scripts() const { return m_scripts; }

    ScriptEntry *add(ScriptInstance *wrapper);
    void remove(ScriptEntry *script);

    void enableDebugger(quint16 port);
    void disableDebugger();

signals:
    void detached();

private:
    static void onAttachReadyWrapper(GObject *obj, GAsyncResult *res, gpointer data);
    void onAttachReady(GAsyncResult *res);
    static void onDetachedWrapper(SessionEntry *self);
    void onDetached();

    Device *m_device;
    unsigned int m_pid;
    FridaSession *m_handle;
    QList<ScriptEntry *> m_scripts;
};

class ScriptEntry : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(ScriptEntry)

public:
    explicit ScriptEntry(SessionEntry *session, ScriptInstance *wrapper, QObject *parent = nullptr);
    ~ScriptEntry();

    SessionEntry *session() const { return m_session; }
    ScriptInstance *wrapper() const { return m_wrapper; }

    void updateSessionHandle(FridaSession *sessionHandle);
    void notifySessionError(GError *error);
    void notifySessionError(QString message);
    void load(QString name, QString source);
    void stop();
    void post(QJsonObject object);

signals:
    void stopped();

private:
    void updateStatus(ScriptInstance::Status status);
    void updateError(GError *error);
    void updateError(QString message);

    void start();
    static void onCreateReadyWrapper(GObject *obj, GAsyncResult *res, gpointer data);
    void onCreateReady(GAsyncResult *res);
    static void onLoadReadyWrapper(GObject *obj, GAsyncResult *res, gpointer data);
    void onLoadReady(GAsyncResult *res);
    void performPost(QJsonObject object);
    static void onMessage(ScriptEntry *self, const gchar *message, const gchar *data, gint dataSize);

    ScriptInstance::Status m_status;
    SessionEntry *m_session;
    ScriptInstance *m_wrapper;
    QString m_name;
    QString m_source;
    FridaScript *m_handle;
    FridaSession *m_sessionHandle;
    QQueue<QJsonObject> m_pending;
};

#endif
