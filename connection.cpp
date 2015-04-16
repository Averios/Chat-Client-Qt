#include "connection.h"
#include "publicchat.h"
#include "privatechat.h"
#include <QMessageBox>
#include <QDebug>
#include <iostream>
#include <iterator>
#include <vector>
#include <random>
#include <algorithm>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <stdio.h>

Connection::Connection(int refreshRate_msec, QObject *parent) : QObject(parent)
{
    timer.setInterval(refreshRate_msec);
    this->InitRSA();
    connect(&timer, SIGNAL(timeout()), this, SLOT(checkUserList()));
    isApplicationRunning = false;
}

Connection::~Connection()
{

}

bool Connection::connectToHost(QString IP, quint16 Port, QString Username){
    socket = new QTcpSocket(this);
    qDebug() << Username;
    this->username = Username;
    connect(socket, SIGNAL(readyRead()), this, SLOT(incomingMessage()));
    socket->connectToHost(IP, Port);
    if(socket->waitForConnected()){
        connect(socket, SIGNAL(disconnected()), this, SLOT(disconnected()));
        PublicChat* thePublic = (PublicChat*)parent();
        connect(thePublic, SIGNAL(sendMessage(QString)), this, SLOT(outgoingPublicMessage(QString)));
        isApplicationRunning = true;
        this->SetRC4Key();
        this->postPubKey();
        qDebug() << "Stop Here!!";
        //TODO: Encrypt the username
        QString sha_name = HashEngine.toSHA1(Username);
        QString encrypted = rc4->crypt(Username + "\r\n.,\r\n" + sha_name);
        socket->write("Mode: Username\r\n" + encrypted.toUtf8() + "\r\n.\r\n");

        timer.start();
        return true;
    }
    return false;
}

void Connection::disconnected(){
    if(isApplicationRunning){
        QMessageBox alert;
        alert.setWindowTitle("Disconnected");
        alert.setText("You have been disconnected from server\nTrying to reconnect");

        alert.exec();
        while(!socket->reset() && isApplicationRunning);
        socket->write(username.toUtf8() + "\r\n.\r\n");
    }
    else{
        socket->deleteLater();
    }
}

void Connection::newSessionHandler(QString receiver, QObject *sender){
    //TODO: Initiate new session to that user
    PrivateChat* privateChat = (PrivateChat*)sender;
    privateChat->InitiateRC4(randomStringGen(4));
    QString message("Mode: GetPubKey\r\nUser: " + privateChat->getReceiver() + "\r\n.\r\n");
    socket->write(message.toUtf8());
}

void Connection::outgoingPublicMessage(QString messageContent){
    PublicChat* thePublic = (PublicChat*)parent();
    RC4Algorithm *rc4 = thePublic->getRC4();
    QString hash_value = HashEngine.toSHA1(messageContent);
    QString encryptedContent= rc4->crypt(messageContent + "\r\n.,\r\n" + hash_value);
    qDebug() << encryptedContent;
    QString message("Mode: Public\r\n" + encryptedContent + "\r\n.\r\n");
    socket->write(message.toUtf8());
}

