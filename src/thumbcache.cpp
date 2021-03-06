#include "thumbcache.h"
#include "thumbcache_p.h"
#include "share/wizDatabaseManager.h"
#include "share/wizDatabase.h"
#include "share/wizthreads.h"


#define THUMB_CACHE_MAX 10000


ThumbCachePrivate::ThumbCachePrivate(ThumbCache* cache)
    : q(cache)
{
    connect(CWizDatabaseManager::instance(), SIGNAL(documentAbstractModified(const WIZDOCUMENTDATA&)),
            SLOT(onNoteThumbChanged(const WIZDOCUMENTDATA&)));
    connect(this, SIGNAL(thumbLoaded(const QString&, const QString&)),
            cache, SIGNAL(loaded(const QString&, const QString&)));
}

QString ThumbCachePrivate::key(const QString& strKbGUID, const QString& strGUID)
{
    return strKbGUID + "::" + strGUID;
}

bool ThumbCachePrivate::find(const QString& strKbGUID, const QString& strGUID, WIZABSTRACT& abs)
{
    QString strKey(key(strKbGUID, strGUID));
    if (m_mapThumb.contains(strKey)) {
        abs = m_mapThumb.value(strKey);
        return true;
    }

    if (m_mapThumb.size() >= THUMB_CACHE_MAX) {
        m_mapThumb.clear();
        qDebug() << "[ThumCache]pool is full, clear...";
    }

    load(strKbGUID, strGUID);
    return false;
}

void ThumbCachePrivate::load(const QString& strKbGUID, const QString& strGUID)
{
    WizExecuteOnThread(WIZ_THREAD_DEFAULT, [=](){
        load_impl(strKbGUID, strGUID);
    });
}

void ThumbCachePrivate::load_impl(const QString& strKbGUID, const QString& strGUID)
{
    if (!CWizDatabaseManager::instance()->isOpened(strKbGUID)) {
        qDebug() << "[ThumbCache]discard for invalid kb: " << strKbGUID;
        return;
    }

    WIZABSTRACT abs;
    CWizDatabase& db = CWizDatabaseManager::instance()->db(strKbGUID);

    bool bUpdated = false;

    // update if not exist
    if (db.PadAbstractFromGUID(strGUID, abs)) {
        bUpdated = false;
    } else {
        qDebug() << "[ThumbCache]thumb not exist, try update: " << strGUID;
        if (db.UpdateDocumentAbstract(strGUID)) {
            bUpdated = true;
        } else {
            bUpdated = false;
        }
    }

    // load again if updated
    if (bUpdated && !db.PadAbstractFromGUID(strGUID, abs)) {
        qDebug() << "[ThumCache]failed to load thumb from db: " << strGUID;
    }

    abs.strKbGUID = strKbGUID;
    if (abs.text.isEmpty()) {
        abs.text = " ";
    }

    m_mapThumb.insert(key(strKbGUID, strGUID), abs);
    Q_EMIT thumbLoaded(strKbGUID, strGUID);
}

void ThumbCachePrivate::onNoteThumbChanged(const WIZDOCUMENTDATA& data)
{
    load(data.strKbGUID, data.strGUID);
}



ThumbCache* m_instance = 0;
ThumbCachePrivate* d = 0;

ThumbCache::ThumbCache()
{
    Q_ASSERT(!m_instance);

    m_instance = this;
    d = new ThumbCachePrivate(this);
}

ThumbCache* ThumbCache::instance()
{
    return m_instance;
}

ThumbCache::~ThumbCache()
{
    delete d;
    d = 0;
}

bool ThumbCache::find(const QString& strKbGUID, const QString& strGUID, WIZABSTRACT& abs)
{
    return d->find(strKbGUID, strGUID, abs);
}
