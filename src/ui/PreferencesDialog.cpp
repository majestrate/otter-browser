/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2013 - 2015 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
* Copyright (C) 2014 Jan Bajer aka bajasoft <jbajer@gmail.com>
* Copyright (C) 2014 Piotr Wójcik <chocimier@tlen.pl>
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
*
**************************************************************************/

#include "PreferencesDialog.h"
#include "ClearHistoryDialog.h"
#include "MainWindow.h"
#include "Menu.h"
#include "OptionDelegate.h"
#include "OptionWidget.h"
#include "SearchPropertiesDialog.h"
#include "UserAgentsManagerDialog.h"
#include "preferences/AcceptLanguageDialog.h"
#include "preferences/JavaScriptPreferencesDialog.h"
#include "preferences/SearchKeywordDelegate.h"
#include "preferences/ShortcutsProfileDialog.h"
#include "../core/Application.h"
#include "../core/BookmarksManager.h"
#include "../core/NetworkManagerFactory.h"
#include "../core/NotificationsManager.h"
#include "../core/PlatformIntegration.h"
#include "../core/SettingsManager.h"
#include "../core/SearchesManager.h"
#include "../core/SessionsManager.h"
#include "../core/Utils.h"
#include "../core/WindowsManager.h"

#include "ui_PreferencesDialog.h"

#include <QtCore/QSettings>
#include <QtCore/QTimer>
#include <QtMultimedia/QSoundEffect>
#include <QtNetwork/QSslSocket>
#include <QtNetwork/QSslCipher>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>

namespace Otter
{

PreferencesDialog::PreferencesDialog(const QLatin1String &section, QWidget *parent) : QDialog(parent),
	m_defaultSearchEngine(SettingsManager::getValue(QLatin1String("Search/DefaultSearchEngine")).toString()),
	m_clearHisorySettings(SettingsManager::getValue(QLatin1String("History/ClearOnClose")).toStringList()),
	m_userAgentsModified(false),
	m_ui(new Ui::PreferencesDialog)
{
	m_ui->setupUi(this);

	m_loadedTabs.fill(false, 5);

	m_clearHisorySettings.removeAll(QString());

	int tab = 0;

	if (section == QLatin1String("content"))
	{
		tab = 1;
	}
	else if (section == QLatin1String("privacy"))
	{
		tab = 2;
	}
	else if (section == QLatin1String("search"))
	{
		tab = 3;
	}
	else if (section == QLatin1String("advanced"))
	{
		tab = 4;
	}

	currentTabChanged(tab);

	m_ui->tabWidget->setCurrentIndex(tab);
	m_ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);

	connect(m_ui->tabWidget, SIGNAL(currentChanged(int)), this, SLOT(currentTabChanged(int)));
	connect(m_ui->advancedListWidget, SIGNAL(currentRowChanged(int)), m_ui->advancedStackedWidget, SLOT(setCurrentIndex(int)));
	connect(m_ui->buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked()), this, SLOT(save()));
	connect(m_ui->buttonBox, SIGNAL(accepted()), this, SLOT(save()));
	connect(m_ui->buttonBox, SIGNAL(rejected()), this, SLOT(close()));
	connect(m_ui->allSettingsButton, SIGNAL(clicked()), this, SLOT(openConfigurationManager()));
}

PreferencesDialog::~PreferencesDialog()
{
	delete m_ui;
}

void PreferencesDialog::changeEvent(QEvent *event)
{
	QDialog::changeEvent(event);

	switch (event->type())
	{
		case QEvent::LanguageChange:
			m_ui->retranslateUi(this);

			break;
		default:
			break;
	}
}

