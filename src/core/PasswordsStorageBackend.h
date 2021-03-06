/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2016 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*
**************************************************************************/

#ifndef OTTER_PASSWORDSSTORAGEBACKEND_H
#define OTTER_PASSWORDSSTORAGEBACKEND_H

#include "AddonsManager.h"
#include "PasswordsManager.h"

namespace Otter
{

class PasswordsStorageBackend : public Addon
{
	Q_OBJECT

public:
	explicit PasswordsStorageBackend(QObject *parent = NULL);

	virtual void clearPasswords(const QString &host = QString());
	virtual void addPassword(const PasswordsManager::PasswordInformation &password);
	virtual void removePassword(const PasswordsManager::PasswordInformation &password);
	QUrl getUpdateUrl() const;
	virtual QStringList getHosts();
	virtual QList<PasswordsManager::PasswordInformation> getPasswords(const QUrl &url, PasswordsManager::PasswordTypes types = PasswordsManager::AnyPassword);
	AddonType getType() const;
	virtual PasswordsManager::PasswordMatch hasPassword(const PasswordsManager::PasswordInformation &password);
	virtual bool hasPasswords(const QUrl &url, PasswordsManager::PasswordTypes types = PasswordsManager::AnyPassword);

signals:
	void passwordsModified();
};

}

#endif
