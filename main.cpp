#include <QApplication>
#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeView>
#include <QFileSystemModel>
#include <QFileDialog>
#include <QDir>
#include <QListWidget>
#include <QSettings>
#include <vlc/vlc.h>
#include <iostream>
#include <QLabel>
#include <QSlider>
#include <QTimer>
#include <QPixmap>
#include <taglib/mpegfile.h>
#include <taglib/flacfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/fileref.h>
#include <QGraphicsOpacityEffect>
#include <QMouseEvent>
#include <QLineEdit>
#include <QSortFilterProxyModel>

// Your original extract function
QPixmap extractAlbumArt(const QString &filepath) {
    TagLib::FileRef f(filepath.toUtf8().constData());
    if (!f.isNull()) {
        if (filepath.endsWith(".flac", Qt::CaseInsensitive)) {
            TagLib::FLAC::File flacFile(filepath.toUtf8().constData());
            if (flacFile.isValid() && flacFile.pictureList().size() > 0) {
                TagLib::FLAC::Picture *pic = flacFile.pictureList().front();
                QByteArray data(pic->data().data(), pic->data().size());
                QPixmap pixmap;
                if (pixmap.loadFromData(data)) return pixmap;
            }
        }
        if (filepath.endsWith(".mp3", Qt::CaseInsensitive)) {
            TagLib::MPEG::File mp3File(filepath.toUtf8().constData());
            if (mp3File.isValid() && mp3File.ID3v2Tag()) {
                TagLib::ID3v2::FrameList frames = mp3File.ID3v2Tag()->frameListMap()["APIC"];
                if (!frames.isEmpty()) {
                    TagLib::ID3v2::AttachedPictureFrame *frame = static_cast<TagLib::ID3v2::AttachedPictureFrame*>(frames.front());
                    QByteArray data(frame->picture().data(), frame->picture().size());
                    QPixmap pixmap;
                    if (pixmap.loadFromData(data)) return pixmap;
                }
            }
        }
    }
    QFileInfo info(filepath);
    QDir dir = info.dir();
    QStringList imageFilters = {"*.jpg", "*.jpeg", "*.png", "*.bmp", "*.webp"};
    QFileInfoList images = dir.entryInfoList(imageFilters, QDir::Files | QDir::Readable);
    if (!images.isEmpty()) {
        QStringList preferredNames = {"cover", "folder", "album", "front"};
        for (const QString &name : preferredNames) {
            for (const QFileInfo &img : images) {
                if (img.baseName().toLower().contains(name)) {
                    QPixmap pix;
                    if (pix.load(img.absoluteFilePath())) return pix;
                }
            }
        }
        QFileInfo best = images.first();
        for (const QFileInfo &img : images) {
            if (img.size() > best.size()) best = img;
        }
        QPixmap pix;
        if (pix.load(best.absoluteFilePath())) return pix;
    }
    return QPixmap();
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    libvlc_instance_t *vlc = libvlc_new(0, nullptr);
    libvlc_media_player_t *player = nullptr;
    if (vlc) player = libvlc_media_player_new(vlc);

    QString currentsong;
    int currentIndex = -1;

    QWidget window;
    window.setWindowTitle("mp");
    window.resize(800, 600);
    window.setStyleSheet("background-color: #0B121A; color: white;"); 

    QPushButton *pickFolderButton = new QPushButton("Select Folder");
    QPushButton *playButton = new QPushButton("|| / ->");
    QListWidget *queueView = new QListWidget;
    QTreeView *tree = new QTreeView;
    QFileSystemModel *model = new QFileSystemModel;
    QLabel *timeLabel = new QLabel("0:00 / 0:00");
    QSlider *progressBar = new QSlider(Qt::Horizontal);
    QLabel *volumeLabel = new QLabel("Volume");
    QSlider *volumeSlider = new QSlider(Qt::Horizontal);

    volumeSlider->setRange(0, 100);
    volumeSlider->setValue(50);
    
    // RESTORED: Your original slider styling
    QString originalSliderStyle = R"(
    QSlider::groove:horizontal {
        height: 6px;
        margin: 0px;
        background: #444;
    }
    QSlider::handle:horizontal {
        width: 14px;
        height: 28px;
        margin: -12px 0;   /* centers handle */
        background: #aaa;
        border-radius: 7px;
    }
    )";
    progressBar->setStyleSheet(originalSliderStyle);
    volumeSlider->setStyleSheet(originalSliderStyle);
    progressBar->setRange(0, 100);

    QTimer *updateTimer = new QTimer(&window);
    model->setFilter(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    model->setNameFilters(QStringList() << "*.mp3" << "*.flac" << "*.wav");
    
    QSortFilterProxyModel *proxyModel = new QSortFilterProxyModel;
    proxyModel->setSourceModel(model);
    proxyModel->setRecursiveFilteringEnabled(true);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    tree->setModel(proxyModel);
    tree->setHeaderHidden(true);
    tree->hideColumn(1); tree->hideColumn(2); tree->hideColumn(3);
    tree->setExpandsOnDoubleClick(false);
    tree->setVisible(false);

    QLineEdit *searchbox = new QLineEdit;
    searchbox->setPlaceholderText("Search");
    searchbox->setClearButtonEnabled(true);

    QSettings settings("mp", "mp");
    QString lastFolder = settings.value("folder", "").toString();
    if (!lastFolder.isEmpty() && QDir(lastFolder).exists()) {
        model->setRootPath(lastFolder);
        tree->setRootIndex(proxyModel->mapFromSource(model->index(lastFolder)));
        tree->setVisible(true);
    }

    QLabel *albumArt = new QLabel;
    albumArt->setFixedSize(200, 200);
    albumArt->setScaledContents(true);
    albumArt->setAlignment(Qt::AlignCenter);
    albumArt->setStyleSheet("border: 1px solid gray;");

    QHBoxLayout *artsection = new QHBoxLayout;
    artsection->addStretch();
    artsection->addWidget(albumArt);
    artsection->addStretch();

    QVBoxLayout *progressLayout = new QVBoxLayout;
    progressLayout->setSpacing(0);
    progressLayout->setContentsMargins(0, 0, 0, 0);
    progressLayout->addWidget(progressBar);
    progressLayout->addWidget(timeLabel);

    QVBoxLayout *volumeLayout = new QVBoxLayout;
    volumeLayout->setSpacing(0);
    volumeLayout->setContentsMargins(0, 0, 0, 0);
    volumeLayout->addWidget(volumeSlider);
    volumeLayout->addWidget(volumeLabel);

    QVBoxLayout *leftLayout = new QVBoxLayout;
    leftLayout->setSpacing(15);
    leftLayout->addWidget(pickFolderButton);
    leftLayout->addWidget(playButton);
    leftLayout->addSpacing(50);
    leftLayout->addLayout(artsection);
    leftLayout->addSpacing(50);
    leftLayout->addLayout(progressLayout);
    leftLayout->addLayout(volumeLayout);

    progressBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    volumeSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    pickFolderButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    playButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QVBoxLayout *rightLayout = new QVBoxLayout;
    rightLayout->addWidget(searchbox);
    rightLayout->addWidget(tree, 2);
    rightLayout->addWidget(queueView, 1);

    QHBoxLayout *contentLayout = new QHBoxLayout;
    contentLayout->addLayout(leftLayout, 1);
    contentLayout->addLayout(rightLayout, 2);
    QVBoxLayout *mainLayout = new QVBoxLayout(&window);
    mainLayout->addLayout(contentLayout);

    // Play function
    auto playIndex = [&](int i) {
        if (!player || !vlc || i < 0 || i >= queueView->count()) return;
        QListWidgetItem *item = queueView->item(i);
        QString path = item->data(Qt::UserRole).toString();
        if (!QFileInfo(path).isFile()) return;

        currentIndex = i;
        currentsong = path;
        libvlc_media_player_stop(player);
        libvlc_media_t *media = libvlc_media_new_path(vlc, path.toUtf8().constData());
        if (!media) return;
        libvlc_media_player_set_media(player, media);
        libvlc_media_release(media);
        libvlc_media_player_play(player);

        QPixmap art = extractAlbumArt(path);
        if (!art.isNull()) albumArt->setPixmap(art.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        else { albumArt->clear(); albumArt->setText("No Album Art"); }

        for (int n = 0; n < queueView->count(); n++) queueView->item(n)->setBackground(Qt::NoBrush);
        item->setBackground(QColor(255, 255, 255, 25));
    };

    QObject::connect(pickFolderButton, &QPushButton::clicked, [&]() {
        QString folder = QFileDialog::getExistingDirectory(&window, "Select Music Folder");
        if (!folder.isEmpty()) {
            settings.setValue("folder", folder);
            model->setRootPath(folder);
            tree->setRootIndex(proxyModel->mapFromSource(model->index(folder)));
            tree->setVisible(true);
        }
    });

    QObject::connect(playButton, &QPushButton::clicked, [&]() {
        if (!player) return;
        if (currentIndex < 0 && queueView->count() > 0) playIndex(0);
        else if (libvlc_media_player_is_playing(player)) libvlc_media_player_set_pause(player, 1);
        else libvlc_media_player_play(player);
    });

    QObject::connect(tree, &QTreeView::doubleClicked, [&](const QModelIndex &index) {
        QString path = model->filePath(proxyModel->mapToSource(index));
        if (QFileInfo(path).isFile()) {
            QListWidgetItem *item = new QListWidgetItem(QFileInfo(path).fileName());
            item->setData(Qt::UserRole, path);
            queueView->addItem(item);
            if (queueView->count() == 1) playIndex(0);
        }
    });

    QObject::connect(queueView, &QListWidget::itemDoubleClicked, [&](QListWidgetItem *item) {
        playIndex(queueView->row(item));

    });
    queueView->setContextMenuPolicy(Qt::CustomContextMenu);
    
    QObject::connect(queueView, &QListWidget::customContextMenuRequested, [&](const QPoint &pos) {
        QListWidgetItem *item = queueView->itemAt(pos);
        if (item) {
            int row = queueView->row(item);
            if (row == currentIndex) {
                libvlc_media_player_stop(player);
                currentIndex = -1;
                albumArt->clear();
                albumArt->setText("No Album Art");
            }
            else if (row < currentIndex) currentIndex--;
            delete queueView->takeItem(row);
        }
    });
    bool isDragging = false;
    QObject::connect(progressBar, &QSlider::sliderPressed, [&]() { isDragging = true; });
    QObject::connect(progressBar, &QSlider::sliderReleased, [&]() {
        if (player) {
            isDragging = false;
            libvlc_time_t total = libvlc_media_player_get_length(player);
            if (total > 0) libvlc_media_player_set_time(player, (progressBar->value() * total) / 100);
        }
    });

    QObject::connect(updateTimer, &QTimer::timeout, [&]() {
        if (!player) return;
        
        // NEW: Auto-advance queue logic
        if (libvlc_media_player_get_state(player) == libvlc_Ended) {
            if (currentIndex >= 0 && currentIndex < queueView->count()){
                delete queueView->takeItem(currentIndex);
                if(currentIndex < queueView->count())
                    playIndex(currentIndex);
                else{
                    currentIndex = -1;
                    albumArt->clear();
                    albumArt->setText("No Album Art");
                }
            }
            return;
        }

        if (isDragging) return;
        libvlc_time_t current = libvlc_media_player_get_time(player);
        libvlc_time_t total = libvlc_media_player_get_length(player);
        if (total > 0) {
            progressBar->setValue((current * 100) / total);
            int curS = current / 1000, totS = total / 1000;
            timeLabel->setText(QString("%1:%2 / %3:%4").arg(curS / 60).arg(curS % 60, 2, 10, QChar('0')).arg(totS / 60).arg(totS % 60, 2, 10, QChar('0')));
        }
    });

    QObject::connect(volumeSlider, &QSlider::valueChanged, [&](int value) {
        if (player) libvlc_audio_set_volume(player, value);
    });

    QObject::connect(searchbox, &QLineEdit::textChanged, [&](const QString &text) {
        proxyModel->setFilterFixedString(text);
    });

    updateTimer->start(100);
    window.show();
    return app.exec();
}