void PreferencesDialog::currentTabChanged(int tab)
{
	if (m_loadedTabs[tab])
	{
		return;
	}

	m_loadedTabs[tab] = true;

	switch (tab)
	{
		case 0:
			{
				m_ui->startupBehaviorComboBox->addItem(tr("Continue previous session"), QLatin1String("continuePrevious"));
				m_ui->startupBehaviorComboBox->addItem(tr("Show startup dialog"), QLatin1String("showDialog"));
				m_ui->startupBehaviorComboBox->addItem(tr("Show home page"), QLatin1String("startHomePage"));
				m_ui->startupBehaviorComboBox->addItem(tr("Show empty page"), QLatin1String("startEmpty"));

				const int startupBehaviorIndex = m_ui->startupBehaviorComboBox->findData(SettingsManager::getValue(QLatin1String("Browser/StartupBehavior")).toString());

				m_ui->startupBehaviorComboBox->setCurrentIndex((startupBehaviorIndex < 0) ? 0 : startupBehaviorIndex);
				m_ui->homePageLineEdit->setText(SettingsManager::getValue(QLatin1String("Browser/HomePage")).toString());

				Menu *bookmarksMenu = new Menu(Menu::BookmarkSelectorMenuRole, m_ui->useBookmarkAsHomePageButton);

				m_ui->useBookmarkAsHomePageButton->setMenu(bookmarksMenu);
				m_ui->useBookmarkAsHomePageButton->setEnabled(BookmarksManager::getModel()->getRootItem()->rowCount() > 0);
				m_ui->downloadsFilePathWidget->setSelectFile(false);
				m_ui->downloadsFilePathWidget->setPath(SettingsManager::getValue(QLatin1String("Paths/Downloads")).toString());
				m_ui->alwaysAskCheckBox->setChecked(SettingsManager::getValue(QLatin1String("Browser/AlwaysAskWhereToSaveDownload")).toBool());
				m_ui->tabsInsteadOfWindowsCheckBox->setChecked(SettingsManager::getValue(QLatin1String("Browser/OpenLinksInNewTab")).toBool());
				m_ui->delayTabsLoadingCheckBox->setChecked(SettingsManager::getValue(QLatin1String("Browser/DelayRestoringOfBackgroundTabs")).toBool());
				m_ui->reuseCurrentTabCheckBox->setChecked(SettingsManager::getValue(QLatin1String("Browser/ReuseCurrentTab")).toBool());
				m_ui->openNextToActiveheckBox->setChecked(SettingsManager::getValue(QLatin1String("TabBar/OpenNextToActive")).toBool());

				PlatformIntegration *integration = Application::getInstance()->getPlatformIntegration();

				if (integration == NULL || integration->isDefaultBrowser())
				{
					m_ui->setDefaultButton->setEnabled(false);
				}
				else if (!integration->canSetAsDefaultBrowser())
				{
					m_ui->setDefaultButton->setVisible(false);
					m_ui->systemDefaultLabel->setText(tr("Run Otter Browser with administrator rights to set it as a default browser."));
				}
				else
				{
					connect(m_ui->setDefaultButton, SIGNAL(clicked()), integration, SLOT(setAsDefaultBrowser()));
				}

				connect(bookmarksMenu, SIGNAL(triggered(QAction*)), this, SLOT(useBookmarkAsHomePage(QAction*)));
				connect(m_ui->useCurrentAsHomePageButton, SIGNAL(clicked()), this, SLOT(useCurrentAsHomePage()));
				connect(m_ui->restoreHomePageButton, SIGNAL(clicked()), this, SLOT(restoreHomePage()));
				connect(m_ui->acceptLanguageButton, SIGNAL(clicked()), this, SLOT(manageAcceptLanguage()));
			}

			break;
		case 1:
			{
				m_ui->defaultZoomSpinBox->setValue(SettingsManager::getValue(QLatin1String("Content/DefaultZoom")).toInt());
				m_ui->zoomTextOnlyCheckBox->setChecked(SettingsManager::getValue(QLatin1String("Content/ZoomTextOnly")).toBool());
				m_ui->proportionalFontSizeSpinBox->setValue(SettingsManager::getValue(QLatin1String("Content/DefaultFontSize")).toInt());
				m_ui->fixedFontSizeSpinBox->setValue(SettingsManager::getValue(QLatin1String("Content/DefaultFixedFontSize")).toInt());
				m_ui->minimumFontSizeSpinBox->setValue(SettingsManager::getValue(QLatin1String("Content/MinimumFontSize")).toInt());

				QList<QLatin1String> fonts;
				fonts << QLatin1String("StandardFont") << QLatin1String("FixedFont") << QLatin1String("SerifFont") << QLatin1String("SansSerifFont") << QLatin1String("CursiveFont") << QLatin1String("FantasyFont");

				QStringList fontCategories;
				fontCategories << tr("Standard font") << tr("Fixed-width font") << tr("Serif font") << tr("Sans-serif font") << tr("Cursive font") << tr("Fantasy font");

				OptionDelegate *fontsDelegate = new OptionDelegate(true, this);

				m_ui->fontsWidget->setRowCount(fonts.count());
				m_ui->fontsWidget->setItemDelegateForColumn(1, fontsDelegate);

				for (int i = 0; i < fonts.count(); ++i)
				{
					const QString family = SettingsManager::getValue(QLatin1String("Content/") + fonts.at(i)).toString();
					QTableWidgetItem *familyItem = new QTableWidgetItem(family);
					familyItem->setData(Qt::UserRole, QLatin1String("Content/") + fonts.at(i));
					familyItem->setData((Qt::UserRole + 1), QLatin1String("font"));

					QTableWidgetItem *previewItem = new QTableWidgetItem(tr("The quick brown fox jumps over the lazy dog"));
					previewItem->setFont(QFont(family));

					m_ui->fontsWidget->setItem(i, 0, new QTableWidgetItem(fontCategories.at(i)));
					m_ui->fontsWidget->setItem(i, 1, familyItem);
					m_ui->fontsWidget->setItem(i, 2, previewItem);
				}

				QList<QLatin1String> colors;
				colors << QLatin1String("BackgroundColor") << QLatin1String("TextColor") << QLatin1String("LinkColor") << QLatin1String("VisitedLinkColor");

				QStringList colorTypes;
				colorTypes << tr("Background Color") << tr("Text Color") << tr("Link Color") << tr("Visited Link Color");

				OptionDelegate *colorsDelegate = new OptionDelegate(true, this);

				m_ui->colorsWidget->setRowCount(colors.count());
				m_ui->colorsWidget->setItemDelegateForColumn(1, colorsDelegate);

				for (int i = 0; i < colors.count(); ++i)
				{
					const QString color = SettingsManager::getValue(QLatin1String("Content/") + colors.at(i)).toString();
					QTableWidgetItem *previewItem = new QTableWidgetItem(color);
					previewItem->setBackgroundColor(QColor(color));
					previewItem->setTextColor(Qt::transparent);
					previewItem->setData(Qt::UserRole, QLatin1String("Content/") + colors.at(i));
					previewItem->setData((Qt::UserRole + 1), QLatin1String("color"));

					m_ui->colorsWidget->setItem(i, 0, new QTableWidgetItem(colorTypes.at(i)));
					m_ui->colorsWidget->setItem(i, 1, previewItem);
				}

				connect(m_ui->fontsWidget, SIGNAL(currentCellChanged(int,int,int,int)), this, SLOT(currentFontChanged(int,int,int,int)));
				connect(fontsDelegate, SIGNAL(commitData(QWidget*)), this, SLOT(fontChanged(QWidget*)));
				connect(m_ui->colorsWidget, SIGNAL(currentCellChanged(int,int,int,int)), this, SLOT(currentColorChanged(int,int,int,int)));
				connect(colorsDelegate, SIGNAL(commitData(QWidget*)), this, SLOT(colorChanged(QWidget*)));
			}

			break;
		case 2:
			{
				m_ui->doNotTrackComboBox->addItem(tr("Inform websites that I do not want to be tracked"), QLatin1String("doNotAllow"));
				m_ui->doNotTrackComboBox->addItem(tr("Inform websites that I allow tracking"), QLatin1String("allow"));
				m_ui->doNotTrackComboBox->addItem(tr("Do not inform websites about my preference"), QLatin1String("skip"));

				const int doNotTrackPolicyIndex = m_ui->doNotTrackComboBox->findData(SettingsManager::getValue(QLatin1String("Network/DoNotTrackPolicy")).toString());

				m_ui->doNotTrackComboBox->setCurrentIndex((doNotTrackPolicyIndex < 0) ? 2 : doNotTrackPolicyIndex);
				m_ui->privateModeCheckBox->setChecked(SettingsManager::getValue(QLatin1String("Browser/PrivateMode")).toBool());
				m_ui->historyWidget->setDisabled(m_ui->privateModeCheckBox->isChecked());
				m_ui->rememberBrowsingHistoryCheckBox->setChecked(SettingsManager::getValue(QLatin1String("History/RememberBrowsing")).toBool());
				m_ui->rememberDownloadsHistoryCheckBox->setChecked(SettingsManager::getValue(QLatin1String("History/RememberDownloads")).toBool());
				m_ui->enableCookiesCheckBox->setChecked(SettingsManager::getValue(QLatin1String("Network/CookiesPolicy")).toString() != QLatin1String("ignore"));
				m_ui->cookiesWidget->setEnabled(m_ui->enableCookiesCheckBox->isChecked());
				m_ui->cookiesPolicyComboBox->addItem(tr("Always"), QLatin1String("acceptAll"));
				m_ui->cookiesPolicyComboBox->addItem(tr("Only existing"), QLatin1String("acceptExisting"));
				m_ui->cookiesPolicyComboBox->addItem(tr("Only read existing"), QLatin1String("readOnly"));
				m_ui->cookiesPolicyComboBox->addItem(tr("Never"), QLatin1String("ignore"));

				const int cookiesPolicyIndex = m_ui->cookiesPolicyComboBox->findData(SettingsManager::getValue(QLatin1String("Network/CookiesPolicy")).toString());

				m_ui->cookiesPolicyComboBox->setCurrentIndex((cookiesPolicyIndex < 0) ? 0 : cookiesPolicyIndex);
				m_ui->keepCookiesModeComboBox->addItem(tr("Expires"), QLatin1String("keepUntilExpires"));
				m_ui->keepCookiesModeComboBox->addItem(tr("Current session is closed"), QLatin1String("keepUntilExit"));
				m_ui->keepCookiesModeComboBox->addItem(tr("Always ask"), QLatin1String("ask"));

				const int cookiesKeepModeIndex = m_ui->keepCookiesModeComboBox->findData(SettingsManager::getValue(QLatin1String("Network/CookiesKeepMode")).toString());

				m_ui->keepCookiesModeComboBox->setCurrentIndex((cookiesKeepModeIndex < 0) ? 0 : cookiesKeepModeIndex);
				m_ui->thirdPartyCookiesPolicyComboBox->addItem(tr("Always"), QLatin1String("acceptAll"));
				m_ui->thirdPartyCookiesPolicyComboBox->addItem(tr("Only existing"), QLatin1String("acceptExisting"));
				m_ui->thirdPartyCookiesPolicyComboBox->addItem(tr("Never"), QLatin1String("ignore"));

				const int thirdPartyCookiesIndex = m_ui->thirdPartyCookiesPolicyComboBox->findData(SettingsManager::getValue(QLatin1String("Network/ThirdPartyCookiesPolicy")).toString());

				m_ui->thirdPartyCookiesPolicyComboBox->setCurrentIndex((thirdPartyCookiesIndex < 0) ? 0 : thirdPartyCookiesIndex);
				m_ui->clearHistoryCheckBox->setChecked(!m_clearHisorySettings.isEmpty());
				m_ui->clearHistoryButton->setEnabled(!m_clearHisorySettings.isEmpty());

				connect(m_ui->privateModeCheckBox, SIGNAL(toggled(bool)), m_ui->historyWidget, SLOT(setDisabled(bool)));
				connect(m_ui->enableCookiesCheckBox, SIGNAL(toggled(bool)), m_ui->cookiesWidget, SLOT(setEnabled(bool)));
				connect(m_ui->clearHistoryCheckBox, SIGNAL(toggled(bool)), m_ui->clearHistoryButton, SLOT(setEnabled(bool)));
				connect(m_ui->clearHistoryButton, SIGNAL(clicked()), this, SLOT(setupClearHistory()));
			}

			break;
		case 3:
			{
				QStringList searchEnginesLabels;
				searchEnginesLabels << tr("Name") << tr("Keyword");

				QStandardItemModel *searchEnginesModel = new QStandardItemModel(this);
				searchEnginesModel->setHorizontalHeaderLabels(searchEnginesLabels);

				const QStringList searchEngines = SearchesManager::getSearchEngines();

				for (int i = 0; i < searchEngines.count(); ++i)
				{
					const SearchInformation engine = SearchesManager::getSearchEngine(searchEngines.at(i));

					if (engine.identifier.isEmpty())
					{
						continue;
					}

					m_searchEngines[engine.identifier] = qMakePair(false, engine);

					QList<QStandardItem*> items;
					items.append(new QStandardItem(engine.icon, engine.title));
					items[0]->setToolTip(engine.description);
					items[0]->setData(engine.identifier, Qt::UserRole);
					items[0]->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsDragEnabled);
					items.append(new QStandardItem(engine.keyword));
					items[1]->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsDragEnabled);

					searchEnginesModel->appendRow(items);
				}

				m_ui->searchViewWidget->setModel(searchEnginesModel);
				m_ui->searchViewWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
				m_ui->searchViewWidget->setItemDelegateForColumn(0, new OptionDelegate(true, this));
				m_ui->searchViewWidget->setItemDelegateForColumn(1, new SearchKeywordDelegate(this));
				m_ui->searchSuggestionsCheckBox->setChecked(SettingsManager::getValue(QLatin1String("Search/SearchEnginesSuggestions")).toBool());

				QMenu *addSearchMenu = new QMenu(m_ui->addSearchButton);
				addSearchMenu->addAction(tr("New..."));
				addSearchMenu->addAction(tr("Readd"))->setMenu(new QMenu(m_ui->addSearchButton));

				m_ui->addSearchButton->setMenu(addSearchMenu);
				m_ui->moveDownSearchButton->setIcon(Utils::getIcon(QLatin1String("arrow-down")));
				m_ui->moveUpSearchButton->setIcon(Utils::getIcon(QLatin1String("arrow-up")));

				updateReaddSearchMenu();

				connect(m_ui->searchFilterLineEdit, SIGNAL(textChanged(QString)), m_ui->searchViewWidget, SLOT(setFilter(QString)));
				connect(m_ui->searchViewWidget, SIGNAL(canMoveDownChanged(bool)), m_ui->moveDownSearchButton, SLOT(setEnabled(bool)));
				connect(m_ui->searchViewWidget, SIGNAL(canMoveUpChanged(bool)), m_ui->moveUpSearchButton, SLOT(setEnabled(bool)));
				connect(m_ui->searchViewWidget, SIGNAL(needsActionsUpdate()), this, SLOT(updateSearchActions()));
				connect(m_ui->searchViewWidget, SIGNAL(modified()), this, SLOT(markModified()));
				connect(m_ui->addSearchButton->menu()->actions().at(0), SIGNAL(triggered()), this, SLOT(addSearchEngine()));
				connect(m_ui->addSearchButton->menu()->actions().at(1)->menu(), SIGNAL(triggered(QAction*)), this, SLOT(readdSearchEngine(QAction*)));
				connect(m_ui->editSearchButton, SIGNAL(clicked()), this, SLOT(editSearchEngine()));
				connect(m_ui->removeSearchButton, SIGNAL(clicked()), this, SLOT(removeSearchEngine()));
				connect(m_ui->moveDownSearchButton, SIGNAL(clicked()), m_ui->searchViewWidget, SLOT(moveDownRow()));
				connect(m_ui->moveUpSearchButton, SIGNAL(clicked()), m_ui->searchViewWidget, SLOT(moveUpRow()));
			}

			break;
		default:
			{
				m_ui->notificationsPlaySoundButton->setIcon(Utils::getIcon(QLatin1String("media-playback-start")));

				QStringList notificationsLabels;
				notificationsLabels << tr("Name") << tr("Description");

				QStandardItemModel *notificationsModel = new QStandardItemModel(this);
				notificationsModel->setHorizontalHeaderLabels(notificationsLabels);

				const QVector<EventDefinition> events = NotificationsManager::getEventDefinitions();

				for (int i = 0; i < events.count(); ++i)
				{
					QList<QStandardItem*> items;
					items.append(new QStandardItem(QCoreApplication::translate("notifications", events.at(i).title.toUtf8())));
					items[0]->setData(events.at(i).identifier, Qt::UserRole);
					items[0]->setData(events.at(i).playSound, (Qt::UserRole + 1));
					items[0]->setData(events.at(i).showAlert, (Qt::UserRole + 2));
					items[0]->setData(events.at(i).showNotification, (Qt::UserRole + 3));
					items[0]->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
					items.append(new QStandardItem(QCoreApplication::translate("notifications", events.at(i).description.toUtf8())));
					items[1]->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

					notificationsModel->appendRow(items);
				}

				m_ui->notificationsItemView->setModel(notificationsModel);
				m_ui->preferNativeNotificationsCheckBox->setChecked(SettingsManager::getValue(QLatin1String("Interface/UseNativeNotifications")).toBool());

				m_ui->suggestBookmarksCheckBox->setChecked(SettingsManager::getValue(QLatin1String("AddressField/SuggestBookmarks")).toBool());

				m_ui->enableImagesCheckBox->setChecked(SettingsManager::getValue(QLatin1String("Browser/EnableImages")).toBool());
				m_ui->enableJavaScriptCheckBox->setChecked(SettingsManager::getValue(QLatin1String("Browser/EnableJavaScript")).toBool());
				m_ui->javaScriptOptionsButton->setEnabled(m_ui->enableJavaScriptCheckBox->isChecked());
				m_ui->enableJavaCheckBox->setChecked(SettingsManager::getValue(QLatin1String("Browser/EnableJava")).toBool());

				m_ui->pluginsComboBox->addItem(tr("Enabled"), QLatin1String("enabled"));
				m_ui->pluginsComboBox->addItem(tr("On demand"), QLatin1String("onDemand"));
				m_ui->pluginsComboBox->addItem(tr("Disabled"), QLatin1String("disabled"));

				const int pluginsIndex = m_ui->pluginsComboBox->findData(SettingsManager::getValue(QLatin1String("Browser/EnablePlugins")).toString());

				m_ui->pluginsComboBox->setCurrentIndex((pluginsIndex < 0) ? 1 : pluginsIndex);
				m_ui->userStyleSheetFilePathWidget->setPath(SettingsManager::getValue(QLatin1String("Content/UserStyleSheet")).toString());

				m_ui->sendReferrerCheckBox->setChecked(SettingsManager::getValue(QLatin1String("Network/EnableReferrer")).toBool());

				const QStringList userAgents = NetworkManagerFactory::getUserAgents();

				m_ui->userAgentComboBox->addItem(tr("Default"), QLatin1String("default"));

				for (int i = 0; i < userAgents.count(); ++i)
				{
					const UserAgentInformation userAgent = NetworkManagerFactory::getUserAgent(userAgents.at(i));
					const QString title = userAgent.title;

					m_ui->userAgentComboBox->addItem((title.isEmpty() ? tr("(Untitled)") : title), userAgents.at(i));
					m_ui->userAgentComboBox->setItemData((i + 1), userAgent.value, (Qt::UserRole + 1));
				}

				m_ui->userAgentComboBox->setCurrentIndex(m_ui->userAgentComboBox->findData(SettingsManager::getValue(QLatin1String("Network/UserAgent")).toString()));

				const QString proxyString = SettingsManager::getValue(QLatin1String("Network/ProxyMode")).toString();
				int proxyIndex = 0;

				if (proxyString == QLatin1String("system"))
				{
					proxyIndex = 1;
				}
				else if (proxyString == QLatin1String("manual"))
				{
					proxyIndex = 2;
				}
				else if (proxyString == QLatin1String("automatic"))
				{
					proxyIndex = 3;
				}

				m_ui->proxyModeComboBox->setCurrentIndex(proxyIndex);

				if (proxyIndex == 2 || proxyIndex == 3)
				{
					proxyModeChanged(proxyIndex);
				}

				m_ui->allProxyCheckBox->setChecked(SettingsManager::getValue(QLatin1String("Proxy/UseCommon")).toBool());
				m_ui->httpProxyCheckBox->setChecked(SettingsManager::getValue(QLatin1String("Proxy/UseHttp")).toBool());
				m_ui->httpsProxyCheckBox->setChecked(SettingsManager::getValue(QLatin1String("Proxy/UseHttps")).toBool());
				m_ui->ftpProxyCheckBox->setChecked(SettingsManager::getValue(QLatin1String("Proxy/UseFtp")).toBool());
				m_ui->socksProxyCheckBox->setChecked(SettingsManager::getValue(QLatin1String("Proxy/UseSocks")).toBool());
				m_ui->allProxyServersLineEdit->setText(SettingsManager::getValue(QLatin1String("Proxy/CommonServers")).toString());
				m_ui->httpProxyServersLineEdit->setText(SettingsManager::getValue(QLatin1String("Proxy/HttpServers")).toString());
				m_ui->httpsProxyServersLineEdit->setText(SettingsManager::getValue(QLatin1String("Proxy/HttpsServers")).toString());
				m_ui->ftpProxyServersLineEdit->setText(SettingsManager::getValue(QLatin1String("Proxy/FtpServers")).toString());
				m_ui->socksProxyServersLineEdit->setText(SettingsManager::getValue(QLatin1String("Proxy/SocksServers")).toString());
				m_ui->allProxyPortSpinBox->setValue(SettingsManager::getValue(QLatin1String("Proxy/CommonPort")).toInt());
				m_ui->httpProxyPortSpinBox->setValue(SettingsManager::getValue(QLatin1String("Proxy/HttpPort")).toInt());
				m_ui->httpsProxyPortSpinBox->setValue(SettingsManager::getValue(QLatin1String("Proxy/HttpsPort")).toInt());
				m_ui->ftpProxyPortSpinBox->setValue(SettingsManager::getValue(QLatin1String("Proxy/FtpPort")).toInt());
				m_ui->socksProxyPortSpinBox->setValue(SettingsManager::getValue(QLatin1String("Proxy/SocksPort")).toInt());
				m_ui->automaticProxyConfigurationFilePathWidget->setPath(SettingsManager::getValue(QLatin1String("Proxy/AutomaticConfigurationPath")).toString());
				m_ui->proxySystemAuthentication->setChecked(SettingsManager::getValue(QLatin1String("Proxy/UseSystemAuthentication")).toBool());

				m_ui->ciphersAddButton->setMenu(new QMenu(m_ui->ciphersAddButton));

				if (QSslSocket::supportsSsl())
				{
					QStandardItemModel *ciphersModel = new QStandardItemModel(this);
					const bool useDefaultCiphers = (SettingsManager::getValue(QLatin1String("Security/Ciphers")).toString() == QLatin1String("default"));
					const QStringList selectedCiphers = (useDefaultCiphers ? QStringList() : SettingsManager::getValue(QLatin1String("Security/Ciphers")).toStringList());
					const QList<QSslCipher> defaultCiphers = NetworkManagerFactory::getDefaultCiphers();
					const QList<QSslCipher> supportedCiphers = QSslSocket::supportedCiphers();

					for (int i = 0; i < supportedCiphers.count(); ++i)
					{
						if ((useDefaultCiphers && defaultCiphers.contains(supportedCiphers.at(i))) || (!useDefaultCiphers && (selectedCiphers.isEmpty() || selectedCiphers.contains(supportedCiphers.at(i).name()))))
						{
							QStandardItem *item = new QStandardItem(supportedCiphers.at(i).name());
							item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled);

							ciphersModel->appendRow(item);
						}
						else
						{
							m_ui->ciphersAddButton->menu()->addAction(supportedCiphers.at(i).name());
						}
					}

					m_ui->ciphersViewWidget->setModel(ciphersModel);
					m_ui->ciphersViewWidget->setItemDelegate(new OptionDelegate(true, this));
					m_ui->ciphersAddButton->setEnabled(m_ui->ciphersAddButton->menu()->actions().count() > 0);
				}
				else
				{
					m_ui->ciphersViewWidget->setEnabled(false);
				}

				m_ui->ciphersMoveDownButton->setIcon(Utils::getIcon(QLatin1String("arrow-down")));
				m_ui->ciphersMoveUpButton->setIcon(Utils::getIcon(QLatin1String("arrow-up")));

				m_ui->actionShortcutsMoveDownButton->setIcon(Utils::getIcon(QLatin1String("arrow-down")));
				m_ui->actionShortcutsMoveUpButton->setIcon(Utils::getIcon(QLatin1String("arrow-up")));

				QStandardItemModel *shortcutsProfilesModel = new QStandardItemModel(this);
				const QStringList shortcutsProfiles = SettingsManager::getValue(QLatin1String("Browser/KeyboardShortcutsProfilesOrder")).toStringList();

				for (int i = 0; i < shortcutsProfiles.count(); ++i)
				{
					const ShortcutsProfile profile = loadKeyboardProfile(shortcutsProfiles.at(i), true);

					if (profile.identifier.isEmpty())
					{
						continue;
					}

					m_shortcutsProfiles[shortcutsProfiles.at(i)] = profile;

					QStandardItem *item = new QStandardItem(profile.title.isEmpty() ? tr("(Untitled)") : profile.title);
					item->setToolTip(profile.description);
					item->setData(profile.identifier, Qt::UserRole);
					item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled);

					shortcutsProfilesModel->appendRow(item);
				}

				m_ui->actionShortcutsViewWidget->setModel(shortcutsProfilesModel);
				m_ui->actionShortcutsViewWidget->setItemDelegate(new OptionDelegate(true, this));

				QMenu *addKeyboardProfileMenu = new QMenu(m_ui->actionShortcutsAddButton);
				addKeyboardProfileMenu->addAction(tr("New..."));
				addKeyboardProfileMenu->addAction(tr("Readd"))->setMenu(new QMenu(m_ui->actionShortcutsAddButton));

				m_ui->actionShortcutsAddButton->setMenu(addKeyboardProfileMenu);

				m_ui->enableTrayIconCheckBox->setChecked(SettingsManager::getValue(QLatin1String("Browser/EnableTrayIcon")).toBool());

				updateReaddKeyboardProfileMenu();

				connect(m_ui->notificationsItemView, SIGNAL(needsActionsUpdate()), this, SLOT(updateNotificationsActions()));
				connect(m_ui->notificationsPlaySoundButton, SIGNAL(clicked()), this, SLOT(playNotificationSound()));
				connect(m_ui->enableJavaScriptCheckBox, SIGNAL(toggled(bool)), m_ui->javaScriptOptionsButton, SLOT(setEnabled(bool)));
				connect(m_ui->javaScriptOptionsButton, SIGNAL(clicked()), this, SLOT(updateJavaScriptOptions()));
				connect(m_ui->userAgentButton, SIGNAL(clicked()), this, SLOT(manageUserAgents()));
				connect(m_ui->proxyModeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(proxyModeChanged(int)));
				connect(m_ui->ciphersViewWidget, SIGNAL(canMoveDownChanged(bool)), m_ui->ciphersMoveDownButton, SLOT(setEnabled(bool)));
				connect(m_ui->ciphersViewWidget, SIGNAL(canMoveUpChanged(bool)), m_ui->ciphersMoveUpButton, SLOT(setEnabled(bool)));
				connect(m_ui->ciphersViewWidget, SIGNAL(needsActionsUpdate()), this, SLOT(updateCiphersActions()));
				connect(m_ui->ciphersViewWidget, SIGNAL(modified()), this, SLOT(markModified()));
				connect(m_ui->ciphersAddButton->menu(), SIGNAL(triggered(QAction*)), this, SLOT(addCipher(QAction*)));
				connect(m_ui->ciphersRemoveButton, SIGNAL(clicked()), this, SLOT(removeCipher()));
				connect(m_ui->ciphersMoveDownButton, SIGNAL(clicked()), m_ui->ciphersViewWidget, SLOT(moveDownRow()));
				connect(m_ui->ciphersMoveUpButton, SIGNAL(clicked()), m_ui->ciphersViewWidget, SLOT(moveUpRow()));
				connect(m_ui->actionShortcutsViewWidget, SIGNAL(canMoveDownChanged(bool)), m_ui->actionShortcutsMoveDownButton, SLOT(setEnabled(bool)));
				connect(m_ui->actionShortcutsViewWidget, SIGNAL(canMoveUpChanged(bool)), m_ui->actionShortcutsMoveUpButton, SLOT(setEnabled(bool)));
				connect(m_ui->actionShortcutsViewWidget, SIGNAL(needsActionsUpdate()), this, SLOT(updateKeyboardProfileActions()));
				connect(m_ui->actionShortcutsViewWidget, SIGNAL(modified()), this, SLOT(markModified()));
				connect(m_ui->actionShortcutsAddButton->menu()->actions().at(0), SIGNAL(triggered()), this, SLOT(addKeyboardProfile()));
				connect(m_ui->actionShortcutsAddButton->menu()->actions().at(1)->menu(), SIGNAL(triggered(QAction*)), this, SLOT(readdKeyboardProfile(QAction*)));
				connect(m_ui->actionShortcutsEditButton, SIGNAL(clicked()), this, SLOT(editKeyboardProfile()));
				connect(m_ui->actionShortcutsCloneButton, SIGNAL(clicked()), this, SLOT(cloneKeyboardProfile()));
				connect(m_ui->actionShortcutsRemoveButton, SIGNAL(clicked()), this, SLOT(removeKeyboardProfile()));
				connect(m_ui->actionShortcutsMoveDownButton, SIGNAL(clicked()), m_ui->actionShortcutsViewWidget, SLOT(moveDownRow()));
				connect(m_ui->actionShortcutsMoveUpButton, SIGNAL(clicked()), m_ui->actionShortcutsViewWidget, SLOT(moveUpRow()));
			}

			break;
	}

	QWidget *widget = m_ui->tabWidget->widget(tab);
	QList<QLineEdit*> lineEdits = widget->findChildren<QLineEdit*>();

	for (int i = 0; i < lineEdits.count(); ++i)
	{
		connect(lineEdits.at(i), SIGNAL(textChanged(QString)), this, SLOT(markModified()));
	}

	QList<QAbstractButton*> buttons = widget->findChildren<QAbstractButton*>();

	for (int i = 0; i < buttons.count(); ++i)
	{
		connect(buttons.at(i), SIGNAL(toggled(bool)), this, SLOT(markModified()));
	}

	QList<QComboBox*> comboBoxes = widget->findChildren<QComboBox*>();

	for (int i = 0; i < comboBoxes.count(); ++i)
	{
		connect(comboBoxes.at(i), SIGNAL(currentIndexChanged(int)), this, SLOT(markModified()));
	}
}

