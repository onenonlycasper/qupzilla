/* ============================================================
* QupZilla - WebKit based browser
* Copyright (C) 2010-2014  David Rosca <nowrep@gmail.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
* ============================================================ */
#include "locationcompletermodel.h"
#include "mainapplication.h"
#include "iconprovider.h"
#include "bookmarkitem.h"
#include "bookmarks.h"
#include "qzsettings.h"
#include "browserwindow.h"
#include "tabwidget.h"

#include <QSqlQuery>

LocationCompleterModel::LocationCompleterModel(QObject* parent)
    : QStandardItemModel(parent)
{
}

void LocationCompleterModel::setCompletions(const QList<QStandardItem*> &items)
{
    foreach (QStandardItem* item, items) {
        item->setIcon(QPixmap::fromImage(item->data(ImageRole).value<QImage>()));
        setTabPosition(item);

        if (item->icon().isNull()) {
            item->setIcon(IconProvider::emptyWebIcon());
        }
    }

    clear();
    appendColumn(items);
}

void LocationCompleterModel::showMostVisited()
{
    clear();

    QSqlQuery query;
    query.exec("SELECT id, url, title FROM history ORDER BY count DESC LIMIT 15");

    while (query.next()) {
        QStandardItem* item = new QStandardItem();
        const QUrl url = query.value(1).toUrl();

        item->setIcon(IconProvider::iconForUrl(url));
        item->setText(url.toEncoded());
        item->setData(query.value(0), IdRole);
        item->setData(query.value(2), TitleRole);
        item->setData(url, UrlRole);
        item->setData(QVariant(false), BookmarkRole);
        setTabPosition(item);

        appendRow(item);
    }
}

QSqlQuery LocationCompleterModel::createDomainQuery(const QString &text)
{
    if (text.isEmpty() || text == QLatin1String("www.")) {
        return QSqlQuery();
    }

    bool withoutWww = text.startsWith(QLatin1Char('w')) && !text.startsWith(QLatin1String("www."));
    QString query = "SELECT url FROM history WHERE ";

    if (withoutWww) {
        query.append(QLatin1String("url NOT LIKE ? AND url NOT LIKE ? AND "));
    }
    else {
        query.append(QLatin1String("url LIKE ? OR url LIKE ? OR "));
    }

    query.append(QLatin1String("(url LIKE ? OR url LIKE ?) ORDER BY count DESC LIMIT 1"));

    QSqlQuery sqlQuery;
    sqlQuery.prepare(query);

    if (withoutWww) {
        sqlQuery.addBindValue(QString("http://www.%"));
        sqlQuery.addBindValue(QString("https://www.%"));
        sqlQuery.addBindValue(QString("http://%1%").arg(text));
        sqlQuery.addBindValue(QString("https://%1%").arg(text));
    }
    else {
        sqlQuery.addBindValue(QString("http://%1%").arg(text));
        sqlQuery.addBindValue(QString("https://%1%").arg(text));
        sqlQuery.addBindValue(QString("http://www.%1%").arg(text));
        sqlQuery.addBindValue(QString("https://www.%1%").arg(text));
    }

    return sqlQuery;
}

QSqlQuery LocationCompleterModel::createHistoryQuery(const QString &searchString, int limit, bool exactMatch)
{
    QStringList searchList;
    QString query = QLatin1String("SELECT id, url, title, count FROM history WHERE ");

    if (exactMatch) {
        query.append(QLatin1String("title LIKE ? OR url LIKE ? "));
    }
    else {
        searchList = searchString.split(QLatin1Char(' '), QString::SkipEmptyParts);
        const int slSize = searchList.size();
        for (int i = 0; i < slSize; ++i) {
            query.append(QLatin1String("(title LIKE ? OR url LIKE ?) "));
            if (i < slSize - 1) {
                query.append(QLatin1String("AND "));
            }
        }
    }

    query.append(QLatin1String("LIMIT ?"));

    QSqlQuery sqlQuery;
    sqlQuery.prepare(query);

    if (exactMatch) {
        sqlQuery.addBindValue(QString("%%1%").arg(searchString));
        sqlQuery.addBindValue(QString("%%1%").arg(searchString));
    }
    else {
        foreach (const QString &str, searchList) {
            sqlQuery.addBindValue(QString("%%1%").arg(str));
            sqlQuery.addBindValue(QString("%%1%").arg(str));
        }
    }

    sqlQuery.addBindValue(limit);

    return sqlQuery;
}

void LocationCompleterModel::setTabPosition(QStandardItem* item) const
{
    Q_ASSERT(item);

    if (!qzSettings->showSwitchTab) {
        return;
    }

    const QUrl url = item->data(UrlRole).toUrl();
    const QList<BrowserWindow*> windows = mApp->windows();

    foreach (BrowserWindow* window, windows) {
        QList<WebTab*> tabs = window->tabWidget()->allTabs();
        for (int i = 0; i < tabs.count(); ++i) {
            WebTab* tab = tabs.at(i);
            if (tab->url() == url) {
                item->setData(QVariant::fromValue<void*>(static_cast<void*>(window)), TabPositionWindowRole);
                item->setData(i, TabPositionTabRole);
                return;
            }
        }
    }

    // Tab wasn't found
    item->setData(QVariant::fromValue<void*>(static_cast<void*>(0)), TabPositionWindowRole);
    item->setData(-1, TabPositionTabRole);
}

void LocationCompleterModel::refreshTabPositions() const
{
    for (int row = 0; row < rowCount(); ++row) {
        QStandardItem* itm = item(row);
        if (itm) {
            setTabPosition(itm);
        }
    }
}