void Connection::incomingMessage(){
    QByteArray data = socket->readAll();
    QString message(data);
    PublicChat* PublicWindow = (PublicChat*)parent();
    for(const QString oneMessage : message.split("\r\n.\r\n")){
        if(oneMessage == NULL) continue;
        QStringList stringList = oneMessage.split("\r\n");
        if(stringList.at(0) == "Mode: Public"){
            //Send message to the window
            //'User: '/
            QString newString = stringList.at(1);
            newString.remove(0, 6);
            PublicChat* thePublic = (PublicChat*)parent();
            qDebug() << stringList.at(2);
            RC4Algorithm *rc4 = thePublic->getRC4();
            QString mesg = rc4->crypt(stringList.at(2));
            QStringList content = mesg.split("\r\n.,\r\n");
            if(!content.isEmpty()){
                if(HashEngine.checkIntegrity(content.at(0), content.at(1)))
                    PublicWindow->addMessage(newString, content.at(0));
            }
        }
        else if(stringList.at(0) == "Mode: Private"){
            //Check private window
            //Create new window if necessary
            QString username = stringList.at(1);
            username.remove(0, 6);

            PrivateChat* destination = nullptr;
            for(PrivateChat* now : *(PublicWindow->getPrivateChatList())){
                if(now->getReceiver() == username){
                    destination = now;
                    break;
                }
            }
            //Send message to the window
            if(destination != nullptr){
                if(destination->getRC4() != nullptr){
                    QString decrypted = destination->getRC4()->crypt(stringList.at(2));
                    QStringList content = decrypted.split("\r\n.,\r\n");
                    if(!content.isEmpty()){
                        if(HashEngine.checkIntegrity(content.at(0), content.at(1)))
                            destination->addMessage(content.at(0));
                    }
                }
            }

        }
        else if(stringList.at(0) == "Mode: List"){
            //Contain user list in QStringList
            //Send the list to PublicChat
            QStringList newList(stringList);
            newList.removeFirst();
            PublicWindow->updateUserList(newList);
        }
        else if(stringList.at(0) == "Mode: ClientPubKey"){
            QString username = stringList.at(1);
            username.remove(0, 6);
            PrivateChat* destination = nullptr;
            for(PrivateChat* now : *(PublicWindow->getPrivateChatList())){
                if(now->getReceiver() == username){
                    destination = now;
                    break;
                }
            }
            if(destination != nullptr && !destination->getInitiateStatus()){
                //TODO: Rework this!!!
                //This section is totally wrong
                BIO* bufio;
                RSA* clientKey;
                QByteArray ba = this->rc4->crypt(stringList.at(2)).toLatin1();
                bufio = BIO_new_mem_buf((void*)ba.data(), stringList.at(2).length());
                PEM_read_bio_RSAPublicKey(bufio, &clientKey, NULL, NULL);
                BIO_free_all(bufio);
                if(destination->cryptedKey != "" && !destination->initiator){
                    QByteArray ba2 = destination->cryptedKey.toLatin1();
                    char decrypted[4096];
                    int decrypt_len = RSA_public_decrypt(destination->cryptedKey.length(), (unsigned char*)ba2.data(), (unsigned char*)decrypted, clientKey, RSA_PKCS1_PADDING);
                    decrypted[decrypt_len] = '\0';
                    char* hash_value = strstr(decrypted, "\r\n.,\r\n");
                    *hash_value = '\0';
                    hash_value = hash_value + 6;
                    if(HashEngine.checkIntegrity(QString::fromUtf8(decrypted), QString::fromUtf8(hash_value))){
                        destination->InitiateRC4(std::string(decrypted));
                        destination->setInitiateStatus(true);
                        QString random_message = QString::fromStdString(this->randomStringGen(200));
                        QString random_hash = this->toSHA1(random_message);
                        QString content = destination->getRC4()->crypt(random_message + "\r\n.,\r\n" + random_hash);
                        QString message("Mode: AccPriv\r\nUser: " + destination->getReceiver() + "\r\n" + content + "\r\n.\r\n");
                        socket->write(message.toUtf8());
                    }
                    else{
                        QList<PrivateChat*>* PrivateList = PublicWindow->getPrivateChatList();
                        PrivateList->removeOne(destination);
                        delete destination;
                    }
                }
                else{
                    string clientrc4 = this->randomStringGen(4);
                    QString hash_value = this->toSHA1(clientrc4);
                    QString To_encrypt = QString::fromStdString(clientrc4) + "\r\n.,\r\n" + hash_value;
                    QByteArray ba2 = To_encrypt.toLatin1();
                    char encrypt[4096];
                    int encrypt_len = RSA_private_encrypt(To_encrypt.length(), (unsigned char*)ba2.data(), (unsigned char*)encrypt, keypair, RSA_PKCS1_PADDING);
                    char encrypt2[4096];
                    int encrypt2_len = RSA_public_encrypt(encrypt_len, (unsigned char*)encrypt, (unsigned char*)encrypt2, clientKey, RSA_PKCS1_OAEP_PADDING);
                    QString message("Mode: InitPriv\r\nUser: " + destination->getReceiver() + "\r\n" + QString::fromUtf8(encrypt2, encrypt2_len) + "\r\n.\r\n");
                    socket->write(message.toUtf8());
                }
                RSA_free(clientKey);
            }
        }
        else if(stringList.at(0) == "Mode: AccPriv"){
            QString username = stringList.at(1);
            username.remove(0, 6);
            PrivateChat* destination = nullptr;
            for(PrivateChat* now : *(PublicWindow->getPrivateChatList())){
                if(now->getReceiver() == username){
                    destination = now;
                    break;
                }
            }
            if(destination != nullptr && destination->initiator && !destination->getInitiateStatus()){
                QString decrypted = destination->getRC4()->crypt(stringList.at(2));
                QStringList thePair = decrypted.split("\r\n.,\r\n");
                if(!thePair.empty()){
                    if(this->checkIntegrity(thePair.at(0), thePair.at(1))){
                        destination->setInitiateStatus(true);
                    }
                }
            }
        }
        else if(stringList.at(0) == "Mode: InitPriv"){
            QString username = stringList.at(1);
            username.remove(0, 6);
            PrivateChat* destination = nullptr;
            for(PrivateChat* now : *(PublicWindow->getPrivateChatList())){
                if(now->getReceiver() == username){
                    destination = now;
                    break;
                }
            }
            if(destination == nullptr){
                destination = PublicWindow->addPrivateChat(username);
                QByteArray ba = stringList.at(2).toLatin1();
                char decrypt[4096];
                int decrypt_len = RSA_private_decrypt(stringList.at(2).length(), (unsigned char*)ba.data(), (unsigned char*)decrypt, keypair, RSA_PKCS1_OAEP_PADDING);
                destination->cryptedKey = QString::fromUtf8(decrypt, decrypt_len);
                destination->initiator = false;
                QString message("Mode: GetPubKey\r\nUser: " + username + "\r\n.\r\n");
                socket->write(message.toUtf8());
            }
        }

    }
}