void PreferencesDialog::useCurrentAsHomePage()
{
	WindowsManager *manager = SessionsManager::getWindowsManager();

	if (manager)
	{
		m_ui->homePageLineEdit->setText(manager->getUrl().toString(QUrl::RemovePassword));
	}
}

void PreferencesDialog::useBookmarkAsHomePage(QAction *action)
{
	if (action)
	{
		const QString url = action->data().toModelIndex().data(BookmarksModel::UrlRole).toString();

		if (!url.isEmpty())
		{
			m_ui->homePageLineEdit->setText(url);
		}
	}
}

void PreferencesDialog::restoreHomePage()
{
	m_ui->homePageLineEdit->setText(SettingsManager::getDefaultValue(QLatin1String("Browser/HomePage")).toString());
}

void PreferencesDialog::currentFontChanged(int currentRow, int currentColumn, int previousRow, int previousColumn)
{
	Q_UNUSED(currentColumn)
	Q_UNUSED(previousColumn)

	QTableWidgetItem *previousItem = m_ui->fontsWidget->item(previousRow, 1);

	m_ui->fontsWidget->closePersistentEditor(previousItem);

	if (currentRow >= 0 && currentRow < m_ui->fontsWidget->rowCount())
	{
		m_ui->fontsWidget->openPersistentEditor(m_ui->fontsWidget->item(currentRow, 1));
	}
}

