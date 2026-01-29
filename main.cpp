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
    
    QPushButton *pickFolderButton = new QPushButton("Select Folder");
    QPushButton *playButton = new QPushButton("Play/Pause");
    QListWidget *queueView = new QListWidget;
    QTreeView *tree = new QTreeView;
    QFileSystemModel *model = new QFileSystemModel;
	
    QLabel *timeLabel = new QLabel("0:00 / 0:00");
    QSlider *progressBar = new QSlider(Qt::Horizontal);
    QLabel *volumeLabel = new QLabel("Volume");
    QSlider *volumeSlider = new QSlider(Qt::Horizontal);
    volumeSlider->setRange(0,100);
    volumeSlider->setValue(50);

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
    
    // Layout
    QHBoxLayout *topBar = new QHBoxLayout;
    topBar->addWidget(pickFolderButton);
    topBar->addWidget(playButton);
    topBar->addStretch();
    topBar->addWidget(timeLabel);
    topBar->addWidget(progressBar);
    topBar->addWidget(volumeLabel);
    topBar->addWidget(volumeSlider);

    QHBoxLayout *contentLayout = new QHBoxLayout;
    contentLayout->addWidget(tree,2);
    contentLayout->addWidget(queueView, 1);

    
    QVBoxLayout *mainLayout = new QVBoxLayout(&window);
    mainLayout->addLayout(topBar);
    mainLayout->addLayout(contentLayout, 1);
    
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
    
    // Double click to add to queue and play
    QObject::connect(tree, &QTreeView::doubleClicked, [&](const QModelIndex &index) {
        QString path = model->filePath(index);
        QFileInfo info(path);
        
        if (!info.isFile()) return;
        if (!player || !vlc) return;
        
        queueView->addItem(info.fileName());
        currentsong = path;
        
        libvlc_media_player_stop(player);
        libvlc_media_t *media = libvlc_media_new_path(vlc, path.toUtf8().constData());
        
        if (media) {
            libvlc_media_player_set_media(player, media);
            libvlc_media_release(media);
            libvlc_media_player_play(player);
            std::cerr << "Playing: " << path.toStdString() << std::endl;
        }
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
    
    QObject::connect(updateTimer, &QTimer::timeout, [&]() {
        if(!player) return;

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
