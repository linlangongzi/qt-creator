/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://qt.nokia.com/contact.
**
**************************************************************************/

#include "breakhandler.h"

#include "debuggeractions.h"
#include "debuggermanager.h"
#include "debuggerstringutils.h"
#include "stackframe.h"

#include <texteditor/basetextmark.h>
#include <utils/qtcassert.h>

#include <QtCore/QByteArray>
#include <QtCore/QDebug>
#include <QtCore/QTextStream>
#include <QtCore/QFileInfo>

using namespace Debugger;
using namespace Debugger::Internal;


//////////////////////////////////////////////////////////////////
//
// BreakpointMarker
//
//////////////////////////////////////////////////////////////////

// Compare file names case insensitively on Windows.
static inline bool fileNameMatch(const QString &f1, const QString &f2)
{
#ifdef Q_OS_WIN
    return f1.compare(f2, Qt::CaseInsensitive) == 0;
#else
    return f1 == f2;
#endif
}

namespace Debugger {
namespace Internal {

// The red blob on the left side in the cpp editor.
class BreakpointMarker : public TextEditor::BaseTextMark
{
    Q_OBJECT
public:
    BreakpointMarker(BreakpointData *data, const QString &fileName, int lineNumber)
      : BaseTextMark(fileName, lineNumber)
    {
        m_data = data;
        m_pending = true;
        m_enabled = true;
        //qDebug() << "CREATE MARKER " << fileName << lineNumber;
    }

    ~BreakpointMarker()
    {
        //qDebug() << "REMOVE MARKER ";
        m_data = 0;
    }

    QIcon icon() const
    {
        const BreakHandler *handler = m_data->handler();
        if (!m_enabled)
            return handler->disabledBreakpointIcon();
        return m_pending ? handler->pendingBreakPointIcon() : handler->breakpointIcon();
    }

    void setPending(bool pending, bool enabled)
    {
        if (pending == m_pending && enabled == m_enabled)
            return;
        m_pending = pending;
        m_enabled = enabled;
        updateMarker();
    }

    void updateBlock(const QTextBlock &)
    {
        //qDebug() << "BREAKPOINT MARKER UPDATE BLOCK";
    }

    void removedFromEditor()
    {
        if (!m_data)
            return;

        BreakHandler *handler = m_data->handler();
        handler->removeBreakpoint(m_data);
        handler->saveBreakpoints();
        handler->updateMarkers();
    }

    void updateLineNumber(int lineNumber)
    {
        if (!m_data)
            return;
        //if (m_data->markerLineNumber == lineNumber)
        //    return;
        if (m_data->markerLineNumber() != lineNumber) {
            m_data->setMarkerLineNumber(lineNumber);
            // FIXME: Should we tell gdb about the change?
            // Ignore it for now, as we would require re-compilation
            // and debugger re-start anyway.
            if (0 && !m_data->bpLineNumber.isEmpty()) {
                if (!m_data->bpNumber.trimmed().isEmpty()) {
                    m_data->pending = true;
                }
            }
        }
        // Ignore updates to the "real" line number while the debugger is
        // running, as this can be triggered by moving the breakpoint to
        // the next line that generated code. 
        // FIXME: Do we need yet another data member?
        if (m_data->bpNumber.trimmed().isEmpty()) {
            m_data->lineNumber = QByteArray::number(lineNumber);
            m_data->handler()->updateMarkers();
        }
    }

private:
    BreakpointData *m_data;
    bool m_pending;
    bool m_enabled;
};

} // namespace Internal
} // namespace Debugger



//////////////////////////////////////////////////////////////////
//
// BreakpointData
//
//////////////////////////////////////////////////////////////////

BreakpointData::BreakpointData()
{
    m_handler = 0;
    enabled = true;
    pending = true;
    type = BreakpointType;
    marker = 0;
    m_markerLineNumber = 0;
    bpMultiple = false;
    useFullPath = false;
}

BreakpointData::~BreakpointData()
{
    removeMarker();
}

void BreakpointData::removeMarker()
{
    BreakpointMarker *m = marker;
    marker = 0;
    delete m;
}

void BreakpointData::updateMarker()
{
    if (marker && (m_markerFileName != marker->fileName()
            || m_markerLineNumber != marker->lineNumber()))
        removeMarker();

    if (!marker && !m_markerFileName.isEmpty() && m_markerLineNumber > 0)
        marker = new BreakpointMarker(this, m_markerFileName, m_markerLineNumber);

    if (marker)
        marker->setPending(pending, enabled);
}