void PreferencesDialog::fontChanged(QWidget *editor)
{
	OptionWidget *widget = qobject_cast<OptionWidget*>(editor);

	if (widget && widget->getIndex().row() >= 0 && widget->getIndex().row() < m_ui->fontsWidget->rowCount())
	{
		m_ui->fontsWidget->item(widget->getIndex().row(), 1)->setText(m_ui->fontsWidget->item(widget->getIndex().row(), 1)->data(Qt::EditRole).toString());
		m_ui->fontsWidget->item(widget->getIndex().row(), 2)->setFont(QFont(widget->getValue().toString()));
	}

	markModified();
}

void PreferencesDialog::currentColorChanged(int currentRow, int currentColumn, int previousRow, int previousColumn)
{
	Q_UNUSED(currentColumn)
	Q_UNUSED(previousColumn)

	QTableWidgetItem *previousItem = m_ui->colorsWidget->item(previousRow, 1);

	m_ui->colorsWidget->closePersistentEditor(previousItem);

	if (currentRow >= 0 && currentRow < m_ui->colorsWidget->rowCount())
	{
		m_ui->colorsWidget->openPersistentEditor(m_ui->colorsWidget->item(currentRow, 1));
	}
}

void PreferencesDialog::colorChanged(QWidget *editor)
{
	OptionWidget *widget = qobject_cast<OptionWidget*>(editor);

	if (widget && widget->getIndex().row() >= 0 && widget->getIndex().row() < m_ui->colorsWidget->rowCount())
	{
		m_ui->colorsWidget->item(widget->getIndex().row(), 1)->setBackgroundColor(QColor(widget->getValue().toString()));
		m_ui->colorsWidget->item(widget->getIndex().row(), 1)->setData(Qt::EditRole, widget->getValue());
	}

	markModified();
}

void PreferencesDialog::setupClearHistory()
{
	ClearHistoryDialog dialog(m_clearHisorySettings, true, this);

	if (dialog.exec() == QDialog::Accepted)
	{
		m_clearHisorySettings = dialog.getClearSettings();

		markModified();
	}

	m_ui->clearHistoryCheckBox->setChecked(!m_clearHisorySettings.isEmpty());
	m_ui->clearHistoryButton->setEnabled(!m_clearHisorySettings.isEmpty());
}

void PreferencesDialog::manageAcceptLanguage()
{
	AcceptLanguageDialog dialog(this);

	if (dialog.exec() == QDialog::Accepted)
	{
		SettingsManager::setValue(QLatin1String("Network/AcceptLanguage"), dialog.getLanguageList());
	}
}

void PreferencesDialog::addSearchEngine()
{
	QStringList identifiers;
	QStringList keywords;

	for (int i = 0; i < m_ui->searchViewWidget->getRowCount(); ++i)
	{
		identifiers.append(m_ui->searchViewWidget->getIndex(i, 0).data(Qt::UserRole).toHash().value(QLatin1String("identifier"), QString()).toString());

		const QString keyword = m_ui->searchViewWidget->getIndex(i, 1).data(Qt::DisplayRole).toString();

		if (!keyword.isEmpty())
		{
			keywords.append(keyword);
		}
	}

	const QString identifier = Utils::createIdentifier(QString(), identifiers);

	if (identifier.isEmpty())
	{
		return;
	}

	SearchInformation engine;
	engine.identifier = identifier;
	engine.title = tr("New Search Engine");
	engine.icon = Utils::getIcon(QLatin1String("edit-find"));

	SearchPropertiesDialog dialog(engine, keywords, false, this);

	if (dialog.exec() == QDialog::Rejected)
	{
		return;
	}

	engine = dialog.getSearchEngine();

	m_searchEngines[identifier] = qMakePair(true, engine);

	if (dialog.isDefault())
	{
		m_defaultSearchEngine = identifier;
	}

	QList<QStandardItem*> items;
	items.append(new QStandardItem(engine.icon, engine.title));
	items[0]->setToolTip(engine.description);
	items[0]->setData(identifier, Qt::UserRole);
	items[0]->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsDragEnabled);
	items.append(new QStandardItem(engine.keyword));
	items[1]->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsDragEnabled);

	m_ui->searchViewWidget->insertRow(items);

	markModified();
}

void PreferencesDialog::readdSearchEngine(QAction *action)
{
	if (!action || action->data().isNull())
	{
		return;
	}

	const QString identifier = action->data().toString();
	QFile file(SessionsManager::getReadableDataPath(QLatin1String("searches/") + identifier + QLatin1String(".xml")));

	if (!file.open(QIODevice::ReadOnly))
	{
		return;
	}

	const SearchInformation engine = SearchesManager::loadSearchEngine(&file, identifier);

	file.close();

	if (engine.identifier.isEmpty() || m_searchEngines.contains(identifier))
	{
		return;
	}

	m_searchEngines[identifier] = qMakePair(false, engine);

	QList<QStandardItem*> items;
	items.append(new QStandardItem(engine.icon, engine.title));
	items[0]->setToolTip(engine.description);
	items[0]->setData(identifier, Qt::UserRole);
	items[0]->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsDragEnabled);
	items.append(new QStandardItem(engine.keyword));
	items[1]->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsDragEnabled);

	m_ui->searchViewWidget->insertRow(items);

	updateReaddSearchMenu();
	markModified();
}

