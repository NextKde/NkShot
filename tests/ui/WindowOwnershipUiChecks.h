// SPDX-License-Identifier: GPL-3.0-only
#pragma once
class QQuickView;
class QString;
class WindowOwnershipUiChecks
{
public:
    static bool run(QQuickView *owner, QString *failure);
};