void BreakpointData::setMarkerFileName(const QString &fileName)
{
    m_markerFileName = fileName;
}

void BreakpointData::setMarkerLineNumber(int lineNumber)
{
    m_markerLineNumber = lineNumber;
}

QString BreakpointData::toToolTip() const
{
    QString rc;
    QTextStream str(&rc);
    str << "<html><body><table>"
        << "<tr><td>" << BreakHandler::tr("Marker File:")
        << "</td><td>" << m_markerFileName << "</td></tr>"
        << "<tr><td>" << BreakHandler::tr("Marker Line:")
        << "</td><td>" << m_markerLineNumber << "</td></tr>"
        << "<tr><td>" << BreakHandler::tr("Breakpoint Number:")
        << "</td><td>" << bpNumber << "</td></tr>"
        << "<tr><td>" << BreakHandler::tr("Breakpoint Type:")
        << "</td><td>"
        << (type == BreakpointType ? BreakHandler::tr("Breakpoint") 
            : type == WatchpointType ? BreakHandler::tr("Watchpoint")
            : BreakHandler::tr("Unknown breakpoint type"))
        << "</td></tr>"
        << "</table><br><hr><table>"
        << "<tr><th>" << BreakHandler::tr("Property")
        << "</th><th>" << BreakHandler::tr("Requested")
        << "</th><th>" << BreakHandler::tr("Obtained") << "</th></tr>"
        << "<tr><td>" << BreakHandler::tr("Internal Number:")
        << "</td><td>&mdash;</td><td>" << bpNumber << "</td></tr>"
        << "<tr><td>" << BreakHandler::tr("File Name:")
        << "</td><td>" << fileName << "</td><td>" << bpFileName << "</td></tr>"
        << "<tr><td>" << BreakHandler::tr("Function Name:")
        << "</td><td>" << funcName << "</td><td>" << bpFuncName << "</td></tr>"
        << "<tr><td>" << BreakHandler::tr("Line Number:")
        << "</td><td>" << lineNumber << "</td><td>" << bpLineNumber << "</td></tr>"
        << "<tr><td>" << BreakHandler::tr("Breakpoint Address:")
        << "</td><td>" << address << "</td><td>" << bpAddress << "</td></tr>"
        << "<tr><td>" << BreakHandler::tr("Corrected Line Number:")
        << "</td><td>-</td><td>" << bpCorrectedLineNumber << "</td></tr>"
        << "<tr><td>" << BreakHandler::tr("Condition:")
        << "</td><td>" << condition << "</td><td>" << bpCondition << "</td></tr>"
        << "<tr><td>" << BreakHandler::tr("Ignore Count:")
        << "</td><td>" << ignoreCount << "</td><td>" << bpIgnoreCount << "</td></tr>"
        << "<tr><td>" << BreakHandler::tr("Thread Specification:")
        << "</td><td>" << threadSpec << "</td><td>" << bpThreadSpec << "</td></tr>"
        << "</table></body></html>";
    return rc;
}

QString BreakpointData::toString() const
{
    QString rc;
    QTextStream str(&rc);
    str << BreakHandler::tr("Marker File:") << ' ' <<  m_markerFileName << '\n'
        << BreakHandler::tr("Marker Line:") <<  ' ' << m_markerLineNumber << '\n'
        << BreakHandler::tr("Breakpoint Number:") <<  ' ' << bpNumber << '\n'
        << BreakHandler::tr("Breakpoint Type:") << ' '
        << (type == BreakpointType ? BreakHandler::tr("Breakpoint") 
            : type == WatchpointType ? BreakHandler::tr("Watchpoint")
            : BreakHandler::tr("Unknown breakpoint type")) << '\n'
        << BreakHandler::tr("File Name:") << ' '
        << fileName << " -- " << bpFileName << '\n'
        << BreakHandler::tr("Function Name:") << ' '
        << funcName << " -- " << bpFuncName << '\n'
        << BreakHandler::tr("Line Number:") << ' '
        << lineNumber << " -- " << bpLineNumber << '\n'
        << BreakHandler::tr("Breakpoint Address:") << ' '
        << address << " -- " << bpAddress << '\n'
        << BreakHandler::tr("Condition:") << ' '
        << condition << " -- " << bpCondition << '\n'
        << BreakHandler::tr("Ignore Count:") << ' '
        << ignoreCount << " -- " << bpIgnoreCount << '\n'
        << BreakHandler::tr("Thread Specification:") << ' '
        << threadSpec << " -- " << bpThreadSpec << '\n';
    return rc;
}