void PreferencesDialog::editSearchEngine()
{
	const QModelIndex index = m_ui->searchViewWidget->getIndex(m_ui->searchViewWidget->getCurrentRow(), 0);
	const QString identifier = index.data(Qt::UserRole).toString();

	if (identifier.isEmpty() || !m_searchEngines.contains(identifier))
	{
		return;
	}

	QStringList keywords;

	for (int i = 0; i < m_ui->searchViewWidget->getRowCount(); ++i)
	{
		const QString keyword = m_ui->searchViewWidget->getIndex(i, 1).data(Qt::DisplayRole).toString();

		if (m_ui->searchViewWidget->getCurrentRow() != i && !keyword.isEmpty())
		{
			keywords.append(keyword);
		}
	}

	SearchPropertiesDialog dialog(m_searchEngines[identifier].second, keywords, (identifier == m_defaultSearchEngine), this);

	if (dialog.exec() == QDialog::Rejected)
	{
		return;
	}

	SearchInformation engine = dialog.getSearchEngine();

	if (keywords.contains(engine.keyword))
	{
		engine.keyword = QString();
	}

	m_searchEngines[identifier] = qMakePair(true, engine);

	if (dialog.isDefault())
	{
		m_defaultSearchEngine = identifier;
	}

	m_ui->searchViewWidget->setData(index, engine.title, Qt::DisplayRole);
	m_ui->searchViewWidget->setData(index, engine.description, Qt::ToolTipRole);
	m_ui->searchViewWidget->setData(index, engine.icon, Qt::DecorationRole);
	m_ui->searchViewWidget->setData(m_ui->searchViewWidget->getIndex(index.row(), 1), engine.keyword, Qt::DisplayRole);

	markModified();
}

void PreferencesDialog::removeSearchEngine()
{
	const QString identifier = m_ui->searchViewWidget->getIndex(m_ui->searchViewWidget->getCurrentRow(), 0).data(Qt::UserRole).toString();

	if (identifier.isEmpty() || !m_searchEngines.contains(identifier))
	{
		return;
	}

	QMessageBox messageBox;
	messageBox.setWindowTitle(tr("Question"));
	messageBox.setText(tr("Do you really want to remove this search engine?"));
	messageBox.setIcon(QMessageBox::Question);
	messageBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
	messageBox.setDefaultButton(QMessageBox::Cancel);

	const QString path = SessionsManager::getWritableDataPath(QLatin1String("searches/") + identifier + QLatin1String(".xml"));

	if (QFile::exists(path))
	{
		messageBox.setCheckBox(new QCheckBox(tr("Delete search engine permanently")));
	}

	if (messageBox.exec() == QMessageBox::Yes)
	{
		if (messageBox.checkBox() && messageBox.checkBox()->isChecked())
		{
			m_filesToRemove.append(path);
		}

		m_searchEngines.remove(identifier);

		m_ui->searchViewWidget->removeRow();

		updateReaddSearchMenu();
		markModified();
	}
}

void PreferencesDialog::updateSearchActions()
{
	const int currentRow = m_ui->searchViewWidget->getCurrentRow();
	const bool isSelected = (currentRow >= 0 && currentRow < m_ui->searchViewWidget->getRowCount());

	m_ui->editSearchButton->setEnabled(isSelected);
	m_ui->removeSearchButton->setEnabled(isSelected);
}

void PreferencesDialog::playNotificationSound()
{
	QSoundEffect *effect = new QSoundEffect(this);
	effect->setSource(QUrl::fromLocalFile(m_ui->notificationsPlaySoundFilePathWidget->getPath()));
	effect->setLoopCount(1);
	effect->setVolume(0.5);

	if (effect->status() == QSoundEffect::Error)
	{
		effect->deleteLater();
	}
	else
	{
		QTimer::singleShot(10000, effect, SLOT(deleteLater()));

		effect->play();
	}
}

void PreferencesDialog::updateNotificationsActions()
{
	disconnect(m_ui->notificationsPlaySoundFilePathWidget, SIGNAL(pathChanged()), this, SLOT(updateNotificationsOptions()));
	disconnect(m_ui->notificationsShowAlertCheckBox, SIGNAL(clicked()), this, SLOT(updateNotificationsOptions()));
	disconnect(m_ui->notificationsShowNotificationCheckBox, SIGNAL(clicked()), this, SLOT(updateNotificationsOptions()));

	const QModelIndex index = m_ui->notificationsItemView->getIndex(m_ui->notificationsItemView->getCurrentRow());

	m_ui->notificationOptionsWidget->setEnabled(index.isValid());
	m_ui->notificationsPlaySoundFilePathWidget->setPath(index.data(Qt::UserRole + 1).toString());
	m_ui->notificationsPlaySoundFilePathWidget->setFilter(tr("WAV files (*.wav)"));
	m_ui->notificationsShowAlertCheckBox->setChecked(index.data(Qt::UserRole + 2).toBool());
	m_ui->notificationsShowNotificationCheckBox->setChecked(index.data(Qt::UserRole + 3).toBool());

	connect(m_ui->notificationsPlaySoundFilePathWidget, SIGNAL(pathChanged()), this, SLOT(updateNotificationsOptions()));
	connect(m_ui->notificationsShowAlertCheckBox, SIGNAL(clicked()), this, SLOT(updateNotificationsOptions()));
	connect(m_ui->notificationsShowNotificationCheckBox, SIGNAL(clicked()), this, SLOT(updateNotificationsOptions()));
}

void PreferencesDialog::updateNotificationsOptions()
{
	const QModelIndex index = m_ui->notificationsItemView->getIndex(m_ui->notificationsItemView->getCurrentRow());

	if (index.isValid())
	{
		m_ui->notificationsItemView->setData(index, m_ui->notificationsPlaySoundFilePathWidget->getPath(), (Qt::UserRole + 1));
		m_ui->notificationsItemView->setData(index, m_ui->notificationsShowAlertCheckBox->isChecked(), (Qt::UserRole + 2));
		m_ui->notificationsItemView->setData(index, m_ui->notificationsShowNotificationCheckBox->isChecked(), (Qt::UserRole + 3));

		markModified();
	}
}

void PreferencesDialog::updateReaddSearchMenu()
{
	if (!m_ui->addSearchButton->menu())
	{
		return;
	}

	QStringList availableIdentifiers;
	QList<SearchInformation> availableSearchEngines;
	QList<QFileInfo> allSearchEngines = QDir(SessionsManager::getReadableDataPath(QLatin1String("searches"))).entryInfoList(QDir::Files);
	allSearchEngines.append(QDir(SessionsManager::getReadableDataPath(QLatin1String("searches"), true)).entryInfoList(QDir::Files));

	for (int i = 0; i < allSearchEngines.count(); ++i)
	{
		const QString identifier = allSearchEngines.at(i).baseName();

		if (!m_searchEngines.contains(identifier) && !availableIdentifiers.contains(identifier))
		{
			QFile file(allSearchEngines.at(i).absoluteFilePath());

			if (file.open(QFile::ReadOnly))
			{
				const SearchInformation engine = SearchesManager::loadSearchEngine(&file, identifier);

				if (!engine.identifier.isEmpty())
				{
					availableIdentifiers.append(identifier);

					availableSearchEngines.append(engine);
				}

				file.close();
			}
		}
	}

	m_ui->addSearchButton->menu()->actions().at(1)->menu()->clear();
	m_ui->addSearchButton->menu()->actions().at(1)->menu()->setEnabled(!availableSearchEngines.isEmpty());

	for (int i = 0; i < availableSearchEngines.count(); ++i)
	{
		m_ui->addSearchButton->menu()->actions().at(1)->menu()->addAction(availableSearchEngines.at(i).icon, (availableSearchEngines.at(i).title.isEmpty() ? tr("(Untitled)") : availableSearchEngines.at(i).title))->setData(availableSearchEngines.at(i).identifier);
	}
}

void PreferencesDialog::manageUserAgents()
{
	QList<UserAgentInformation> userAgents;

	for (int i = 1; i < m_ui->userAgentComboBox->count(); ++i)
	{
		UserAgentInformation userAgent;
		userAgent.identifier = m_ui->userAgentComboBox->itemData(i, Qt::UserRole).toString();
		userAgent.title = m_ui->userAgentComboBox->itemText(i);
		userAgent.value = m_ui->userAgentComboBox->itemData(i, (Qt::UserRole + 1)).toString();

		userAgents.append(userAgent);
	}

	const QString selectedUserAgent = m_ui->userAgentComboBox->currentData().toString();

	UserAgentsManagerDialog dialog(userAgents, this);

	if (dialog.exec() == QDialog::Accepted)
	{
		m_userAgentsModified = true;

		m_ui->userAgentComboBox->clear();

		userAgents = dialog.getUserAgents();

		m_ui->userAgentComboBox->addItem(tr("Default"), QLatin1String("default"));

		for (int i = 0; i < userAgents.count(); ++i)
		{
			const QString title = userAgents.at(i).title;

			m_ui->userAgentComboBox->addItem((title.isEmpty() ? tr("(Untitled)") : title), userAgents.at(i).identifier);
			m_ui->userAgentComboBox->setItemData((i + 1), userAgents.at(i).value, (Qt::UserRole + 1));
		}

		m_ui->userAgentComboBox->setCurrentIndex(m_ui->userAgentComboBox->findData(selectedUserAgent));
	}
}

