/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: http://www.qt-project.org/
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QGUIGLCONTEXT_P_H
#define QGUIGLCONTEXT_P_H

#include "qopengl.h"
#include "qopenglcontext.h"
#include <private/qobject_p.h>
#include <qmutex.h>

#ifndef QT_NO_DEBUG
#include <QtCore/QHash>
#endif

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE


class QOpenGLFunctions;
class QOpenGLContext;
class QOpenGLMultiGroupSharedResource;

class Q_GUI_EXPORT QOpenGLSharedResource
{
public:
    QOpenGLSharedResource(QOpenGLContextGroup *group);
    virtual ~QOpenGLSharedResource() = 0;

    QOpenGLContextGroup *group() const { return m_group; }

    // schedule the resource for deletion at an appropriate time
    void free();

protected:
    // the resource's share group no longer exists, invalidate the resource
    virtual void invalidateResource() = 0;

    // a valid context in the group is current, free the resource
    virtual void freeResource(QOpenGLContext *context) = 0;

private:
    QOpenGLContextGroup *m_group;

    friend class QOpenGLContextGroup;
    friend class QOpenGLContextGroupPrivate;
    friend class QOpenGLMultiGroupSharedResource;

    Q_DISABLE_COPY(QOpenGLSharedResource);
};

class Q_GUI_EXPORT QOpenGLSharedResourceGuard : public QOpenGLSharedResource
{
public:
    typedef void (*FreeResourceFunc)(QOpenGLFunctions *functions, GLuint id);
    QOpenGLSharedResourceGuard(QOpenGLContext *context, GLuint id, FreeResourceFunc func)
        : QOpenGLSharedResource(context->shareGroup())
        , m_id(id)
        , m_func(func)
    {
    }

    GLuint id() const { return m_id; }

protected:
    void invalidateResource()
    {
        m_id = 0;
    }

    void freeResource(QOpenGLContext *context);

private:
    GLuint m_id;
    FreeResourceFunc m_func;
};

class Q_GUI_EXPORT QOpenGLContextGroupPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QOpenGLContextGroup);
public:
    QOpenGLContextGroupPrivate()
        : m_context(0)
        , m_mutex(QMutex::Recursive)
        , m_refs(0)
    {
    }

    void addContext(QOpenGLContext *ctx);
    void removeContext(QOpenGLContext *ctx);

    void cleanup();

    void deletePendingResources(QOpenGLContext *ctx);

    QOpenGLContext *m_context;

    QList<QOpenGLContext *> m_shares;
    QMutex m_mutex;

    QHash<QOpenGLMultiGroupSharedResource *, QOpenGLSharedResource *> m_resources;
    QAtomicInt m_refs;

    QList<QOpenGLSharedResource *> m_sharedResources;
    QList<QOpenGLSharedResource *> m_pendingDeletion;
};

class Q_GUI_EXPORT QOpenGLMultiGroupSharedResource
{
public:
    QOpenGLMultiGroupSharedResource();
    ~QOpenGLMultiGroupSharedResource();

    void insert(QOpenGLContext *context, QOpenGLSharedResource *value);
    void cleanup(QOpenGLContextGroup *group, QOpenGLSharedResource *value);

    QOpenGLSharedResource *value(QOpenGLContext *context);

    QList<QOpenGLSharedResource *> resources() const;

    template <typename T>
    T *value(QOpenGLContext *context) {
        QOpenGLContextGroup *group = context->shareGroup();
        T *resource = static_cast<T *>(group->d_func()->m_resources.value(this, 0));
        if (!resource) {
            resource = new T(context);
            insert(context, resource);
        }
        return resource;
    }

private:
    QAtomicInt active;
    QList<QOpenGLContextGroup *> m_groups;
};

class QPaintEngineEx;
class QOpenGLFunctions;

class Q_GUI_EXPORT QOpenGLContextPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QOpenGLContext)
public:
    QOpenGLContextPrivate()
        : qGLContextHandle(0)
        , platformGLContext(0)
        , shareContext(0)
        , shareGroup(0)
        , screen(0)
        , surface(0)
        , functions(0)
        , current_fbo(0)
        , workaround_brokenFBOReadBack(false)
        , workaround_brokenTexSubImage(false)
        , active_engine(0)
    {
    }

    virtual ~QOpenGLContextPrivate()
    {
        //do not delete the QOpenGLContext handle here as it is deleted in
        //QWidgetPrivate::deleteTLSysExtra()
    }

    void *qGLContextHandle;
    void (*qGLContextDeleteFunction)(void *handle);

    QSurfaceFormat requestedFormat;
    QPlatformOpenGLContext *platformGLContext;
    QOpenGLContext *shareContext;
    QOpenGLContextGroup *shareGroup;
    QScreen *screen;
    QSurface *surface;
    QOpenGLFunctions *functions;

    GLuint current_fbo;

    bool workaround_brokenFBOReadBack;
    bool workaround_brokenTexSubImage;

    QPaintEngineEx *active_engine;

    static void setCurrentContext(QOpenGLContext *context);

    int maxTextureSize() const { return 1024; }

#if !defined(QT_NO_DEBUG)
    static bool toggleMakeCurrentTracker(QOpenGLContext *context, bool value)
    {
        QMutexLocker locker(&makeCurrentTrackerMutex);
        bool old = makeCurrentTracker.value(context, false);
        makeCurrentTracker.insert(context, value);
        return old;
    }
    static void cleanMakeCurrentTracker(QOpenGLContext *context)
    {
        QMutexLocker locker(&makeCurrentTrackerMutex);
        makeCurrentTracker.remove(context);
    }
    static QHash<QOpenGLContext *, bool> makeCurrentTracker;
    static QMutex makeCurrentTrackerMutex;
#endif
};

QT_END_NAMESPACE

QT_END_HEADER

#endif // QGUIGLCONTEXT_P_H