bool BreakpointData::isLocatedAt(const QString &fileName_, int lineNumber_) const
{
    /*
    if (lineNumber != QString::number(lineNumber_))
        return false;
    if (fileName == fileName_)
        return true;
    if (fileName_.endsWith(fileName))
        return true;
    return false;
    */
    return lineNumber_ == m_markerLineNumber
        && fileNameMatch(fileName_, m_markerFileName);
}

bool BreakpointData::conditionsMatch() const
{
    // Some versions of gdb "beautify" the passed condition.
    QString s1 = condition;
    s1.remove(QChar(' '));
    QString s2 = bpCondition;
    s2.remove(QChar(' '));
    return s1 == s2;
}


//////////////////////////////////////////////////////////////////
//
// BreakHandler
//
//////////////////////////////////////////////////////////////////

BreakHandler::BreakHandler(DebuggerManager *manager, QObject *parent) :
    QAbstractTableModel(parent),
    m_breakpointIcon(_(":/debugger/images/breakpoint_16.png")),
    m_disabledBreakpointIcon(_(":/debugger/images/breakpoint_disabled_16.png")),
    m_pendingBreakPointIcon(_(":/debugger/images/breakpoint_pending_16.png")),
    m_watchpointIcon(_(":/debugger/images/watchpoint.png")),
    m_manager(manager)
{
}

BreakHandler::~BreakHandler()
{
    clear();
}

int BreakHandler::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 8;
}

int BreakHandler::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : size();
}

bool BreakHandler::hasPendingBreakpoints() const
{
    for (int i = size() - 1; i >= 0; i--)
        if (at(i)->pending)
            return true;
    return false;
}

BreakpointData *BreakHandler::at(int index) const
{
    QTC_ASSERT(index < size(), return 0);
    return m_bp.at(index);
}

void BreakHandler::removeAt(int index)
{
    BreakpointData *data = at(index);
    m_bp.removeAt(index);
    delete data;
}

void BreakHandler::clear()
{
    qDeleteAll(m_bp);
    m_bp.clear();
    m_enabled.clear();
    m_disabled.clear();
    m_removed.clear();
    m_inserted.clear();
}

BreakpointData *BreakHandler::findSimilarBreakpoint(const BreakpointData &needle) const
{
    // Search a breakpoint we might refer to.
    for (int index = 0; index != size(); ++index) {
        BreakpointData *data = m_bp[index];
        // Clear hit.
        if (data->bpNumber == needle.bpNumber
                && !data->bpNumber.isEmpty()
                && data->bpNumber.toInt() != 0)
            return data;
        // Clear miss.
        if (data->type != needle.type)
            continue;
        // We have numbers, but they are different.
        if (!data->bpNumber.isEmpty() && !needle.bpNumber.isEmpty()
                && !data->bpNumber.startsWith(needle.bpNumber)
                && !needle.bpNumber.startsWith(data->bpNumber))
            continue;
        // At least at a position we were looking for.
        // FIXME: breaks multiple breakpoints at the same location
        if (!data->fileName.isEmpty()
                && fileNameMatch(data->fileName, needle.bpFileName)
                && data->lineNumber == needle.bpLineNumber)
            return data;
    }
    return 0;
}

BreakpointData *BreakHandler::findBreakpointByNumber(int bpNumber) const
{
    if (!size())
        return 0;
    QString numStr = QString::number(bpNumber);
    for (int index = 0; index != size(); ++index)
        if (at(index)->bpNumber == numStr)
            return at(index);
    return 0;
}

BreakpointData *BreakHandler::findWatchPointByAddress(const QByteArray &a) const
{
    for (int index = size() - 1; index >= 0; --index) {
        BreakpointData *bd = at(index);
        if (bd->type == BreakpointData::WatchpointType && bd->address == a)
            return bd;
    }
    return 0;
}

bool BreakHandler::watchPointAt(quint64 address) const
{
    const QByteArray addressBA = QByteArray("0x") + QByteArray::number(address, 16);
    return findWatchPointByAddress(addressBA);
}