void PreferencesDialog::proxyModeChanged(int index)
{
	if (index == 2)
	{
		m_ui->manualProxyConfigurationWidget->setEnabled(true);
		m_ui->allProxyCheckBox->setEnabled(true);
		m_ui->allProxyServersLineEdit->setEnabled(true);
		m_ui->allProxyPortSpinBox->setEnabled(true);

		if (!m_ui->allProxyCheckBox->isChecked())
		{
			m_ui->httpProxyCheckBox->setEnabled(true);
			m_ui->httpsProxyCheckBox->setEnabled(true);
			m_ui->ftpProxyCheckBox->setEnabled(true);
			m_ui->socksProxyCheckBox->setEnabled(true);
			m_ui->httpProxyServersLineEdit->setEnabled(true);
			m_ui->httpProxyPortSpinBox->setEnabled(true);
			m_ui->httpsProxyServersLineEdit->setEnabled(true);
			m_ui->httpsProxyPortSpinBox->setEnabled(true);
			m_ui->ftpProxyServersLineEdit->setEnabled(true);
			m_ui->ftpProxyPortSpinBox->setEnabled(true);
			m_ui->socksProxyServersLineEdit->setEnabled(true);
			m_ui->socksProxyPortSpinBox->setEnabled(true);
		}
	}
	else
	{
		m_ui->manualProxyConfigurationWidget->setDisabled(true);
		m_ui->allProxyCheckBox->setChecked(false);
		m_ui->httpProxyCheckBox->setChecked(false);
		m_ui->httpsProxyCheckBox->setChecked(false);
		m_ui->ftpProxyCheckBox->setChecked(false);
		m_ui->socksProxyCheckBox->setChecked(false);
	}

	if (index == 3)
	{
		m_ui->automaticProxyConfigurationWidget->setEnabled(true);
		m_ui->automaticProxyConfigurationFilePathWidget->setEnabled(true);
	}
	else
	{
		m_ui->automaticProxyConfigurationWidget->setEnabled(false);
		m_ui->automaticProxyConfigurationFilePathWidget->setEnabled(false);
	}

	if (index == 0)
	{
		m_ui->proxySystemAuthentication->setEnabled(false);
		m_ui->proxySystemAuthentication->setChecked(false);
	}
	else
	{
		m_ui->proxySystemAuthentication->setEnabled(true);
	}
}

void PreferencesDialog::addCipher(QAction *action)
{
	if (!action)
	{
		return;
	}

	QStandardItem *item = new QStandardItem(action->text());
	item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled);

	m_ui->ciphersViewWidget->insertRow(item);
	m_ui->ciphersAddButton->menu()->removeAction(action);
	m_ui->ciphersAddButton->setEnabled(m_ui->ciphersAddButton->menu()->actions().count() > 0);
}

void PreferencesDialog::removeCipher()
{
	const int currentRow = m_ui->ciphersViewWidget->getCurrentRow();

	if (currentRow >= 0)
	{
		m_ui->ciphersAddButton->menu()->addAction(m_ui->ciphersViewWidget->getIndex(currentRow, 0).data(Qt::DisplayRole).toString());
		m_ui->ciphersAddButton->setEnabled(true);

		m_ui->ciphersViewWidget->removeRow();
	}
}

void PreferencesDialog::updateCiphersActions()
{
	const int currentRow = m_ui->ciphersViewWidget->getCurrentRow();

	m_ui->ciphersRemoveButton->setEnabled(currentRow >= 0 && currentRow < m_ui->ciphersViewWidget->getRowCount());
}

void PreferencesDialog::addKeyboardProfile()
{
	const QString identifier = createProfileIdentifier(m_ui->actionShortcutsViewWidget);

	if (identifier.isEmpty())
	{
		return;
	}

	ShortcutsProfile profile;
	profile.title = tr("(Untitled)");;

	m_shortcutsProfiles[identifier] = profile;

	QStandardItem *item = new QStandardItem(tr("(Untitled)"));
	item->setData(identifier, Qt::UserRole);
	item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled);

	m_ui->actionShortcutsViewWidget->insertRow(item);

	markModified();
}

void PreferencesDialog::readdKeyboardProfile(QAction *action)
{
	if (!action || action->data().isNull())
	{
		return;
	}

	const QString identifier = action->data().toString();
	const ShortcutsProfile profile = loadKeyboardProfile(identifier, true);

	if (profile.identifier.isEmpty())
	{
		return;
	}

	m_shortcutsProfiles[identifier] = profile;

	QStandardItem *item = new QStandardItem(profile.title.isEmpty() ? tr("(Untitled)") : profile.title);
	item->setToolTip(profile.description);
	item->setData(profile.identifier, Qt::UserRole);
	item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled);

	m_ui->actionShortcutsViewWidget->insertRow(item);

	updateReaddKeyboardProfileMenu();
	markModified();
}

void PreferencesDialog::editKeyboardProfile()
{
	const QModelIndex index = m_ui->actionShortcutsViewWidget->currentIndex();
	const QString identifier = index.data(Qt::UserRole).toString();

	if (identifier.isEmpty() || !m_shortcutsProfiles.contains(identifier))
	{
		return;
	}

	ShortcutsProfileDialog dialog(identifier, m_shortcutsProfiles, this);

	if (dialog.exec() == QDialog::Rejected)
	{
		return;
	}

	m_shortcutsProfiles[identifier] = dialog.getProfile();

	m_ui->actionShortcutsViewWidget->setData(index, (m_shortcutsProfiles[identifier].title.isEmpty() ? tr("(Untitled)") : m_shortcutsProfiles[identifier].title), Qt::DisplayRole);
	m_ui->actionShortcutsViewWidget->setData(index, m_shortcutsProfiles[identifier].description, Qt::ToolTipRole);

	markModified();
}

void PreferencesDialog::cloneKeyboardProfile()
{
	const QString identifier = m_ui->actionShortcutsViewWidget->currentIndex().data(Qt::UserRole).toString();

	if (identifier.isEmpty() || !m_shortcutsProfiles.contains(identifier))
	{
		return;
	}

	const QString newIdentifier = createProfileIdentifier(m_ui->actionShortcutsViewWidget, identifier);

	if (newIdentifier.isEmpty())
	{
		return;
	}

	m_shortcutsProfiles[newIdentifier] = m_shortcutsProfiles[identifier];
	m_shortcutsProfiles[newIdentifier].identifier = newIdentifier;
	m_shortcutsProfiles[newIdentifier].isModified = true;

	QStandardItem *item = new QStandardItem(m_shortcutsProfiles[newIdentifier].title.isEmpty() ? tr("(Untitled)") : m_shortcutsProfiles[newIdentifier].title);
	item->setToolTip(m_shortcutsProfiles[newIdentifier].description);
	item->setData(newIdentifier, Qt::UserRole);
	item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled);

	m_ui->actionShortcutsViewWidget->insertRow(item);

	markModified();
}

void PreferencesDialog::removeKeyboardProfile()
{
	const QString identifier = m_ui->actionShortcutsViewWidget->currentIndex().data(Qt::UserRole).toString();

	if (identifier.isEmpty() || !m_shortcutsProfiles.contains(identifier))
	{
		return;
	}

	QMessageBox messageBox;
	messageBox.setWindowTitle(tr("Question"));
	messageBox.setText(tr("Do you really want to remove this profile?"));
	messageBox.setIcon(QMessageBox::Question);
	messageBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
	messageBox.setDefaultButton(QMessageBox::Cancel);

	const QString path = SessionsManager::getWritableDataPath(QLatin1String("keyboard/") + identifier + QLatin1String(".ini"));

	if (QFile::exists(path))
	{
		messageBox.setCheckBox(new QCheckBox(tr("Delete profile permanently")));
	}

	if (messageBox.exec() == QMessageBox::Yes)
	{
		if (messageBox.checkBox() && messageBox.checkBox()->isChecked())
		{
			m_filesToRemove.append(path);
		}

		m_shortcutsProfiles.remove(identifier);

		m_ui->actionShortcutsViewWidget->removeRow();

		updateReaddKeyboardProfileMenu();
		markModified();
	}
}

void PreferencesDialog::updateKeyboardProfileActions()
{
	const int currentRow = m_ui->actionShortcutsViewWidget->getCurrentRow();
	const bool isSelected = (currentRow >= 0 && currentRow < m_ui->actionShortcutsViewWidget->getRowCount());

	m_ui->actionShortcutsEditButton->setEnabled(isSelected);
	m_ui->actionShortcutsCloneButton->setEnabled(isSelected);
	m_ui->actionShortcutsRemoveButton->setEnabled(isSelected);
}

void PreferencesDialog::updateReaddKeyboardProfileMenu()
{
	if (!m_ui->actionShortcutsAddButton->menu())
	{
		return;
	}

	QStringList availableIdentifiers;
	QList<ShortcutsProfile> availableShortcutsProfiles;
	QList<QFileInfo> allShortcutsProfiles = QDir(SessionsManager::getReadableDataPath(QLatin1String("keyboard"))).entryInfoList(QDir::Files);
	allShortcutsProfiles.append(QDir(SessionsManager::getReadableDataPath(QLatin1String("keyboard"), true)).entryInfoList(QDir::Files));

	for (int i = 0; i < allShortcutsProfiles.count(); ++i)
	{
		const QString identifier = allShortcutsProfiles.at(i).baseName();

		if (!m_shortcutsProfiles.contains(identifier) && !availableIdentifiers.contains(identifier))
		{
			const ShortcutsProfile profile = loadKeyboardProfile(identifier, false);

			if (!profile.identifier.isEmpty())
			{
				availableIdentifiers.append(identifier);

				availableShortcutsProfiles.append(profile);
			}
		}
	}

	m_ui->actionShortcutsAddButton->menu()->actions().at(1)->menu()->clear();
	m_ui->actionShortcutsAddButton->menu()->actions().at(1)->menu()->setEnabled(!availableShortcutsProfiles.isEmpty());

	for (int i = 0; i < availableShortcutsProfiles.count(); ++i)
	{
		m_ui->actionShortcutsAddButton->menu()->actions().at(1)->menu()->addAction((availableShortcutsProfiles.at(i).title.isEmpty() ? tr("(Untitled)") : availableShortcutsProfiles.at(i).title))->setData(availableShortcutsProfiles.at(i).identifier);
	}
}

void PreferencesDialog::updateJavaScriptOptions()
{
	const bool isSet = !m_javaScriptOptions.isEmpty();

	if (!isSet)
	{
		m_javaScriptOptions[QLatin1String("Browser/JavaScriptCanShowStatusMessages")] = SettingsManager::getValue(QLatin1String("Browser/JavaScriptCanShowStatusMessages"));
		m_javaScriptOptions[QLatin1String("Browser/JavaScriptCanAccessClipboard")] = SettingsManager::getValue(QLatin1String("Browser/JavaScriptCanAccessClipboard"));
		m_javaScriptOptions[QLatin1String("Browser/JavaScriptCanDisableContextMenu")] = SettingsManager::getValue(QLatin1String("Browser/JavaScriptCanDisableContextMenu"));
		m_javaScriptOptions[QLatin1String("Browser/JavaScriptCanOpenWindows")] = SettingsManager::getValue(QLatin1String("Browser/JavaScriptCanOpenWindows"));
		m_javaScriptOptions[QLatin1String("Browser/JavaScriptCanCloseWindows")] = SettingsManager::getValue(QLatin1String("Browser/JavaScriptCanCloseWindows"));
	}

	JavaScriptPreferencesDialog dialog(m_javaScriptOptions, this);

	if (dialog.exec() == QDialog::Accepted)
	{
		m_javaScriptOptions = dialog.getOptions();

		markModified();
	}
	else if (!isSet)
	{
		m_javaScriptOptions.clear();
	}
}

