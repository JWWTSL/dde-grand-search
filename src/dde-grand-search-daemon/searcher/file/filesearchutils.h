// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FILESEARCHUTILS_H
#define FILESEARCHUTILS_H

#include "global/matcheditem.h"

namespace GrandSearch {

class FileSearchUtils
{
public:
    enum Group {
        Unknown = 0,
        File,
        Folder,
        Picture,
        Audio,
        Video,
        Document,
        GroupCount,
        GroupBegin = Unknown
    };

    // 通过搜索内容获取到的搜索信息
    struct SearchInfo
    {
        bool isCombinationSearch = false;   // 是否是组合搜索
        QString keyword;                    // 搜索关键字
        QStringList boolKeywords;           // 空格分词
        QStringList typeKeywords;           // 类目搜索分词
        QStringList suffixList;             // 搜索后缀表
        QList<Group> groupList;             // 搜索类目表
    };

    static MatchedItem packItem(const QString &fileName, const QString &searcher);
    static QString groupKey(Group group);
    static Group getGroupByName(const QString &fileName);
    static Group getGroupBySuffix(const QString &suffix);
    static Group getGroupByGroupName(const QString &groupName);
    static SearchInfo parseContent(const QString &content);
    static bool fileShouldVisible(const QString &fileName, Group &group, const SearchInfo &info);
    static bool filterByBlacklist(const QString &fileName);
    static QVariantHash tailerData(const QFileInfo &info);
    static QStringList buildDFMSearchFileTypes(const QList<Group> &groupList);
    static bool isPinyin(const QString &str);
    static bool hasWildcard(const QString &str);
};

}

#endif   // FILESEARCHUTILS_H
