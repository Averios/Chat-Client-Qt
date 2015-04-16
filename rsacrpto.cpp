#include "rsacrpto.h"
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <string.h>

RSACrpto::RSACrpto()
{
    this->public_key = NULL;
    this->private_key = NULL;
    this->keypair = NULL;
}

RSACrpto::~RSACrpto()
{
    free(public_key);
    free(private_key);
    if(keypair != NULL)
        RSA_free(keypair);
}

RSA* RSACrpto::InitKey(size_t key_len){
    size_t pri_len;
    size_t pub_len;

    BIGNUM *bne = BN_new();
    BN_set_word(bne, RSA_F4);
    keypair = RSA_new();
    int r = RSA_generate_key_ex(keypair, key_len, bne, NULL);
    BN_free(bne);

    BIO* pri = BIO_new(BIO_s_mem());
    BIO* pub = BIO_new(BIO_s_mem());

    PEM_write_bio_RSAPrivateKey(pri, keypair, NULL, NULL, 0 ,NULL, NULL);
    PEM_write_bio_RSAPublicKey(pub, keypair);

    pri_len = BIO_pending(pri);
    pub_len = BIO_pending(pub);

    private_key = (char*)malloc(pri_len + 1);
    public_key = (char*)malloc(pub_len + 1);

    BIO_read(pri, private_key, pri_len);
    BUI_read(pub, public_key, pub_len);

    private_key[pri_len] = '\0';
    public_key[pub_len] = '\0';

    BIO_free_all(pri);
    BIO_free_all(pub);

    return keypair;
}

RSA* RSACrpto::CreateRSA(bool isPublic){
    BIO* keybio;
    if(isPublic){
        keybio = BIO_new_mem_buf(this->public_key, -1);
        if(keybio == NULL)
            return 0;
        keypair = PEM_read_bio_RSA_PUBKEY(keybio, &keypair, NULL, NULL);
    }
    else{
        keybio = BIO_new_mem_buf(this->private_key, -1);
        if(keybio == NULL)
            return 0;
        keypair = PEM_read_bio_RSAPrivateKey(keybio, &keypair, NULL, NULL);
    }
    BIO_free(keybio);
    return keypair;
}

bool RSACrpto::setPubKey(const char *public_key, int public_len){
    if(this->public_key != NULL)
        free(this->public_key);
    this->public_key = NULL;
    int length;
    if(public_len == -1){
        length = strlen(public_key);
        this->public_key = (char*)malloc(length);
    }
    else{
        length = public_len;
        this->public_key = (char*)malloc(public_len);
    }

    if(this->public_key){
        memcpy(this->public_key, public_key, length);
        this->CreateRSA(true);
        return true;
    }
    else{
        return false;
    }
}

bool RSACrpto::setPubKey(FILE *public_key){
    fseek(public_key, 0, SEEK_END);
    size_t file_size = ftell(public_key);

    if(this->public_key != NULL)
        free(this->public_key);
    this->public_key = NULL;
    this->public_key = (char*)malloc(file_size);
    if(!this->public_key)
        return false;

    fseek(public_key, 0, SEEK_SET);
    fread(this->public_key, 1, file_size, public_key);
    this->CreateRSA(true);
    return true;
}

bool RSACrpto::setPubKey(QString public_key){
    QByteArray ba = public_key.toLatin1();
    return this->setPubKey(ba.data());
}

bool RSACrpto::setPubKey(std::string public_key){
    return this->setPubKey(public_key.c_str());
}

bool RSACrpto::setPrivateKey(const char *private_key, int private_len){
    if(this->private_key != NULL)
        free(this->private_key);
    this->private_key = NULL;
    int length;
    if(private_len == -1){
        length = strlen(private_key);
        this->private_key = (char*)malloc(length);
    }
    else{
        length = private_len;
        this->private_key = (char*)malloc(private_len);
    }

    if(this->private_key){
        memcpy(this->private_key, private_key, length);
        this->CreateRSA(false);
        return true;
    }
    else{
        return false;
    }
}