void PreferencesDialog::markModified()
{
	m_ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
}

void PreferencesDialog::openConfigurationManager()
{
	const QUrl url(QLatin1String("about:config"));

	if (!SessionsManager::hasUrl(url, true))
	{
		WindowsManager *manager = SessionsManager::getWindowsManager();

		if (manager)
		{
			manager->open(url);
		}
	}
}

void PreferencesDialog::save()
{
	for (int i = 0; i < m_filesToRemove.count(); ++i)
	{
		QFile::remove(m_filesToRemove.at(i));
	}

	m_filesToRemove.clear();

	if (m_loadedTabs[0])
	{
		SettingsManager::setValue(QLatin1String("Browser/StartupBehavior"), m_ui->startupBehaviorComboBox->currentData().toString());
		SettingsManager::setValue(QLatin1String("Browser/HomePage"), m_ui->homePageLineEdit->text());
		SettingsManager::setValue(QLatin1String("Paths/Downloads"), m_ui->downloadsFilePathWidget->getPath());
		SettingsManager::setValue(QLatin1String("Browser/AlwaysAskWhereSaveFile"), m_ui->alwaysAskCheckBox->isChecked());
		SettingsManager::setValue(QLatin1String("Browser/OpenLinksInNewTab"), m_ui->tabsInsteadOfWindowsCheckBox->isChecked());
		SettingsManager::setValue(QLatin1String("Browser/DelayRestoringOfBackgroundTabs"), m_ui->delayTabsLoadingCheckBox->isChecked());
		SettingsManager::setValue(QLatin1String("Browser/ReuseCurrentTab"), m_ui->reuseCurrentTabCheckBox->isChecked());
		SettingsManager::setValue(QLatin1String("TabBar/OpenNextToActive"), m_ui->openNextToActiveheckBox->isChecked());
	}

	if (m_loadedTabs[1])
	{
		SettingsManager::setValue(QLatin1String("Content/DefaultZoom"), m_ui->defaultZoomSpinBox->value());
		SettingsManager::setValue(QLatin1String("Content/ZoomTextOnly"), m_ui->zoomTextOnlyCheckBox->isChecked());
		SettingsManager::setValue(QLatin1String("Content/DefaultFontSize"), m_ui->proportionalFontSizeSpinBox->value());
		SettingsManager::setValue(QLatin1String("Content/DefaultFixedFontSize"), m_ui->fixedFontSizeSpinBox->value());
		SettingsManager::setValue(QLatin1String("Content/MinimumFontSize"), m_ui->minimumFontSizeSpinBox->value());

		for (int i = 0; i < m_ui->fontsWidget->rowCount(); ++i)
		{
			SettingsManager::setValue(m_ui->fontsWidget->item(i, 1)->data(Qt::UserRole).toString() , m_ui->fontsWidget->item(i, 1)->data(Qt::DisplayRole));
		}

		for (int i = 0; i < m_ui->colorsWidget->rowCount(); ++i)
		{
			SettingsManager::setValue(m_ui->colorsWidget->item(i, 1)->data(Qt::UserRole).toString() , m_ui->colorsWidget->item(i, 1)->data(Qt::EditRole));
		}
	}

	if (m_loadedTabs[2])
	{
		SettingsManager::setValue(QLatin1String("Network/DoNotTrackPolicy"), m_ui->doNotTrackComboBox->currentData().toString());
		SettingsManager::setValue(QLatin1String("Browser/PrivateMode"), m_ui->privateModeCheckBox->isChecked());
		SettingsManager::setValue(QLatin1String("History/RememberBrowsing"), m_ui->rememberBrowsingHistoryCheckBox->isChecked());
		SettingsManager::setValue(QLatin1String("History/RememberDownloads"), m_ui->rememberDownloadsHistoryCheckBox->isChecked());
		SettingsManager::setValue(QLatin1String("Network/CookiesPolicy"), (m_ui->enableCookiesCheckBox->isChecked() ? m_ui->cookiesPolicyComboBox->currentData().toString() : QLatin1String("ignore")));
		SettingsManager::setValue(QLatin1String("Network/CookiesKeepMode"), m_ui->keepCookiesModeComboBox->currentData().toString());
		SettingsManager::setValue(QLatin1String("Network/ThirdPartyCookiesPolicy"), m_ui->thirdPartyCookiesPolicyComboBox->currentData().toString());
		SettingsManager::setValue(QLatin1String("History/ClearOnClose"), (m_ui->clearHistoryCheckBox->isChecked() ? m_clearHisorySettings : QStringList()));
	}

	if (m_loadedTabs[3])
	{
		QStringList searchEnginesOrder;

		for (int i = 0; i < m_ui->searchViewWidget->getRowCount(); ++i)
		{
			const QString identifier = m_ui->searchViewWidget->getIndex(i, 0).data(Qt::UserRole).toString();
			const QString keyword = m_ui->searchViewWidget->getIndex(i, 1).data(Qt::DisplayRole).toString();

			if (!identifier.isEmpty())
			{
				searchEnginesOrder.append(identifier);
			}

			if (m_searchEngines.contains(identifier) && m_searchEngines[identifier].second.keyword != keyword)
			{
				m_searchEngines[identifier].first = true;
				m_searchEngines[identifier].second.keyword = keyword;
			}
		}

		QDir().mkpath(SessionsManager::getWritableDataPath(QLatin1String("searches")));

		QHash<QString, QPair<bool, SearchInformation> >::iterator searchEnginesIterator;

		for (searchEnginesIterator = m_searchEngines.begin(); searchEnginesIterator != m_searchEngines.end(); ++searchEnginesIterator)
		{
			if (searchEnginesIterator.value().first)
			{
				SearchesManager::saveSearchEngine(searchEnginesIterator.value().second);
			}
		}

		if (SettingsManager::getValue(QLatin1String("Search/SearchEnginesOrder")).toStringList() == searchEnginesOrder)
		{
			SearchesManager::loadSearchEngines();
		}
		else
		{
			SettingsManager::setValue(QLatin1String("Search/SearchEnginesOrder"), searchEnginesOrder);
		}

		SettingsManager::setValue(QLatin1String("Search/DefaultSearchEngine"), m_defaultSearchEngine);
		SettingsManager::setValue(QLatin1String("Search/SearchEnginesSuggestions"), m_ui->searchSuggestionsCheckBox->isChecked());

		updateReaddSearchMenu();
	}

	if (m_loadedTabs[4])
	{
		QSettings notificationsSettings(SessionsManager::getWritableDataPath(QLatin1String("notifications.ini")), QSettings::IniFormat);
		notificationsSettings.setIniCodec("UTF-8");
		notificationsSettings.clear();

		for (int i = 0; i < m_ui->notificationsItemView->getRowCount(); ++i)
		{
			const QModelIndex index = m_ui->notificationsItemView->getIndex(i, 0);
			const QString eventName = NotificationsManager::getEventName(index.data(Qt::UserRole).toInt());

			if (eventName.isEmpty())
			{
				continue;
			}

			notificationsSettings.beginGroup(eventName);
			notificationsSettings.setValue(QLatin1String("playSound"), index.data(Qt::UserRole + 1).toString());
			notificationsSettings.setValue(QLatin1String("showAlert"), index.data(Qt::UserRole + 2).toBool());
			notificationsSettings.setValue(QLatin1String("showNotification"), index.data(Qt::UserRole + 3).toBool());
			notificationsSettings.endGroup();
		}

		SettingsManager::setValue(QLatin1String("Interface/UseNativeNotifications"), m_ui->preferNativeNotificationsCheckBox->isChecked());

		SettingsManager::setValue(QLatin1String("AddressField/SuggestBookmarks"), m_ui->suggestBookmarksCheckBox->isChecked());

		SettingsManager::setValue(QLatin1String("Browser/EnableImages"), m_ui->enableImagesCheckBox->isChecked());
		SettingsManager::setValue(QLatin1String("Browser/EnableJavaScript"), m_ui->enableJavaScriptCheckBox->isChecked());
		SettingsManager::setValue(QLatin1String("Browser/EnableJava"), m_ui->enableJavaCheckBox->isChecked());
		SettingsManager::setValue(QLatin1String("Browser/EnablePlugins"), m_ui->pluginsComboBox->currentData(Qt::UserRole).toString());

		SettingsManager::setValue(QLatin1String("Content/UserStyleSheet"), m_ui->userStyleSheetFilePathWidget->getPath());

		SettingsManager::setValue(QLatin1String("Network/EnableReferrer"), m_ui->sendReferrerCheckBox->isChecked());
		SettingsManager::setValue(QLatin1String("Network/UserAgent"), m_ui->userAgentComboBox->currentData().toString());

		const int proxyIndex = m_ui->proxyModeComboBox->currentIndex();
		QLatin1String proxyString = QLatin1String("noproxy");

		if (proxyIndex == 1)
		{
			proxyString = QLatin1String("system");
		}
		else if (proxyIndex == 2)
		{
			proxyString = QLatin1String("manual");
		}
		else if (proxyIndex == 3)
		{
			proxyString = QLatin1String("automatic");
		}

		SettingsManager::setValue(QLatin1String("Network/ProxyMode"), proxyString);

		if (!m_ui->allProxyServersLineEdit->text().isEmpty())
		{
			SettingsManager::setValue(QLatin1String("Proxy/UseCommon"), m_ui->allProxyCheckBox->isChecked());
		}

		if (!m_ui->httpProxyServersLineEdit->text().isEmpty())
		{
			SettingsManager::setValue(QLatin1String("Proxy/UseHttp"), m_ui->httpProxyCheckBox->isChecked());
		}

		if (!m_ui->httpsProxyServersLineEdit->text().isEmpty())
		{
			SettingsManager::setValue(QLatin1String("Proxy/UseHttps"), m_ui->httpsProxyCheckBox->isChecked());
		}

		if (!m_ui->ftpProxyServersLineEdit->text().isEmpty())
		{
			SettingsManager::setValue(QLatin1String("Proxy/UseFtp"), m_ui->ftpProxyCheckBox->isChecked());
		}

		if (!m_ui->socksProxyServersLineEdit->text().isEmpty())
		{
			SettingsManager::setValue(QLatin1String("Proxy/UseSocks"), m_ui->socksProxyCheckBox->isChecked());
		}

		SettingsManager::setValue(QLatin1String("Proxy/CommonServers"), m_ui->allProxyServersLineEdit->text());
		SettingsManager::setValue(QLatin1String("Proxy/HttpServers"), m_ui->httpProxyServersLineEdit->text());
		SettingsManager::setValue(QLatin1String("Proxy/HttpsServers"), m_ui->httpsProxyServersLineEdit->text());
		SettingsManager::setValue(QLatin1String("Proxy/FtpServers"), m_ui->ftpProxyServersLineEdit->text());
		SettingsManager::setValue(QLatin1String("Proxy/SocksServers"), m_ui->socksProxyServersLineEdit->text());
		SettingsManager::setValue(QLatin1String("Proxy/CommonPort"), m_ui->allProxyPortSpinBox->value());
		SettingsManager::setValue(QLatin1String("Proxy/HttpPort"), m_ui->httpProxyPortSpinBox->value());
		SettingsManager::setValue(QLatin1String("Proxy/HttpsPort"), m_ui->httpsProxyPortSpinBox->value());
		SettingsManager::setValue(QLatin1String("Proxy/FtpPort"), m_ui->ftpProxyPortSpinBox->value());
		SettingsManager::setValue(QLatin1String("Proxy/SocksPort"), m_ui->socksProxyPortSpinBox->value());
		SettingsManager::setValue(QLatin1String("Proxy/AutomaticConfigurationPath"), m_ui->automaticProxyConfigurationFilePathWidget->getPath());
		SettingsManager::setValue(QLatin1String("Proxy/UseSystemAuthentication"), m_ui->proxySystemAuthentication->isChecked());

		if (m_userAgentsModified)
		{
			QSettings userAgents(SessionsManager::getWritableDataPath(QLatin1String("userAgents.ini")), QSettings::IniFormat);
			userAgents.setIniCodec("UTF-8");
			userAgents.clear();

			for (int i = 1; i < m_ui->userAgentComboBox->count(); ++i)
			{
				userAgents.setValue(m_ui->userAgentComboBox->itemData(i, Qt::UserRole).toString() + "/title", m_ui->userAgentComboBox->itemText(i));
				userAgents.setValue(m_ui->userAgentComboBox->itemData(i, Qt::UserRole).toString() + "/value", m_ui->userAgentComboBox->itemData(i, (Qt::UserRole + 1)).toString());
			}

			userAgents.sync();

			NetworkManagerFactory::loadUserAgents();
		}

		if (m_ui->ciphersViewWidget->isModified())
		{
			QStringList ciphers;

			for (int i = 0; i < m_ui->ciphersViewWidget->getRowCount(); ++i)
			{
				ciphers.append(m_ui->ciphersViewWidget->getIndex(i, 0).data(Qt::DisplayRole).toString());
			}

			SettingsManager::setValue(QLatin1String("Security/Ciphers"), ciphers);
		}

		QDir().mkpath(SessionsManager::getWritableDataPath(QLatin1String("keyboard")));

		QHash<QString, ShortcutsProfile>::iterator shortcutsProfilesIterator;

		for (shortcutsProfilesIterator = m_shortcutsProfiles.begin(); shortcutsProfilesIterator != m_shortcutsProfiles.end(); ++shortcutsProfilesIterator)
		{
			if (!shortcutsProfilesIterator.value().isModified)
			{
				continue;
			}

			QFile file(SessionsManager::getWritableDataPath(QLatin1String("keyboard/") + shortcutsProfilesIterator.key() + QLatin1String(".ini")));

			if (!file.open(QIODevice::WriteOnly))
			{
				continue;
			}

			QTextStream stream(&file);
			stream.setCodec("UTF-8");
			stream << QLatin1String("; Title: ") << (shortcutsProfilesIterator.value().title.isEmpty() ? tr("(Untitled)") : shortcutsProfilesIterator.value().title) << QLatin1Char('\n');
			stream << QLatin1String("; Description: ") << shortcutsProfilesIterator.value().description << QLatin1Char('\n');
			stream << QLatin1String("; Type: keyboard-profile\n");
			stream << QLatin1String("; Author: ") << shortcutsProfilesIterator.value().author << QLatin1Char('\n');
			stream << QLatin1String("; Version: ") << shortcutsProfilesIterator.value().version << QLatin1String("\n\n");

			QHash<int, QVector<QKeySequence> >::iterator shortcutsIterator;

			for (shortcutsIterator = shortcutsProfilesIterator.value().shortcuts.begin(); shortcutsIterator != shortcutsProfilesIterator.value().shortcuts.end(); ++shortcutsIterator)
			{
				if (!shortcutsIterator.value().isEmpty())
				{
					QStringList shortcuts;

					for (int i = 0; i < shortcutsIterator.value().count(); ++i)
					{
						shortcuts.append(shortcutsIterator.value().at(i).toString());
					}

					stream << QLatin1Char('[') << ActionsManager::getActionName(shortcutsIterator.key()) << QLatin1String("]\n");
					stream << QLatin1String("shortcuts=") << shortcuts.join(QLatin1Char(' ')) << QLatin1String("\n\n");
				}
			}

			file.close();
		}

		QStringList shortcutsProfiles;

		for (int i = 0; i < m_ui->actionShortcutsViewWidget->getRowCount(); ++i)
		{
			const QString identifier = m_ui->actionShortcutsViewWidget->getIndex(i, 0).data(Qt::UserRole).toString();

			if (!identifier.isEmpty())
			{
				shortcutsProfiles.append(identifier);
			}
		}

		SettingsManager::setValue(QLatin1String("Browser/KeyboardShortcutsProfilesOrder"), shortcutsProfiles);

		ActionsManager::loadShortcuts();

		SettingsManager::setValue(QLatin1String("Browser/EnableTrayIcon"), m_ui->enableTrayIconCheckBox->isChecked());

		if (!m_javaScriptOptions.isEmpty())
		{
			SettingsManager::setValue(QLatin1String("Browser/JavaScriptCanShowStatusMessages"), m_javaScriptOptions.value(QLatin1String("Browser/JavaScriptCanShowStatusMessages")));
			SettingsManager::setValue(QLatin1String("Browser/JavaScriptCanAccessClipboard"), m_javaScriptOptions.value(QLatin1String("Browser/JavaScriptCanAccessClipboard")));
			SettingsManager::setValue(QLatin1String("Browser/JavaScriptCanDisableContextMenu"), m_javaScriptOptions.value(QLatin1String("Browser/JavaScriptCanDisableContextMenu")));
			SettingsManager::setValue(QLatin1String("Browser/JavaScriptCanOpenWindows"), m_javaScriptOptions.value(QLatin1String("Browser/JavaScriptCanOpenWindows")));
			SettingsManager::setValue(QLatin1String("Browser/JavaScriptCanCloseWindows"), m_javaScriptOptions.value(QLatin1String("Browser/JavaScriptCanCloseWindows")));
		}

		updateReaddKeyboardProfileMenu();
	}

	if (sender() == m_ui->buttonBox)
	{
		close();
	}
	else
	{
		m_ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
	}
}