void BreakHandler::saveBreakpoints()
{
    QList<QVariant> list;
    for (int index = 0; index != size(); ++index) {
        const BreakpointData *data = at(index);
        QMap<QString, QVariant> map;
        // Do not persist Watchpoints.
        //if (data->type == BreakpointData::WatchpointType)
        //    continue;
        if (data->type != BreakpointData::BreakpointType)
            map.insert(_("type"), data->type);
        if (!data->fileName.isEmpty())
            map.insert(_("filename"), data->fileName);
        if (!data->lineNumber.isEmpty())
            map.insert(_("linenumber"), data->lineNumber);
        if (!data->funcName.isEmpty())
            map.insert(_("funcname"), data->funcName);
        if (!data->address.isEmpty())
            map.insert(_("address"), data->address);
        if (!data->condition.isEmpty())
            map.insert(_("condition"), data->condition);
        if (!data->ignoreCount.isEmpty())
            map.insert(_("ignorecount"), data->ignoreCount);
        if (!data->threadSpec.isEmpty())
            map.insert(_("threadspec"), data->threadSpec);
        if (!data->enabled)
            map.insert(_("disabled"), _("1"));
        if (data->useFullPath)
            map.insert(_("usefullpath"), _("1"));
        list.append(map);
    }
    m_manager->setSessionValue("Breakpoints", list);
}

void BreakHandler::loadBreakpoints()
{
    QVariant value = m_manager->sessionValue("Breakpoints");
    QList<QVariant> list = value.toList();
    clear();
    foreach (const QVariant &var, list) {
        const QMap<QString, QVariant> map = var.toMap();
        BreakpointData *data = new BreakpointData;
        QVariant v = map.value(_("filename"));
        if (v.isValid())
            data->fileName = v.toString();
        v = map.value(_("linenumber"));
        if (v.isValid())
            data->lineNumber = v.toString().toLatin1();
        v = map.value(_("condition"));
        if (v.isValid())
            data->condition = v.toString().toLatin1();
        v = map.value(_("address"));
        if (v.isValid())
            data->address = v.toString().toLatin1();
        v = map.value(_("ignorecount"));
        if (v.isValid())
            data->ignoreCount = v.toString().toLatin1();
        v = map.value(_("threadspec"));
        if (v.isValid())
            data->threadSpec = v.toString().toLatin1();
        v = map.value(_("funcname"));
        if (v.isValid())
            data->funcName = v.toString();
        v = map.value(_("disabled"));
        if (v.isValid())
            data->enabled = !v.toInt();
        v = map.value(_("usefullpath"));
        if (v.isValid())
            data->useFullPath = bool(v.toInt());
        v = map.value(_("type"));
        if (v.isValid())
            data->type = BreakpointData::Type(v.toInt());
        data->setMarkerFileName(data->fileName);
        data->setMarkerLineNumber(data->lineNumber.toInt());
        append(data);
    }
}

void BreakHandler::resetBreakpoints()
{
    for (int index = size(); --index >= 0;) {
        BreakpointData *data = at(index);
        data->pending = true;
        data->bpMultiple = false;
        data->bpEnabled = true;
        data->bpNumber.clear();
        data->bpFuncName.clear();
        data->bpFileName.clear();
        data->bpLineNumber.clear();
        data->bpCorrectedLineNumber.clear();
        data->bpCondition.clear();
        data->bpIgnoreCount.clear();
        data->bpThreadSpec.clear();
        data->bpAddress.clear();
        // Keep marker data if it was primary.
        if (data->markerFileName() != data->fileName)
            data->setMarkerFileName(QString());
        if (data->markerLineNumber() != data->lineNumber.toInt())
            data->setMarkerLineNumber(0);
    }
    m_enabled.clear();
    m_disabled.clear();
    m_removed.clear();
    m_inserted.clear();
}

void BreakHandler::updateMarkers()
{
    for (int index = 0; index != size(); ++index)
        at(index)->updateMarker();
    emit layoutChanged();
}

QVariant BreakHandler::headerData(int section,
    Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        static QString headers[] = {
            tr("Number"),  tr("Function"), tr("File"), tr("Line"),
            tr("Condition"), tr("Ignore"), tr("Threads"), tr("Address")
        };
        return headers[section];
    }
    return QVariant();
}

