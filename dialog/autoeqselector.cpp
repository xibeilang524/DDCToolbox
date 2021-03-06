#include "autoeqselector.h"
#include "ui_autoeqselector.h"

#include "utils/autoeqclient.h"
#include "overlaymsgproxy.h"

#include "item/detaillistitem.h"
#include "item/detaillistitem_delegate.h"

#include <QListWidget>
#include <QMessageBox>
#include <QSslSocket>

AutoEQSelector::AutoEQSelector(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AutoEQSelector)
{
    ui->setupUi(this);
    setFixedSize(geometry().width(),geometry().height());
    ui->listWidget->setItemDelegate(new ItemSizeDelegate);

    waitScreen = new OverlayMsgProxy(this);

    imgSizeCache = ui->picture->size() * 11.5;

    DetailListItem *item = new DetailListItem();
    item->setData("No query","Enter your headphone model above and hit 'Search'");
    ui->listWidget->addItem(new QListWidgetItem());
    ui->listWidget->setItemWidget(ui->listWidget->item(ui->listWidget->count()-1),item);

    ui->buttonBox->button(QDialogButtonBox::StandardButton::Ok)->setEnabled(false);

    connect(ui->listWidget,&QListWidget::itemSelectionChanged,this,&AutoEQSelector::updateDetails);
    connect(ui->search,&QAbstractButton::clicked,this,&AutoEQSelector::doQuery);
}

AutoEQSelector::~AutoEQSelector()
{
    delete ui;
}

void AutoEQSelector::showEvent(QShowEvent *event){
    if(!QSslSocket::supportsSsl()){
        QString actual = QSslSocket::sslLibraryVersionString();
        QMessageBox::warning(this, "OpenSSL Error", "Failed to initialize SSL module.\n"
                                                    "Therefore this feature is currently unavailable on your machine.\n"
                                                    "Please make sure all OpenSSL DLLs are either in PATH or in the working directory of this app.\n\n"
                                                    "Required OpenSSL version: " + QSslSocket::sslLibraryBuildVersionString() + "\n"
                                                    "Loaded OpenSSL version: " + (actual.isEmpty() ? "None" : actual));
    }
    QWidget::showEvent(event);
}

void AutoEQSelector::appendToList(QueryResult result){
    DetailListItem *item = new DetailListItem();
    item->setData(result.getModel(),result.getGroup());
    QListWidgetItem* li = new QListWidgetItem();
    li->setData(Qt::UserRole,QVariant::fromValue<QueryResult>(result));
    ui->listWidget->addItem(li);
    ui->listWidget->setItemWidget(ui->listWidget->item(ui->listWidget->count()-1),item);
}

void AutoEQSelector::updateDetails(){
    if(ui->listWidget->selectedItems().count() < 1)
        return;

    auto selection = ui->listWidget->selectedItems().first();
    if(!selection->data(Qt::UserRole).canConvert<QueryResult>())
        return;

    ui->buttonBox->button(QDialogButtonBox::StandardButton::Ok)->setEnabled(false);


    QueryResult result = selection->data(Qt::UserRole).value<QueryResult>();
    ui->title->setText(result.getModel());
    ui->group->setText(result.getGroup());

    waitScreen = new OverlayMsgProxy(this);
    waitScreen->openBase("Please wait...","Fetching headphone details from GitHub");

    HeadphoneMeasurement hp = AutoEQClient::fetchDetails(result);
    hpCache = hp;
    if(hp.getParametricEQ() == "" && hp.getGraphUrl() == "")
        QMessageBox::warning(this,"Error","API request failed.\n\nEither your network connection is experiencing issues, or you are being rate-limited by GitHub.\nKeep in mind that you can only send 60 web requests per hour to this API.\n\nYou can check your current rate limit status here: https://api.github.com/rate_limit");
    else
        ui->picture->setPixmap(hp.getGraphImage().scaled(imgSizeCache,
                                                         Qt::AspectRatioMode::KeepAspectRatio,
                                                         Qt::TransformationMode::SmoothTransformation));
    ui->buttonBox->button(QDialogButtonBox::StandardButton::Ok)->setEnabled(true);

    waitScreen->hide();
}

void AutoEQSelector::doQuery(){
    ui->listWidget->setEnabled(false);
    ui->search->setEnabled(false);

    ui->listWidget->clear();
    DetailListItem *item = new DetailListItem();
    item->setData("Please wait...","Searching the database for your headphone model");
    ui->listWidget->addItem(new QListWidgetItem());
    ui->listWidget->setItemWidget(ui->listWidget->item(ui->listWidget->count()-1),item);

    QueryRequest req(ui->searchInput->text());
    QVector<QueryResult> ress = AutoEQClient::query(req);
    ui->listWidget->clear();
    for(auto res : ress)
        appendToList(res);

    ui->listWidget->setEnabled(true);
    ui->search->setEnabled(true);
}

HeadphoneMeasurement AutoEQSelector::getSelection(){
    if(ui->listWidget->selectedItems().count() < 1)
        return HeadphoneMeasurement();

    auto selection = ui->listWidget->selectedItems().first();
    if(!selection->data(Qt::UserRole).canConvert<QueryResult>())
        return HeadphoneMeasurement();

    return hpCache;
}
