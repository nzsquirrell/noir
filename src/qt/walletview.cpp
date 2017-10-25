// Copyright (c) 2011-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "walletview.h"
#include "bitcoingui.h"
#include "transactiontablemodel.h"
#include "addressbookpage.h"
#include "sendcoinsdialog.h"
#include "signverifymessagedialog.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "transactionview.h"
#include "overviewpage.h"
#include "learnmorepage.h"
#include "communitypage.h"
#include "askpassphrasedialog.h"
#include "ui_interface.h"
#include "menupage.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QAction>
#include <QStatusBar>
#include <QtWidgets>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>

#if QT_VERSION < 0x050000
#include <QDesktopServices>
#else
#include <QStandardPaths>
#endif
#include <QFileDialog>
#include <QPushButton>

using namespace std;

WalletView::WalletView(QWidget *parent, BitcoinGUI *_gui):
    QStackedWidget(parent),
    gui(_gui),
    clientModel(0),
    walletModel(0)
{
    // Create tabs
    overviewPage = new OverviewPage();

    transactionsPage = new QWidget(this);
    QVBoxLayout *vbox = new QVBoxLayout();
    QHBoxLayout *hbox_buttons = new QHBoxLayout();
    transactionView = new TransactionView(this);
    vbox->addWidget(transactionView);
    QPushButton *exportButton = new QPushButton(tr("&Export"), this);
    exportButton->setToolTip(tr("Export the data in the current tab to a file"));
#ifndef Q_OS_MAC // Icons on push buttons are very uncommon on Mac
    exportButton->setIcon(QIcon(":/icons/export"));
#endif
    hbox_buttons->addStretch();
    hbox_buttons->addWidget(exportButton);
    vbox->addLayout(hbox_buttons);

    transactionsPage->setLayout(vbox);

    addressBookPage = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::SendingTab);

    receiveCoinsPage = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::ReceivingTab);

    zerocoinPage = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::ZerocoinTab);

    sendCoinsPage = new SendCoinsDialog(gui);

    communityPage = new CommunityPage();

    learnMorePage = new LearnMorePage();

    signVerifyMessageDialog = new SignVerifyMessageDialog(gui);

    addWidget(learnMorePage);
    addWidget(communityPage);
    addWidget(overviewPage);
    addWidget(transactionsPage);
    addWidget(addressBookPage);
    addWidget(receiveCoinsPage);
    addWidget(sendCoinsPage);
    addWidget(zerocoinPage);


    // Clicking on a transaction on the overview page simply sends you to transaction history page
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), this, SLOT(gotoHistoryPage()));
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), transactionView, SLOT(focusTransaction(QModelIndex)));

    // Double-clicking on a transaction on the transaction history page shows details
    connect(transactionView, SIGNAL(doubleClicked(QModelIndex)), transactionView, SLOT(showDetails()));

    // Clicking on "Send Coins" in the address book sends you to the send coins tab
    connect(addressBookPage, SIGNAL(sendCoins(QString)), this, SLOT(gotoSendCoinsPage(QString)));
    // Clicking on "Verify Message" in the address book opens the verify message tab in the Sign/Verify Message dialog
    connect(addressBookPage, SIGNAL(verifyMessage(QString)), this, SLOT(gotoVerifyMessageTab(QString)));
    // Clicking on "Sign Message" in the receive coins page opens the sign message tab in the Sign/Verify Message dialog
    connect(receiveCoinsPage, SIGNAL(signMessage(QString)), this, SLOT(gotoSignMessageTab(QString)));
    // Clicking on "Export" allows to export the transaction list
    connect(exportButton, SIGNAL(clicked()), transactionView, SLOT(exportClicked()));
    fetchPrice();
    gotoOverviewPage();

    /* Create timer to fetch price every minute or as needed */
    timerId = startTimer(60000);
}

WalletView::~WalletView()
{
    killTimer(timerId);
}

void WalletView::timerEvent(QTimerEvent *event)
{
    fetchPrice();
}

void WalletView::setBitcoinGUI(BitcoinGUI *gui)
{
    this->gui = gui;
    //Set status bar into subscreens
    gui->progressBar->setStyleSheet("QProgressBar { border: 0px solid grey; border-radius: 0px;background-color: #d8d8d8;} "
                                    "QProgressBar::chunk { background: QLinearGradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #121548, stop: 1 #4a0e95); width: 5px; }");
    gui->progressBar->setTextVisible(false);
    gui->progressBar->setFixedSize(300,10);
    gui->progressBarLabel->setStyleSheet("color: rgb(158,158,158)");
    overviewPage->statusText->addWidget(gui->progressBarLabel);
    overviewPage->statusBar->addWidget(gui->progressBar);
    //overviewPage->statusBar->addWidget(gui->frameBlocks);
}