QVariant BreakHandler::data(const QModelIndex &mi, int role) const
{
    static const QString empty = QString(QLatin1Char('-'));

    QTC_ASSERT(mi.isValid(), return QVariant());

    if (mi.row() >= size())
        return QVariant();

    const BreakpointData *data = at(mi.row());
    switch (mi.column()) {
        case 0:
            if (role == Qt::DisplayRole) {
                const QString str = data->bpNumber;
                return str.isEmpty() ? empty : str;
            }
            if (role == Qt::UserRole)
                return data->enabled;
            if (role == Qt::DecorationRole) {
                if (data->type == BreakpointData::WatchpointType)
                    return m_watchpointIcon;
                if (!data->enabled)
                    return m_disabledBreakpointIcon;
                return data->pending ? m_pendingBreakPointIcon : m_breakpointIcon;
            }
            break;
        case 1:
            if (role == Qt::DisplayRole) {
                const QString str = data->pending ? data->funcName : data->bpFuncName;
                return str.isEmpty() ? empty : str;
            }
            if (role == Qt::UserRole + 1)
                return data->funcName;
            break;
        case 2:
            if (role == Qt::DisplayRole) {
                QString str = data->pending ? data->fileName : data->bpFileName;
                str = QFileInfo(str).fileName();
                // FIXME: better?
                //if (data->bpMultiple && str.isEmpty() && !data->markerFileName.isEmpty())
                //    str = data->markerFileName;
                str = str.isEmpty() ? empty : str;
                if (data->useFullPath)
                    str = "/.../" + str;
                return str;
            }
            if (role == Qt::UserRole)
                return data->useFullPath;
            if (role == Qt::UserRole + 1)
                return data->fileName;
            break;
        case 3:
            if (role == Qt::DisplayRole) {
                // FIXME: better?
                //if (data->bpMultiple && str.isEmpty() && !data->markerFileName.isEmpty())
                //    str = data->markerLineNumber;
                const QString str = data->pending ? data->lineNumber : data->bpLineNumber;
                return str.isEmpty() ? empty : str;
            }
            if (role == Qt::UserRole + 1)
                return data->lineNumber;
            break;
        case 4:
            if (role == Qt::DisplayRole)
                return data->pending ? data->condition : data->bpCondition;
            if (role == Qt::ToolTipRole)
                return tr("Breakpoint will only be hit if this condition is met.");
            if (role == Qt::UserRole + 1)
                return data->condition;
            break;
        case 5:
            if (role == Qt::DisplayRole)
                return data->pending ? data->ignoreCount : data->bpIgnoreCount;
            if (role == Qt::ToolTipRole)
                return tr("Breakpoint will only be hit after being ignored so many times.");
            if (role == Qt::UserRole + 1)
                return data->ignoreCount;
        case 6:
            if (role == Qt::DisplayRole) {
                if (data->pending)
                    return !data->threadSpec.isEmpty() ? data->threadSpec : tr("(all)");
                else
                    return !data->bpThreadSpec.isEmpty() ? data->bpThreadSpec : tr("(all)");
            }
            if (role == Qt::ToolTipRole)
                return tr("Breakpoint will only be hit in the specified thread(s).");
            if (role == Qt::UserRole + 1)
                return data->threadSpec;
        case 7:
            if (role == Qt::DisplayRole) {
                if (data->type == BreakpointData::WatchpointType)
                    return data->address;
                return data->bpAddress;
            } 
            break;
    }
    if (role == Qt::ToolTipRole)
        return theDebuggerBoolSetting(UseToolTipsInBreakpointsView)
                ? data->toToolTip() : QVariant();
    return QVariant();
}

Qt::ItemFlags BreakHandler::flags(const QModelIndex &mi) const
{
    switch (mi.column()) {
        //case 0:
        //    return Qt::ItemIsUserCheckable | Qt::ItemIsEnabled;
        default:
            return QAbstractTableModel::flags(mi);
    }
}

