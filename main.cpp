#include<QApplication>
#include<QWidget>
#include<QPushButton>
#include<QVBoxLayout>
#include<QHBoxLayout>
#include<QTreeView>
#include<QFileSystemModel>
#include<QFileDialog>
#include<QDir>
#include<QListWidget>
#include<QSettings>
#include<vlc/vlc.h>
#include<iostream>
#include<QLabel>
#include<QSlider>
#include<QTimer>
#include<QPixmap>
#include<taglib/mpegfile.h>
#include<taglib/flacfile.h>
#include<taglib/id3v2tag.h>
#include<taglib/attachedpictureframe.h>
#include<taglib/fileref.h>
#include<QGraphicsOpacityEffect>
#include<QMouseEvent>

QPixmap extractAlbumArt(const QString &filepath) {
    // First try: Extract embedded album art
    TagLib::FileRef f(filepath.toUtf8().constData());
    
    if (!f.isNull()) {
        // Try FLAC
        if (filepath.endsWith(".flac", Qt::CaseInsensitive)) {
            TagLib::FLAC::File flacFile(filepath.toUtf8().constData());
            if (flacFile.isValid() && flacFile.pictureList().size() > 0) {
                TagLib::FLAC::Picture *pic = flacFile.pictureList().front();
                QByteArray data(pic->data().data(), pic->data().size());
                QPixmap pixmap;
                if (pixmap.loadFromData(data)) {
                    return pixmap;
                }
            }
        }
        
        // Try MP3
        if (filepath.endsWith(".mp3", Qt::CaseInsensitive)) {
            TagLib::MPEG::File mp3File(filepath.toUtf8().constData());
            if (mp3File.isValid() && mp3File.ID3v2Tag()) {
                TagLib::ID3v2::FrameList frames = mp3File.ID3v2Tag()->frameListMap()["APIC"];
                if (!frames.isEmpty()) {
                    TagLib::ID3v2::AttachedPictureFrame *frame = 
                        static_cast<TagLib::ID3v2::AttachedPictureFrame*>(frames.front());
                    QByteArray data(frame->picture().data(), frame->picture().size());
                    QPixmap pixmap;
                    if (pixmap.loadFromData(data)) {
                        return pixmap;
                    }
                }
            }
        }
    }
    
    // Second try: Look for image files in the same folder
    QFileInfo info(filepath);
    QDir dir = info.dir();

    QStringList imageFilters = {"*.jpg", "*.jpeg", "*.png", "*.bmp", "*.webp"};
    QFileInfoList images = dir.entryInfoList(imageFilters, QDir::Files | QDir::Readable);

    if (!images.isEmpty()) {
        // Prioritize common album art filenames
        QStringList preferredNames = {"cover", "folder", "album", "front"};
        
        for (const QString &name : preferredNames) {
            for (const QFileInfo &img : images) {
                if (img.baseName().toLower().contains(name)) {
                    QPixmap pix;
                    if (pix.load(img.absoluteFilePath())) {
                        return pix;
                    }
                }
            }
        }
        
        // If no preferred name found, pick the largest image
        QFileInfo best = images.first();
        for (const QFileInfo &img : images) {
            if (img.size() > best.size()) {
                best = img;
            }
        }
        
        QPixmap pix;
        if (pix.load(best.absoluteFilePath())) {
            return pix;
        }
    }
    
    return QPixmap();  // No album art found
}