void WalletView::setClientModel(ClientModel *clientModel)
{
    this->clientModel = clientModel;
    if (clientModel)
    {
        overviewPage->setClientModel(clientModel);
        addressBookPage->setOptionsModel(clientModel->getOptionsModel());
        receiveCoinsPage->setOptionsModel(clientModel->getOptionsModel());
        zerocoinPage->setOptionsModel(clientModel->getOptionsModel());
    }
}

void WalletView::setWalletModel(WalletModel *walletModel)
{
    this->walletModel = walletModel;
    if (walletModel)
    {
        // Receive and report messages from wallet thread
        connect(walletModel, SIGNAL(message(QString,QString,unsigned int)), gui, SLOT(message(QString,QString,unsigned int)));

        // Put transaction list in tabs
        transactionView->setModel(walletModel);
        overviewPage->setWalletModel(walletModel);
        addressBookPage->setModel(walletModel->getAddressTableModel());
        receiveCoinsPage->setModel(walletModel->getAddressTableModel());
        zerocoinPage->setModel(walletModel->getAddressTableModel());
        sendCoinsPage->setModel(walletModel);
        signVerifyMessageDialog->setModel(walletModel);

        setEncryptionStatus();
        connect(walletModel, SIGNAL(encryptionStatusChanged(int)), gui, SLOT(setEncryptionStatus(int)));

        // Balloon pop-up for new transaction
        connect(walletModel->getTransactionTableModel(), SIGNAL(rowsInserted(QModelIndex,int,int)),
                this, SLOT(incomingTransaction(QModelIndex,int,int)));

        // Ask for passphrase if needed
        connect(walletModel, SIGNAL(requireUnlock()), this, SLOT(unlockWallet()));
    }
}

void WalletView::incomingTransaction(const QModelIndex& parent, int start, int /*end*/)
{
    // Prevent balloon-spam when initial block download is in progress
    if(!walletModel || !clientModel || clientModel->inInitialBlockDownload())
        return;

    TransactionTableModel *ttm = walletModel->getTransactionTableModel();

    QString date = ttm->index(start, TransactionTableModel::Date, parent).data().toString();
    qint64 amount = ttm->index(start, TransactionTableModel::Amount, parent).data(Qt::EditRole).toULongLong();
    QString type = ttm->index(start, TransactionTableModel::Type, parent).data().toString();
    QString address = ttm->index(start, TransactionTableModel::ToAddress, parent).data().toString();

    gui->incomingTransaction(date, walletModel->getOptionsModel()->getDisplayUnit(), amount, type, address);
}

void WalletView::gotoOverviewPage()
{
    gui->getOverviewAction()->setChecked(true);
    setCurrentWidget(overviewPage);
    overviewPage->statusText->addWidget(gui->progressBarLabel);
    overviewPage->statusBar->addWidget(gui->progressBar);
}

void WalletView::gotoCommunityPage()
{
    //gui->getOverviewAction()->setChecked(true);
    setCurrentWidget(communityPage);
}
void WalletView::gotoHistoryPage()
{
    //transactionsPage->statusText->addWidget(gui->progressBarLabel);
    //transactionsPage->statusBar->addWidget(gui->progressBar);
    gui->getHistoryAction()->setChecked(true);
    setCurrentWidget(transactionsPage);
}

void WalletView::gotoAddressBookPage()
{
    //addressBookPage->statusText->addWidget(gui->progressBarLabel);
    //addressBookPage->statusBar->addWidget(gui->progressBar);
    gui->getAddressBookAction()->setChecked(true);
    setCurrentWidget(addressBookPage);
}

void WalletView::gotoReceiveCoinsPage()
{
    //receiveCoinsPage->statusText->addWidget(gui->progressBarLabel);
    //receiveCoinsPage->statusBar->addWidget(gui->progressBar);
    gui->getReceiveCoinsAction()->setChecked(true);
    setCurrentWidget(receiveCoinsPage);
}

void WalletView::gotoZerocoinPage()
{
    //zerocoinPage->statusText->addWidget(gui->progressBarLabel);
    //zerocoinPage->statusBar->addWidget(gui->progressBar);
    gui->getZerocoinAction()->setChecked(true);
    setCurrentWidget(zerocoinPage);
}

void WalletView::gotoLearnMorePage()
{
    //gui->getOverviewAction()->setChecked(true);
    setCurrentWidget(learnMorePage);
}


void WalletView::gotoSendCoinsPage(QString addr)
{
    sendCoinsPage->statusText->addWidget(gui->progressBarLabel);
    sendCoinsPage->statusBar->addWidget(gui->progressBar);
    gui->getSendCoinsAction()->setChecked(true);
    setCurrentWidget(sendCoinsPage);

    if (!addr.isEmpty())
        sendCoinsPage->setAddress(addr);
}