bool BreakHandler::setData(const QModelIndex &mi, const QVariant &value, int role)
{
    BreakpointData *data = at(mi.row());

    if (role == Qt::UserRole + 1) {
        if (data->enabled != value.toBool()) {
            toggleBreakpointEnabled(data);
            layoutChanged();
        }
        return true;
    }

    if (role == Qt::UserRole + 2) {
        if (data->useFullPath != value.toBool()) {
            data->useFullPath = value.toBool();
            layoutChanged();
        }
        return true;
    }

    //if (role != Qt::EditRole)
    //    return false;

    switch (mi.column()) {
        case 1: {
            QString val = value.toString();
            if (data->funcName != val) {
                data->funcName = val;
                layoutChanged();
            }
            return true;
        }
        case 2: {
            QString val = value.toString();
            if (data->fileName != val) {
                data->fileName = val;
                layoutChanged();
            }
            return true;
        }
        case 3: {
            QByteArray val = value.toString().toLatin1();
            if (data->lineNumber != val) {
                data->lineNumber = val;
                layoutChanged();
            }
            return true;
        }
        case 4: {
            QByteArray val = value.toString().toLatin1();
            if (val != data->condition) {
                data->condition = val;
                layoutChanged();
            }
            return true;
        }
        case 5: {
            QByteArray val = value.toString().toLatin1();
            if (val != data->ignoreCount) {
                data->ignoreCount = val;
                layoutChanged();
            }
            return true;
        }
        case 6: {
            QByteArray val = value.toString().toLatin1();
            if (val != data->threadSpec) {
                data->threadSpec = val;
                layoutChanged();
            }
            return true;
        }
        default: {
            return false;
        }
    }
}

void BreakHandler::append(BreakpointData *data)
{
    data->m_handler = this;
    m_bp.append(data);
    m_inserted.append(data);
}

QList<BreakpointData *> BreakHandler::insertedBreakpoints() const
{
    return m_inserted;
}

void BreakHandler::takeInsertedBreakPoint(BreakpointData *d)
{
    m_inserted.removeAll(d);
}

QList<BreakpointData *> BreakHandler::takeRemovedBreakpoints()
{
    QList<BreakpointData *> result = m_removed;
    m_removed.clear();
    return result;
}

QList<BreakpointData *> BreakHandler::takeEnabledBreakpoints()
{
    QList<BreakpointData *> result = m_enabled;
    m_enabled.clear();
    return result;
}

QList<BreakpointData *> BreakHandler::takeDisabledBreakpoints()
{
    QList<BreakpointData *> result = m_disabled;
    m_disabled.clear();
    return result;
}

void BreakHandler::removeBreakpointHelper(int index)
{
    BreakpointData *data = m_bp.at(index);
    m_bp.removeAt(index);
    data->removeMarker();
    m_removed.append(data);
}

void BreakHandler::removeBreakpoint(int index)
{
    if (index < 0 || index >= size())
        return;
    removeBreakpointHelper(index);
    emit layoutChanged();
    saveBreakpoints();
}

void BreakHandler::removeBreakpoint(BreakpointData *data)
{
    removeBreakpointHelper(m_bp.indexOf(data));
    emit layoutChanged();
    saveBreakpoints();
}

void BreakHandler::toggleBreakpointEnabled(BreakpointData *data)
{
    QTC_ASSERT(data, return);
    data->enabled = !data->enabled;
    if (data->enabled) {
        m_enabled.append(data);
        m_disabled.removeAll(data);
    } else {
        m_enabled.removeAll(data);
        m_disabled.append(data);
    }
    saveBreakpoints();
    updateMarkers();
}

void BreakHandler::appendBreakpoint(BreakpointData *data)
{
    append(data);
    emit layoutChanged();
    saveBreakpoints();
    updateMarkers();
}

void BreakHandler::removeAllBreakpoints()
{
    for (int index = size(); --index >= 0;)
        removeBreakpointHelper(index);
    emit layoutChanged();
    saveBreakpoints();
    updateMarkers();
}

void BreakHandler::setAllPending()
{
    loadBreakpoints();
    for (int index = size(); --index >= 0;)
        at(index)->pending = true;
    saveBreakpoints();
    updateMarkers();
}

void BreakHandler::saveSessionData()
{
    saveBreakpoints();
    updateMarkers();
}

void BreakHandler::loadSessionData()
{
    //resetBreakpoints();
    loadBreakpoints();
    updateMarkers();
}

void BreakHandler::breakByFunction(const QString &functionName)
{
    // One breakpoint per function is enough for now. This does not handle
    // combinations of multiple conditions and ignore counts, though.
    for (int index = size(); --index >= 0;) {
        const BreakpointData *data = at(index);
        QTC_ASSERT(data, break);
        if (data->funcName == functionName && data->condition.isEmpty()
                && data->ignoreCount.isEmpty())
            return;
    }
    BreakpointData *data = new BreakpointData;
    data->funcName = functionName;
    append(data);
    saveBreakpoints();
    updateMarkers();
}

#include "breakhandler.moc"