bool RSACrpto::setPrivateKey(FILE *private_key){
    fseek(private_key, 0, SEEK_END);
    size_t file_size = ftell(private_key);

    if(this->private_key != NULL)
        free(this->private_key);
    this->private_key = NULL;
    this->private_key = (char*)malloc(file_size);
    if(!this->private_key)
        return false;

    fseek(private_key, 0, SEEK_SET);
    fread(this->private_key, 1, file_size, private_key);
    this->CreateRSA(false);
    return true;
}

bool RSACrpto::setPrivateKey(QString private_key){
    QByteArray ba = private_key.toLatin1();
    return this->setPrivateKey(ba.data());
}

bool RSACrpto::setPrivateKey(std::string private_key){
    return this->setPrivateKey(private_key.c_str());
}

QString RSACrpto::public_encrypt(const char *data, int data_len, int padding){
    int data_size;
    if(data_len == -1)
        data_size = strlen(data);
    else
        data_size = data_len;
    char encrypted[4096];
    int encrypt_len = RSA_public_encrypt(data_size, (unsigned char*)data, (unsigned char*)encrypted, keypair, padding);
    return QString::fromUtf8(encrypted, encrypt_len);
}

QString RSACrpto::public_decrypt(const char *data, int data_len, int padding){
    int data_size;
    if(data_len == -1)
        data_size = strlen(data);
    else
        data_size = data_len;
    char decrypted[4096];
    int decrypt_len = RSA_public_decrypt(data_size, (unsigned char*)data, (unsigned char*)decrypted, keypair, padding);
    return QString::fromUtf8(decrypted, decrypt_len);
}

QString RSACrpto::private_encrypt(const char *data, int data_len, int padding){
    int data_size;
    if(data_len == -1)
        data_size = strlen(data);
    else
        data_size = data_len;
    char encrypted[4096];
    int encrypt_len = RSA_private_encrypt(data_size, (unsigned char*)data, (unsigned char*)encrypted, keypair, padding);
    return QString::fromUtf8(encrypted, encrypt_len);
}

QString RSACrpto::private_decrypt(const char *data, int data_len, int padding){
    int data_size;
    if(data_len == -1)
        data_size = strlen(data);
    else
        data_size = data_len;
    char decrypted[4096];
    int decrypt_len = RSA_private_decrypt(data_size, (unsigned char*)data, (unsigned char*)decrypted, keypair, padding);
    return QString::fromUtf8(decrypted, decrypt_len);
}

QString RSACrpto::public_encrypt(QString data, int padding){
    QByteArray ba = data.toLatin1();
    return this->public_encrypt(ba.data(), data.length(), padding);
}

QString RSACrpto::public_encrypt(std::string data, int padding){
    return this->public_encrypt(data.c_str(), data.length(), padding);
}

QString RSACrpto::public_decrypt(QString data, int padding){
    QByteArray ba = data.toLatin1();
    return this->public_decrypt(ba.data(), data.length(), padding);
}

QString RSACrpto::public_decrypt(std::string data, int padding){
    return this->public_decrypt(data.c_str(), data.length(), padding);
}

QString RSACrpto::private_encrypt(QString data, int padding){
    QByteArray ba = data.toLatin1();
    return this->private_encrypt(ba.data(), data.length(), padding);
}

QString RSACrpto::private_encrypt(std::string data, int padding){
    return this->private_encrypt(data.c_str(), data.length(), padding);
}

QString RSACrpto::private_decrypt(QString data, int padding){
    QByteArray ba = data.toLatin1();
    return this->private_decrypt(ba.data(), data.length(), padding);
}

QString RSACrpto::private_decrypt(std::string data, int padding){
    return this->private_decrypt(data.c_str(), data.length(), padding);
}

RSA* RSACrpto::getKey(){
    return this->keypair;
}