void WalletView::gotoSignMessageTab(QString addr)
{
    // call show() in showTab_SM()
    signVerifyMessageDialog->showTab_SM(true);

    if (!addr.isEmpty())
        signVerifyMessageDialog->setAddress_SM(addr);
}

void WalletView::gotoVerifyMessageTab(QString addr)
{
    // call show() in showTab_VM()
    signVerifyMessageDialog->showTab_VM(true);

    if (!addr.isEmpty())
        signVerifyMessageDialog->setAddress_VM(addr);
}

bool WalletView::handleURI(const QString& strURI)
{
    // URI has to be valid
    if (sendCoinsPage->handleURI(strURI))
    {
        gotoSendCoinsPage();
        emit showNormalIfMinimized();
        return true;
    }
    else
        return false;
}

void WalletView::showOutOfSyncWarning(bool fShow)
{
    overviewPage->showOutOfSyncWarning(fShow);
}

void WalletView::setEncryptionStatus()
{
    gui->setEncryptionStatus(walletModel->getEncryptionStatus());
}

void WalletView::encryptWallet(bool status)
{
    if(!walletModel)
        return;
    AskPassphraseDialog dlg(status ? AskPassphraseDialog::Encrypt : AskPassphraseDialog::Decrypt, this);
    dlg.setModel(walletModel);
    dlg.exec();

    setEncryptionStatus();
}

void WalletView::backupWallet()
{
#if QT_VERSION < 0x050000
    QString saveDir = QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation);
#else
    QString saveDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#endif
    QString filename = QFileDialog::getSaveFileName(this, tr("Backup Wallet"), saveDir, tr("Wallet Data (*.dat)"));
    if (!filename.isEmpty()) {
        if (!walletModel->backupWallet(filename)) {
            gui->message(tr("Backup Failed"), tr("There was an error trying to save the wallet data to the new location."),
                      CClientUIInterface::MSG_ERROR);
        }
        else
            gui->message(tr("Backup Successful"), tr("The wallet data was successfully saved to the new location."),
                      CClientUIInterface::MSG_INFORMATION);
    }
}

void WalletView::changePassphrase()
{
    AskPassphraseDialog dlg(AskPassphraseDialog::ChangePass, this);
    dlg.setModel(walletModel);
    dlg.exec();
}

void WalletView::unlockWallet()
{
    if(!walletModel)
        return;
    // Unlock wallet when requested by wallet model
    if (walletModel->getEncryptionStatus() == WalletModel::Locked)
    {
        AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, this);
        dlg.setModel(walletModel);
        dlg.exec();
    }
}

void WalletView::fetchPrice()
{

    QNetworkAccessManager *nam = new QNetworkAccessManager(this);
    connect(nam, SIGNAL(finished(QNetworkReply*)), this, SLOT(replyFinished(QNetworkReply*)));
    nam->get(QNetworkRequest(QUrl("https://api.coinmarketcap.com/v1/ticker/zoin/?convert=USD")));

}


void WalletView::replyFinished(QNetworkReply *reply)
{
    QByteArray bytes = reply->readAll();
    QString str = QString::fromUtf8(bytes.data(), bytes.size());
    //qDebug() << str;
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    //qDebug() << QVariant(statusCode).toString();
    size_t s = str.toStdString().find("\"price_usd\": \"");
    size_t e = str.toStdString().find("\",", s);
    string priceUSD = str.toStdString().substr(s + 14, e - s - 14);
    QString priceUSDq = QString::fromStdString(priceUSD);
    qDebug()<< priceUSDq;
    s = str.toStdString().find("\"price_btc\": \"");
    e = str.toStdString().find("\",", s);
    string priceBTC = str.toStdString().substr(s + 14, e - s - 14);
    QString priceBTCq = QString::fromStdString(priceBTC);
    qDebug()<< priceBTCq;
    string newPriceUSD = "$";
    newPriceUSD.append(priceUSD);
    try{
        if(stod(priceUSD) && stod(priceBTC)){
            priceBTC.append(" BTC");
            overviewPage->priceUSD->setText(QString::fromStdString(newPriceUSD));
            overviewPage->priceBTC->setText(QString::fromStdString(priceBTC));
            overviewPage->labelBalanceUSD->setText(QString::number(priceUSDq.toDouble() * overviewPage->labelBalance->text().toDouble(), 'f', 2) + " USD");
            overviewPage->labelUnconfirmedUSD->setText(QString::number(priceUSDq.toDouble() * overviewPage->labelUnconfirmed->text().toDouble(), 'f', 2) + " USD");
            sendCoinsPage->priceUSD->setText(QString::fromStdString(newPriceUSD));
            sendCoinsPage->priceBTC->setText(QString::fromStdString(priceBTC));
        }
    }
    catch(...){}

}