void Connection::outgoingPrivateMessage(QString receiver, QString messageContent, RC4Algorithm *ClientRC4Key){
    QString message_hash = this->toSHA1(messageContent);
    QString content = ClientRC4Key->crypt(messageContent + "\r\n.,\r\n" + message_hash);
    QString message("Mode: Private\r\nUser: " + receiver + "\r\n" + content + "\r\n.\r\n");
    socket->write(message.toUtf8());
}

void Connection::checkUserList(){
    QString message("Mode: GetList\r\n.\r\n");
    socket->write(message.toUtf8());
}

void Connection::newPrivateWindow(QObject *privateWindow){
    //Add signal listener to the new window
    PrivateChat* privateChat = (PrivateChat*)privateWindow;
    connect(privateChat, SIGNAL(sendMessage(QString,QString,RC4Algorithm*)), this, SLOT(outgoingPrivateMessage(QString,QString,RC4Algorithm*)));
    // TODO : Distribute key with another client
//    privateChat->InitiateRC4(randomStringGen(1024 / 8 - 40));
    QString message("Mode: GetPubKey\r\nUser: " + privateChat->getReceiver() + "\r\n.\r\n");
    socket->write(message.toUtf8());
}

void Connection::setServerKeyPair(const char *key, size_t key_len){
//    BIO* bufio;
//    bufio = BIO_new_mem_buf((void*)key, key_len);
//    PEM_read_bio_RSAPublicKey(bufio, &ServKey, 0, NULL);
//    BIO_free_all(bufio);
    FILE* public_key = fopen("public.pem", "r");
    PEM_read_RSAPublicKey(public_key, &ServKey, NULL, NULL);
    fclose(public_key);
}

int Connection::InitRSA(){
    BIGNUM *bne = BN_new();
    BN_set_word(bne, RSA_F4);
    keypair = RSA_new();
    int r = RSA_generate_key_ex(keypair, 2048, bne, NULL);
    BN_free(bne);
    return r;
}

std::string Connection::randomStringGen(size_t LEN){
//    std::random_device rd;
//    std::default_random_engine rng(rd());
//    std::uniform_int_distribution<> dist(0,sizeof(alphabet)/sizeof(*alphabet)-2);

//    std::string strs;
//    strs.reserve(LEN);
//    std::generate_n(strs.begin(), LEN, [&](){return alphabet[dist(rng)];});
//    return strs;
    int length = LEN;
    auto randchar = []() -> char
    {
        const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
        const size_t max_index = (sizeof(charset) - 1);
        return charset[ rand() % max_index ];
    };
    std::string str(length,0);
    std::generate_n( str.begin(), length, randchar );
    return str;
}

void Connection::postPubKey(){
    // Sending Public key to the server
    size_t pub_len;
    char* pub_key;
    BIO* pub = BIO_new(BIO_s_mem());
    PEM_write_bio_RSAPublicKey(pub, keypair);
    pub_len = BIO_pending(pub);
    pub_key = (char*) malloc(pub_len + 1);
    BIO_read(pub, pub_key, pub_len);
    pub_key[pub_len] = '\0';
    QString sha_value = this->toSHA1(pub_key, pub_len);
    QString content = this->rc4->crypt(QString::fromUtf8(pub_key, pub_len) + "\r\n.,\r\n" + sha_value);
    QString message("Mode: SetPubKey\r\n" + content + "\r\n.\r\n");
    socket->write(message.toUtf8());
    free(pub_key);
    BIO_free_all(pub);
    //
}

void Connection::SetRC4Key(){
    std::string rc4key = randomStringGen(4);
//    qDebug() << QString::fromStdString(rc4key);
    RSA* serverKey = this->ServKey;
    this->rc4 = new RC4Algorithm(rc4key);
    //TODO: Send key to server
    QString key_hash = this->toSHA1(rc4key);
    QString raw_data = QString::fromStdString(rc4key) + "\r\n.,\r\n" + key_hash;
    QByteArray ba = raw_data.toLatin1();
    const char* toEncrypted = ba.data();
    char encrypt[4096];
    int encrypt_len;
//    qDebug() << raw_data;
    printf("%s\n\n\n", toEncrypted);
    encrypt_len = RSA_public_encrypt(raw_data.length(), (unsigned char*)toEncrypted, (unsigned char*)encrypt, serverKey, RSA_PKCS1_OAEP_PADDING);
    QString message = "Mode: SetRC4Key\r\n";
    for(int i = 0; i < encrypt_len; i++)
        message += *(encrypt + i);
    message += "\r\n.\r\n";
    socket->write(message.toUtf8());
}