QString PreferencesDialog::createProfileIdentifier(ItemViewWidget *view, const QString &base) const
{
	QStringList identifiers;

	for (int i = 0; i < view->getRowCount(); ++i)
	{
		const QString identifier = view->getIndex(i, 0).data(Qt::UserRole).toString();

		if (!identifier.isEmpty())
		{
			identifiers.append(identifier);
		}
	}

	return Utils::createIdentifier(base, identifiers);
}

ShortcutsProfile PreferencesDialog::loadKeyboardProfile(const QString &identifier, bool loadShortcuts) const
{
	const QString path = SessionsManager::getReadableDataPath(QLatin1String("keyboard/") + identifier + QLatin1String(".ini"));
	QFile file(path);

	if (!file.open(QIODevice::ReadOnly))
	{
		return ShortcutsProfile();
	}

	const QRegularExpression expression(QLatin1String(";\\s*"));
	ShortcutsProfile profile;
	profile.identifier = identifier;

	while (!file.atEnd())
	{
		const QString line = QString(file.readLine()).trimmed();

		if (!line.startsWith(QLatin1Char(';')))
		{
			break;
		}

		const QString key = line.section(QLatin1Char(':'), 0, 0).remove(expression);
		const QString value = line.section(QLatin1Char(':'), 1).trimmed();

		if (key == QLatin1String("Title"))
		{
			profile.title = value;
		}
		else if (key == QLatin1String("Description"))
		{
			profile.description = value;
		}
		else if (key == QLatin1String("Author"))
		{
			profile.author = value;
		}
		else if (key == QLatin1String("Version"))
		{
			profile.version = value;
		}
	}

	file.close();

	if (!loadShortcuts)
	{
		return profile;
	}

	QSettings settings(path, QSettings::IniFormat);
	settings.setIniCodec("UTF-8");

	const QStringList actions = settings.childGroups();

	for (int i = 0; i < actions.count(); ++i)
	{
		const int action = ActionsManager::getActionIdentifier(actions.at(i));

		if (action < 0)
		{
			continue;
		}

		settings.beginGroup(actions.at(i));

		const QStringList rawShortcuts = settings.value(QLatin1String("shortcuts")).toString().split(QLatin1Char(' '), QString::SkipEmptyParts);
		QVector<QKeySequence> shortcuts;
		shortcuts.reserve(rawShortcuts.count());

		for (int j = 0; j < rawShortcuts.count(); ++j)
		{
			const QKeySequence shortcut(QKeySequence(rawShortcuts.at(j)));

			if (!shortcut.isEmpty())
			{
				shortcuts.append(shortcut);
			}
		}

		if (!shortcuts.isEmpty())
		{
			profile.shortcuts[action] = shortcuts;
		}

		settings.endGroup();
	}

	return profile;
}

}
