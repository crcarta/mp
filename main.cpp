#include<iostream>
#include<filesystem>
#include<taglib/fileref.h>
#include<vlc/vlc.h>
#include<cstdlib>
#include<string>
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
namespace fs=std::filesystem;
std::string getMusicDir(void);
//    std::string home = getMusicDir();
int main(int argc, char *argv[]) 
{
    QApplication app(argc, argv);
    libvlc_instance_t *vlc = libvlc_new(0, nullptr);
    libvlc_media_player_t *player = libvlc_media_player_new(vlc);
        
    QString musicfolder;
    QString currentsong;

    QWidget window;
    window.setWindowTitle("mp");
    
    QPushButton pickFolderButton("Explore Folders");
    QPushButton pickButton("Pick Song");
    QPushButton playButton("Play Song");

    QFileSystemModel *model = new QFileSystemModel;
    model->setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    model->setRootPath(QDir::homePath());
    QTreeView *libraryView = new QTreeView;
    libraryView->setModel(model);
    libraryView->setRootIndex(model->index(QDir::homePath()));
    libraryView->setHeaderHidden(true);

    QVBoxLayout *buttonLayout = new QVBoxLayout;
    buttonLayout->addWidget(&pickFolderButton);
    buttonLayout->addWidget(&pickButton);
    buttonLayout->addWidget(&playButton);
    buttonLayout->addStretch();

    QHBoxLayout *mainLayout = new QHBoxLayout(&window);
    mainLayout->addLayout(buttonLayout);
    mainLayout->addWidget(libraryView);
    
    QObject::connect(&pickFolderButton, &QPushButton::clicked, [&](){
            musicfolder = QFileDialog::getExistingDirectory(
                    &window,
                    "Select Music Folder",
                    QDir::homePath()
            );
            if (!musicfolder.isEmpty()){
                qDebug() << "Music folder: " << musicfolder;
                libraryView->setRootIndex(model->index(musicfolder));
            }
    });

    QObject::connect(&pickButton, &QPushButton::clicked, [&]() {
    QString file = QFileDialog::getOpenFileName(
        &window,"Select a song",QDir::homePath(),
        "Audio Files (*.mp3 *.flac *.wav)"
        );

        if (!file.isEmpty()) {
            currentsong = file;
            qDebug() << "Selected:" << currentsong;

            libvlc_media_t *media = libvlc_media_new_path(vlc, currentsong.toStdString().c_str());
            libvlc_media_player_set_media(player, media);
            libvlc_media_release(media);
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

std::string getMusicDir(void)
{
    std::string musicdir;
    std::cout << "Welcome to mp music player. Please" 
        " enter your music directory: ";
    std::getline(std::cin, musicdir);
    return std::string(musicdir);
}