int main(int argc, char *argv[]) 
{
    QApplication app(argc, argv);
    
    // Initialize VLC
    libvlc_instance_t *vlc = libvlc_new(0, nullptr);
    libvlc_media_player_t *player = nullptr;
    
    if (vlc) {
        player = libvlc_media_player_new(vlc);
        std::cerr << "VLC initialized OK" << std::endl;
    } else {
        std::cerr << "VLC failed to initialize" << std::endl;
    }
    
    QString currentsong;
    
    // Setup UI
    QWidget window;
    window.setWindowTitle("mp");
    window.resize(800, 600);
    window.setStyleSheet("background-color: #0B121A;"); 
   // window.setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    window.setStyleSheet("background-color: rgba(11,18,26);");



    QPushButton *pickFolderButton = new QPushButton("Select Folder");
    QPushButton *playButton = new QPushButton("|| / ->");
    QListWidget *queueView = new QListWidget;
    QTreeView *tree = new QTreeView;
    QFileSystemModel *model = new QFileSystemModel;
    
	
    QLabel *timeLabel = new QLabel("0:00 / 0:00");
    QSlider *progressBar = new QSlider(Qt::Horizontal);
    QLabel *volumeLabel = new QLabel("Volume");
    QSlider *volumeSlider = new QSlider(Qt::Horizontal);
    volumeSlider->setRange(0,100);
    volumeSlider->setValue(50);
    progressBar->setStyleSheet(R"(
    QSlider::groove:horizontal {
        height: 6px;
        margin: 0px;
        background: #444;
    }
    QSlider::handle:horizontal {
        width: 14px;
        height:28px;
        margin: -12px 0;   /* centers handle */
        background: #aaa;
        border-radius: 7px;
    }
    )");

    volumeSlider->setStyleSheet(progressBar->styleSheet()); 

    progressBar->setRange(0,100);

    QTimer *updateTimer = new QTimer(&window);

    model->setFilter(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    model->setNameFilters(QStringList() << "*.mp3" << "*.flac" << "*.wav");
    model->setNameFilterDisables(true);
    
    tree->setModel(model);
    tree->setHeaderHidden(true);
    tree->hideColumn(1);
    tree->hideColumn(2);
    tree->hideColumn(3);
    tree->setExpandsOnDoubleClick(false);
    tree->setVisible(false);
    
    QSettings settings("mp", "mp");
    QString lastFolder = settings.value("folder", "").toString();
    if (!lastFolder.isEmpty() && QDir(lastFolder).exists()) {
        model->setRootPath(lastFolder);
        tree->setRootIndex(model->index(lastFolder));
        tree->setVisible(true);
    }
    
    /* Layout
    QHBoxLayout *topBar = new QHBoxLayout;
    topBar->addWidget(pickFolderButton);
    topBar->addWidget(playButton);
    topBar->addStretch();
    topBar->addWidget(timeLabel);
    topBar->addWidget(progressBar);
    topBar->addWidget(volumeLabel);
    topBar->addWidget(volumeSlider);
    */
    QLabel *albumArt = new QLabel;
    albumArt->setFixedSize(200,200);
    albumArt->setScaledContents(true);
    albumArt->setAlignment(Qt::AlignCenter);
    albumArt->setStyleSheet("border: 1px solid gray;");
    
    QHBoxLayout *artsection = new QHBoxLayout;
    artsection->addStretch();
    artsection->addWidget(albumArt); 
    artsection->addStretch();

    QVBoxLayout *progressLayout = new QVBoxLayout;
    progressLayout->setSpacing(0);          // tight gap
    progressLayout->setContentsMargins(0,0,0,0);
    progressLayout->addWidget(progressBar);
    progressLayout->addWidget(timeLabel);

    QVBoxLayout *volumeLayout = new QVBoxLayout;
    volumeLayout->setSpacing(0);
    volumeLayout->setContentsMargins(0,0,0,0);
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
    rightLayout->addWidget(tree, 2);
    rightLayout->addWidget(queueView, 1);
   
    QHBoxLayout *contentLayout = new QHBoxLayout;
    contentLayout->addLayout(leftLayout, 1);
    contentLayout->addLayout(rightLayout, 2); 

    QVBoxLayout *mainLayout = new QVBoxLayout(&window);
    //mainLayout->addLayout(topBar);
    mainLayout->addLayout(contentLayout);

    // Pick folder
    QObject::connect(pickFolderButton, &QPushButton::clicked, [&]() {
        QString folder = QFileDialog::getExistingDirectory(&window, "Select Music Folder");
        if (!folder.isEmpty()) {
            settings.setValue("folder", folder);
            model->setRootPath(folder);
            tree->setRootIndex(model->index(folder));
            tree->setVisible(true);
        }
    });
    
    QObject::connect(tree, &QTreeView::doubleClicked, [&](const QModelIndex &index) {
        QString path = model->filePath(index);
        QFileInfo info(path);

        if (!info.isFile()) return;
        if (!player || !vlc) return;

        // Create item and store full path
        QListWidgetItem *item = new QListWidgetItem(info.fileName());
        item->setData(Qt::UserRole, path);  // store full path
        queueView->addItem(item);

        currentsong = path;

        libvlc_media_player_stop(player);
        libvlc_media_t *media = libvlc_media_new_path(vlc, path.toUtf8().constData());

        if (media) {
            libvlc_media_player_set_media(player, media);
            libvlc_media_release(media);
            libvlc_media_player_play(player);

            QPixmap art = extractAlbumArt(path);
            if (!art.isNull())
                albumArt->setPixmap(art.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            else {
            albumArt->clear();
            albumArt->setText("No Album Art");
            }
            std::cerr << "Playing: " << path.toStdString() << std::endl;
        }
    });
    QObject::connect(queueView, &QListWidget::itemClicked, [&](QListWidgetItem *item) {
        if (!player || !vlc || !item) return;

        // Retrieve the full path stored in the item
        QString path = item->data(Qt::UserRole).toString();
        QFileInfo info(path);
        if (!info.isFile()) return;

        currentsong = path;

        // Stop current song and play clicked one
        libvlc_media_player_stop(player);
        libvlc_media_t *media = libvlc_media_new_path(vlc, path.toUtf8().constData());
        if (media) {
            libvlc_media_player_set_media(player, media);
            libvlc_media_release(media);
            libvlc_media_player_play(player);
        }

        // Update album art
        QPixmap art = extractAlbumArt(path);
        if (!art.isNull())
            albumArt->setPixmap(art.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        else {
            albumArt->clear();
            albumArt->setText("No Album Art");
        }

        std::cerr << "Playing from queue: " << path.toStdString() << std::endl;
    });


    
    // Play/pause button
    QObject::connect(playButton, &QPushButton::clicked, [&]() {
        if (!player || currentsong.isEmpty()) return;
        
        if (libvlc_media_player_is_playing(player)) {
            libvlc_media_player_set_pause(player, 1);
        } else {
            libvlc_media_player_play(player);
        }
    });
    
    bool isDragging = false;

    QObject::connect(progressBar, &QSlider::sliderPressed, [&]() {
        isDragging = true;
    });

    QObject::connect(progressBar, &QSlider::sliderReleased, [&]() {
        if (!player) return;
        isDragging = false;

        int sliderValue = progressBar->value();
        libvlc_time_t total = libvlc_media_player_get_length(player);
        if (total > 0) {
            libvlc_media_player_set_time(player, (sliderValue * total) / 100);
        }
    });

    QObject::connect(updateTimer, &QTimer::timeout, [&]() {
        if(!player || isDragging) return; // 
	    libvlc_time_t current = libvlc_media_player_get_time(player);
	    libvlc_time_t total = libvlc_media_player_get_length(player);

	    if (total > 0) {
	        int percent = (current * 100) / total;
	    progressBar->setValue(percent);

	    int currentSec = current / 1000;
	    int totalSec = total / 1000;

	    QString timeText = QString("%1:%2 / %3:%4")
	        .arg(currentSec / 60)
		    .arg(currentSec % 60, 2, 10, QChar('0'))
		    .arg(totalSec / 60)
		    .arg(totalSec % 60, 2, 10, QChar('0'));
	        
	    timeLabel->setText(timeText);
	}
    });

    QObject::connect(volumeSlider, &QSlider::valueChanged, [&](int value){
        if(player)
	    libvlc_audio_set_volume(player,value);
    });

    updateTimer->start(100);
    if(player)
        libvlc_audio_set_volume(player, 50);

    
    window.show();
    int ret = app.exec();
    
    // Cleanup
    if (player) {
        libvlc_media_player_stop(player);
        libvlc_media_player_release(player);
    }
    if (vlc) libvlc_release(vlc);
    
    return ret;
}
