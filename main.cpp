#include<iostream>
#include<filesystem>
#include<taglib/fileref.h>
#include<vlc/vlc.h>
#include<cstdlib>
#include<string>
#include<QSettings>
#include<QApplication>
#include<QWidget>
#include<QPushButton>
#include<QVBoxLayout>
#include<QHBoxLayout>
#include<QTreeView>
#include<QFileSystemModel>
#include<QFileDialog>
#include<QDir>
#include<QDebug>
#include<QListWidget>
namespace fs=std::filesystem;
std::string getMusicDir(void);
//    std::string home = getMusicDir();
int main(int argc, char *argv[]) 
{
    QApplication app(argc, argv);

    libvlc_instance_t *vlc = libvlc_new(0, nullptr);
    libvlc_media_player_t *player = libvlc_media_player_new(vlc);
        
    QString currentsong;
    QStringList queue;

    QWidget window;
    window.setWindowTitle("mp");

    QListWidget *queueView = new QListWidget(&window);
    queueView->setMaximumWidth(200);
    
    QPushButton pickFolderButton("Explore Folders");
    QPushButton playButton("||  /  ->");

    QFileSystemModel *model = new QFileSystemModel(&window);
    model->setFilter(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    model->setNameFilters(QStringList() << "*.mp3" << "*.flac" << "*.wav");
    model->setNameFilterDisables(false);

    QTreeView *tree = new QTreeView(&window);
    tree->setHeaderHidden(true);
    tree->setModel(model);
    tree->setRootIndex(QModelIndex());
    tree->setVisible(false);

    QSettings settings("mp", "mp");
    QString musicfolder = settings.value("lastMusicFolder", "").toString();

    if(!musicfolder.isEmpty() && QDir(musicfolder).exists()){
        QModelIndex index = model->setRootPath(musicfolder);
        tree->setRootIndex(index);
        tree->setVisible(true);
    }

    QVBoxLayout *buttonLayout = new QVBoxLayout;
    QHBoxLayout *mainLayout = new QHBoxLayout(&window);

    buttonLayout->addWidget(&pickFolderButton);
    buttonLayout->addWidget(&playButton);
    buttonLayout->addStretch();

    mainLayout->addLayout(buttonLayout);
    mainLayout->addWidget(tree);
    mainLayout->addWidget(queueView);
    
    QObject::connect(&pickFolderButton, &QPushButton::clicked, [&](){
            musicfolder = QFileDialog::getExistingDirectory(
                    &window,
                    "Select Music Folder",
                    QDir::homePath()
            );
            qDebug() << "Music folder: " << musicfolder;
            if (!musicfolder.isEmpty()){
                settings.setValue("lastMusicFolder", musicfolder);
                QModelIndex index = model->setRootPath(musicfolder);
                tree->show();
                tree->setModel(model);
                tree->setRootIndex(index);

            }
    });
    QObject::connect(tree, &QTreeView::doubleClicked, [&](const QModelIndex &index) {
        if(!index.isValid()) return;
        QString filepath = model->filePath(index);
        QFileInfo info(filepath);
        if(!info.isFile()) return;
        queue.append(filepath);
        queueView->addItem(info.fileName());
        qDebug() << "Added to queue:" << filepath;
        if (!libvlc_media_player_is_playing(player)) {
            currentsong = filepath;
            libvlc_media_t *media = libvlc_media_new_path(vlc, currentsong.toStdString().c_str());
            libvlc_media_player_set_media(player, media);
            libvlc_media_release(media);
            libvlc_media_player_play(player);
        }
    });

    // Play button
    QObject::connect(&playButton, &QPushButton::clicked, [&]() {
        if (currentsong.isEmpty()) return; // no song selected

        if (!libvlc_media_player_is_playing(player)) {
            libvlc_media_player_play(player);
        } else 
            libvlc_media_player_set_pause(player, 1);
    });
    window.show();
    int ret = app.exec();

    libvlc_media_player_stop(player);
    libvlc_media_player_release(player);
    libvlc_release(vlc);

    return ret;
}
