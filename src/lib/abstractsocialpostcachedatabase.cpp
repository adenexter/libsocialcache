/*
 * Copyright (C) 2013 Lucien Xu <sfietkonstantin@free.fr>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "abstractsocialpostcachedatabase.h"
#include "abstractsocialcachedatabase_p.h"
#include <QtCore/QDebug>
#include <QtCore/QStringList>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

static const char *INVALID = "invalid";
static const char *PHOTO = "photo";
static const char *VIDEO = "video";

struct SocialPostImagePrivate
{
    explicit SocialPostImagePrivate(const QString &url, SocialPostImage::ImageType type);
    QString url;
    SocialPostImage::ImageType type;
};

SocialPostImagePrivate::SocialPostImagePrivate(const QString &url, SocialPostImage::ImageType type)
    : url(url), type(type)
{
}

SocialPostImage::SocialPostImage()
{
}

SocialPostImage::SocialPostImage(const QString &url, ImageType type)
    : d_ptr(new SocialPostImagePrivate(url, type))
{
}

SocialPostImage::~SocialPostImage()
{
}

SocialPostImage::Ptr SocialPostImage::create(const QString &url, ImageType type)
{
    return SocialPostImage::Ptr(new SocialPostImage(url, type));
}

QString SocialPostImage::url() const
{
    Q_D(const SocialPostImage);
    return d->url;
}

SocialPostImage::ImageType SocialPostImage::type() const
{
    Q_D(const SocialPostImage);
    return d->type;
}

struct SocialPostPrivate
{
    explicit SocialPostPrivate(const QString &identifier, const QString &name,
                               const QString &body, const QDateTime &timestamp,
                               const QVariantMap &extra = QVariantMap(),
                               const QList<int> &accounts = QList<int>());
    QString identifier;
    QString name;
    QString body;
    QDateTime timestamp;
    QMap<int, SocialPostImage::ConstPtr> images;
    QVariantMap extra;
    QList<int> accounts;
};

SocialPostPrivate::SocialPostPrivate(const QString &identifier, const QString &name,
                                     const QString &body, const QDateTime &timestamp,
                                     const QVariantMap &extra, const QList<int> &accounts)
    : identifier(identifier), name(name), body(body), timestamp(timestamp)
    , extra(extra), accounts(accounts)
{
}

SocialPost::SocialPost(const QString &identifier, const QString &name, const QString &body,
                       const QDateTime &timestamp,
                       const QMap<int, SocialPostImage::ConstPtr> &images, const QVariantMap &extra,
                       const QList<int> &accounts)
    : d_ptr(new SocialPostPrivate(identifier, body, name, timestamp ,extra,
                                   accounts))
{
    setImages(images);
}

SocialPost::~SocialPost()
{
}

SocialPost::Ptr SocialPost::create(const QString &identifier, const QString &name,
                                   const QString &body, const QDateTime &timestamp,
                                   const QMap<int, SocialPostImage::ConstPtr> &images,
                                   const QVariantMap &extra, const QList<int> &accounts)
{
    return SocialPost::Ptr(new SocialPost(identifier, name, body, timestamp, images, extra,
                                          accounts));
}

QString SocialPost::identifier() const
{
    Q_D(const SocialPost);
    return d->identifier;
}

QString SocialPost::name() const
{
    Q_D(const SocialPost);
    return d->name;
}

QString SocialPost::body() const
{
    Q_D(const SocialPost);
    return d->body;
}

QDateTime SocialPost::timestamp() const
{
    Q_D(const SocialPost);
    return d->timestamp;
}

QString SocialPost::icon() const
{
    Q_D(const SocialPost);
    if (d->images.isEmpty()) {
        return QString();
    }

    return d->images.value(0)->url();
}

QList<SocialPostImage::ConstPtr> SocialPost::images() const
{
    Q_D(const SocialPost);
    QList<SocialPostImage::ConstPtr> images;
    foreach (int key, d->images.keys()) {
        if (key > 0) {
            images.append(d->images.value(key));
        }
    }

    return images;
}

QMap<int, SocialPostImage::ConstPtr> SocialPost::allImages() const
{
    Q_D(const SocialPost);
    return d->images;
}

void SocialPost::setImages(const QMap<int, SocialPostImage::ConstPtr> &images)
{
    Q_D(SocialPost);
    d->images = images;
}

QVariantMap SocialPost::extra() const
{
    Q_D(const SocialPost);
    return d->extra;
}

void SocialPost::setExtra(const QVariantMap &extra)
{
    Q_D(SocialPost);
    d->extra = extra;
}

QList<int> SocialPost::accounts() const
{
    Q_D(const SocialPost);
    return d->accounts;
}

void SocialPost::setAccounts(const QList<int> &accounts)
{
    Q_D(SocialPost);
    d->accounts = accounts;
}

class AbstractSocialPostCacheDatabasePrivate: public AbstractSocialCacheDatabasePrivate
{
public:
    AbstractSocialPostCacheDatabasePrivate(AbstractSocialPostCacheDatabase *q);
private:
    static void createPostsEntries(const QMap<QString, SocialPost::ConstPtr> &posts,
                                    QStringList &postKeys,
                                    QStringList &imageKeys, QStringList &extraKeys,
                                    QMap<QString, QVariantList> &postEntries,
                                    QMap<QString, QVariantList> &imageEntries,
                                    QMap<QString, QVariantList> &extraEntries);
    static void createAccountsEntries(const QMultiMap<QString, int> &accounts,
                                      QStringList &keys,
                                      QMap<QString, QVariantList> &entries);
    QMap<QString, SocialPost::ConstPtr> queuedPosts;
    QMultiMap<QString, int> queuedPostsAccounts;
    QList<int> queuedRemovePostsForAccount;

    QSqlQuery postQuery;
    QSqlQuery imageQuery;
    QSqlQuery extraQuery;
    QSqlQuery accountQuery;

    Q_DECLARE_PUBLIC(AbstractSocialPostCacheDatabase)
};

AbstractSocialPostCacheDatabasePrivate::AbstractSocialPostCacheDatabasePrivate(AbstractSocialPostCacheDatabase *q)
    : AbstractSocialCacheDatabasePrivate(q)
{
}

void AbstractSocialPostCacheDatabasePrivate::createPostsEntries(const QMap<QString, SocialPost::ConstPtr> &posts,
                                                                 QStringList &postKeys,
                                                                 QStringList &imageKeys,
                                                                 QStringList &extraKeys,
                                                                 QMap<QString, QVariantList> &postEntries,
                                                                 QMap<QString, QVariantList> &imageEntries,
                                                                 QMap<QString, QVariantList> &extraEntries)
{
    postKeys.clear();
    imageKeys.clear();
    extraKeys.clear();
    postKeys << QLatin1String("identifier") << QLatin1String("name") << QLatin1String("body")
               << QLatin1String("timestamp");
    imageKeys << QLatin1String("postId") << QLatin1String("position") << QLatin1String("url")
               << QLatin1String("type");
    extraKeys << QLatin1String("postId") << QLatin1String("key") << QLatin1String("value");

    postEntries.clear();
    imageEntries.clear();
    extraEntries.clear();

    foreach (const SocialPost::ConstPtr &post, posts) {
        postEntries[QLatin1String("identifier")].append(post->identifier());
        postEntries[QLatin1String("name")].append(post->name());
        postEntries[QLatin1String("body")].append(post->body());
        postEntries[QLatin1String("timestamp")].append(post->timestamp().toTime_t());


        foreach (int key, post->allImages().keys()) {
            const SocialPostImage::ConstPtr image = post->allImages().value(key);
            imageEntries[QLatin1String("postId")].append(post->identifier());
            imageEntries[QLatin1String("position")].append(key);
            imageEntries[QLatin1String("url")].append(image->url());
            switch (image->type()) {
            case SocialPostImage::Photo:
                imageEntries[QLatin1String("type")].append(QLatin1String(PHOTO));
                break;
            case SocialPostImage::Video:
                imageEntries[QLatin1String("type")].append(QLatin1String(VIDEO));
                break;
            default:
                imageEntries[QLatin1String("type")].append(QLatin1String(INVALID));
                break;
            }
        }

        QVariantMap extra = post->extra();
        foreach (const QString &key, extra.keys()) {
            extraEntries[QLatin1String("postId")].append(post->identifier());
            extraEntries[QLatin1String("key")].append(key);
            extraEntries[QLatin1String("value")].append(extra.value(key).toString());
        }
    }

}

void AbstractSocialPostCacheDatabasePrivate::createAccountsEntries(const QMultiMap<QString, int> &accounts,
                                                                   QStringList &keys,
                                                                   QMap<QString, QVariantList> &entries)
{
    keys.clear();
    keys << QLatin1String("postId") << QLatin1String("account");

    entries.clear();

    foreach (const QString &key, accounts.keys()) {
        foreach (int value, accounts.values(key)) {
            entries[QLatin1String("postId")].append(key);
            entries[QLatin1String("account")].append(value);
        }
    }
}

AbstractSocialPostCacheDatabase::AbstractSocialPostCacheDatabase()
    : AbstractSocialCacheDatabase(*(new AbstractSocialPostCacheDatabasePrivate(this)))
{
}

QList<SocialPost::ConstPtr> AbstractSocialPostCacheDatabase::posts() const
{
    // This might be slow
    AbstractSocialPostCacheDatabasePrivate * const d = const_cast<AbstractSocialPostCacheDatabasePrivate *>(d_func());

    if (!d->postQuery.exec()) {
        qWarning() << Q_FUNC_INFO << "Error reading from posts table:" << d->postQuery.lastError();
        return QList<SocialPost::ConstPtr>();
    }

    QList<SocialPost::ConstPtr> posts;
    while (d->postQuery.next()) {
        QString identifier = d->postQuery.value(0).toString();

        QString name = d->postQuery.value(1).toString();
        QString body = d->postQuery.value(2).toString();
        int timestamp = d->postQuery.value(3).toInt();
        SocialPost::Ptr post = SocialPost::create(identifier, name, body,
                                                  QDateTime::fromTime_t(timestamp));

        d->imageQuery.bindValue(":postId", identifier);

        QMap<int, SocialPostImage::ConstPtr> images;
        if (d->imageQuery.exec()) {
            while (d->imageQuery.next()) {
                SocialPostImage::ImageType type = SocialPostImage::Invalid;
                QString typeString = d->imageQuery.value(2).toString();
                if (typeString == QLatin1String(PHOTO)) {
                    type = SocialPostImage::Photo;
                } else if (typeString == QLatin1String(VIDEO)) {
                    type = SocialPostImage::Video;
                }

                int position = d->imageQuery.value(0).toInt();
                SocialPostImage::Ptr image  = SocialPostImage::create(d->imageQuery.value(1).toString(),
                                                                      type);
                images.insert(position, image);
            }
            post->setImages(images);
        } else {
            qWarning() << Q_FUNC_INFO << "Error reading from images table:"
                       << d->imageQuery.lastError();
        }

        d->extraQuery.bindValue(":postId", identifier);

        QVariantMap extra;
        if (d->extraQuery.exec()) {
            while (d->extraQuery.next()) {
                QString key = d->extraQuery.value(0).toString();
                QVariant value = d->extraQuery.value(1);
                extra.insert(key, value);
            }
        } else {
            qWarning() << Q_FUNC_INFO << "Error reading from extra table:"
                       << d->extraQuery.lastError();
        }

        post->setExtra(extra);

        d->accountQuery.bindValue(":postId", identifier);

        QList<int> accounts;
        if (d->accountQuery.exec()) {
            while (d->accountQuery.next()) {
                accounts.append(d->accountQuery.value(0).toInt());
            }
        }

        post->setAccounts(accounts);

        posts.append(post);
    }

    return posts;
}

void AbstractSocialPostCacheDatabase::addPost(const QString &identifier, const QString &name,
                                              const QString &body, const QDateTime &timestamp,
                                              const QString &icon,
                                              const QList<QPair<QString, SocialPostImage::ImageType> > &images,
                                              const QVariantMap &extra, int account)
{
    Q_D(AbstractSocialPostCacheDatabase);
    QMap<int, SocialPostImage::ConstPtr> formattedImages;
    if (!icon.isEmpty()) {
        formattedImages.insert(0, SocialPostImage::create(icon, SocialPostImage::Photo));
    }

    for (int i = 0; i < images.count(); i++) {
        const QPair<QString, SocialPostImage::ImageType> &imagePair = images.at(i);
        formattedImages.insert(i + 1, SocialPostImage::create(imagePair.first, imagePair.second));
    }

    d->queuedPosts.insert(identifier, SocialPost::create(identifier, name, body, timestamp,
                                                         formattedImages, extra));
    d->queuedPostsAccounts.insert(identifier, account);
}

void AbstractSocialPostCacheDatabase::removePosts(int accountId)
{
    Q_D(AbstractSocialPostCacheDatabase);
    if (!d->queuedRemovePostsForAccount.contains(accountId)) {
        d->queuedRemovePostsForAccount.append(accountId);
    }
}

bool AbstractSocialPostCacheDatabase::write()
{
    Q_D(AbstractSocialPostCacheDatabase);
    if (!dbBeginTransaction()) {
        return false;
    }

    QStringList postKeys;
    QStringList imageKeys;
    QStringList extraKeys;

    QMap<QString, QVariantList> postEntries;
    QMap<QString, QVariantList> imageEntries;
    QMap<QString, QVariantList> extraEntries;

    // perform removals first.
    if (d->queuedRemovePostsForAccount.size()) {
        QSqlQuery postIdsToRemoveQuery(d->db);
        if (!postIdsToRemoveQuery.prepare(QString::fromLatin1(
                "SELECT PostId FROM link_post_account WHERE account = :accid"))) {
            qWarning() << "Failed to prepare PostIdsToRemoveQuery:" << postIdsToRemoveQuery.lastError();
            dbRollbackTransaction();
            return false;
        }

        QVariantList postIdsToRemove;
        foreach (int accountId, d->queuedRemovePostsForAccount) {
            postIdsToRemoveQuery.bindValue(":accid", accountId);
            if (!postIdsToRemoveQuery.exec()) {
                qWarning() << "Failed to exec PostIdsToRemoveQuery:" << postIdsToRemoveQuery.lastError();
                dbRollbackTransaction();
                return false;
            }

            while (postIdsToRemoveQuery.next()) {
                postIdsToRemove.append(postIdsToRemoveQuery.value(0).toString());
            }
        }
        d->queuedRemovePostsForAccount.clear();

        if (postIdsToRemove.size()) {
            QMap<QString, QVariantList> deletePostsEntries, deleteOtherTablesEntries;
            deleteOtherTablesEntries.insert(QString::fromLatin1("postId"), postIdsToRemove);
            deletePostsEntries.insert(QString::fromLatin1("identifier"), postIdsToRemove);
            QStringList deletePostsKeys("identifier"), deleteOtherTablesKeys("postId");
            if (!dbWrite(QLatin1String("link_post_account"), deleteOtherTablesKeys, deleteOtherTablesEntries, Delete)
                    || !dbWrite(QLatin1String("extra"), deleteOtherTablesKeys, deleteOtherTablesEntries, Delete)
                    || !dbWrite(QLatin1String("images"), deleteOtherTablesKeys, deleteOtherTablesEntries, Delete)
                    || !dbWrite(QLatin1String("posts"), deletePostsKeys, deletePostsEntries, Delete)) {
                dbRollbackTransaction();
                return false;
            }
        }
    }

    // then perform additions.
    d->createPostsEntries(d->queuedPosts, postKeys, imageKeys, extraKeys, postEntries,
                          imageEntries, extraEntries);

    if (!dbWrite(QLatin1String("posts"), postKeys, postEntries, InsertOrReplace)) {
        dbRollbackTransaction();
        return false;
    }

    if (!dbWrite(QLatin1String("images"), imageKeys, imageEntries, InsertOrReplace)) {
        dbRollbackTransaction();
        return false;
    }

    if (!dbWrite(QLatin1String("extra"), extraKeys, extraEntries, InsertOrReplace)) {
        dbRollbackTransaction();
        return false;
    }

    QStringList keys;
    QMap<QString, QVariantList> entries;
    d->createAccountsEntries(d->queuedPostsAccounts, keys, entries);
    if (!dbWrite(QLatin1String("link_post_account"), keys, entries, InsertOrReplace)) {
        dbRollbackTransaction();
        return false;
    }

    if (!dbCommitTransaction()) {
        dbRollbackTransaction();
        return false;
    }

    d->queuedPosts.clear();
    d->queuedPostsAccounts.clear();

    return true;
}

bool AbstractSocialPostCacheDatabase::dbCreateTables()
{
    Q_D(AbstractSocialPostCacheDatabase);
    QSqlQuery query (d->db);

    // Heavily inspired from libeventfeeds
    // posts is composed of
    // * identifier is the identifier of the data (from social
    //    network, like the facebook id)
    // * name is the displayed name of the poster. Twitter, that
    //   requires both the name and "screen name" of the poster,
    //   uses an extra field, passed to the extra table.
    // * body is the content of the entry.
    // * timestamp is the timestamp, converted to milliseconds
    //   from epoch (makes sorting easier).
    query.prepare( "CREATE TABLE IF NOT EXISTS posts ("\
                   "identifier TEXT UNIQUE PRIMARY KEY,"\
                   "name TEXT,"\
                   "body TEXT,"\
                   "timestamp INTEGER)");
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << "Unable to create posts table" << query.lastError().text();
        d->db.close();
        return false;
    }

    query.prepare("CREATE TABLE IF NOT EXISTS images ("\
                  "postId TEXT, "\
                  "position INTEGER, "\
                  "url TEXT, "\
                  "type TEXT)");
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << "Unable to create images table" << query.lastError().text();
        d->db.close();
        return false;
    }

    query.prepare("CREATE TABLE IF NOT EXISTS extra ("\
                  "postId TEXT, "\
                  "key TEXT, "\
                  "value TEXT)");
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << "Unable to create extra table" << query.lastError().text();
        d->db.close();
        return false;
    }

    query.prepare("CREATE TABLE IF NOT EXISTS link_post_account ("\
                  "postId TEXT, "\
                  "account INTEGER, "\
                  "CONSTRAINT id PRIMARY KEY (postId, account))");
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << "Unable to create link_post_account table"
                   << query.lastError().text();
        return false;
    }

    if (!dbCreatePragmaVersion(POST_DB_VERSION)) {
        return false;
    }


    d->postQuery = QSqlQuery(d->db);
    if (!d->postQuery.prepare("SELECT identifier, name, body, timestamp FROM posts "\
                  "ORDER BY timestamp DESC")) {
        qWarning() << Q_FUNC_INFO << "Failed to prepare posts query" << d->postQuery.lastError();
    }

    d->imageQuery = QSqlQuery(d->db);
    if (!d->imageQuery.prepare("SELECT position, url, type FROM images WHERE postId = :postId")) {
        qWarning() << Q_FUNC_INFO << "Failed to prepare images query" << d->imageQuery.lastError();
    }

    d->extraQuery = QSqlQuery(d->db);
    if (!d->extraQuery.prepare("SELECT key, value FROM extra WHERE postId = :postId")) {
        qWarning() << Q_FUNC_INFO << "Failed to prepare extras query" << d->extraQuery.lastError();
    }

    d->accountQuery = QSqlQuery(d->db);
    if (!d->accountQuery.prepare("SELECT account FROM link_post_account WHERE postId = :postId")) {
        qWarning() << Q_FUNC_INFO << "Failed to prepare accounts query" << d->accountQuery.lastError();
    }

    return true;
}

bool AbstractSocialPostCacheDatabase::dbDropTables()
{
    Q_D(AbstractSocialPostCacheDatabase);
    QSqlQuery query(d->db);
    query.prepare("DROP TABLE IF EXISTS posts");
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << "Unable to delete posts table"
                   << query.lastError().text();
        return false;
    }

    query.prepare("DROP TABLE IF EXISTS images");
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << "Unable to delete images table"
                   << query.lastError().text();
        return false;
    }

    query.prepare("DROP TABLE IF EXISTS extra");
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << "Unable to delete extra table"
                   << query.lastError().text();
        return false;
    }

    query.prepare("DROP TABLE IF EXISTS link_post_account");
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << "Unable to delete link_post_account table"
                   << query.lastError().text();
        return false;
    }

    return true;
}
