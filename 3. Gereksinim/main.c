#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Tüm program süresince endless loop yani sonsuz döngü hatalarından kaçınmak için
// #pragma anahtarını kullanma kararı aldım.
#pragma clang diagnostic push // clang diagnostic push benim bazı hata mesajlarını bastırmamı sağlıyor.
#pragma ide diagnostic ignored "EndlessLoop" // ide diagnostic ignored "EndlessLoop" ide taraflı EndlessLoop uyarılarını baskılıyor.

typedef struct {
    int filozofID;                        // Filozoflar üzerinden rahatça işlem yapabilmek için 0-5 aralığında id ataması yapıyorum.
    long baslangicZamani;                // Programın başlangıç zamanı için bir değişken tanımlıyorum. Veri tipi kaynaklı herhangi bir hata almamak için long olarak tanımlıyorum.
    long maksimumIslemSuresi;           // Filozofların maksimum işlem süresi yani maksimum yeme uyuma sürelerini tanımlıyorum.
    void *erisimTanimi;                // Yapının anahtar tanımlamasını burada gerçekleştiriyorum daha sonrasında bu yapıyı çatal mutexleri için de kullanacağım.
    int *filozofBlokajSuresi;         // Filozof için bir blokaj süresi belirliyorum. Bu süre içerisinde filozof herhangi bir işlem yapmayacak.
    int filozofSayisi;               // Sistemdeki filozof sayısını tanımlıyorum.
    pthread_mutex_t *blokajSuresi;  // Blokaj süresi için bir mutex tanımlıyorum. Daha sonrasında yönetebilmek için.
} filozofYapi;

#define MAKSIMUMTHREADSAYISI (5) // Sistemdeki maksimum filozof sayısı için bir tanımlama yapıyorum.
#define dusunenFilozof 0          /* Düşünen */
#define karniAcFilozof 1          /*        Karnı aç olan ve */
#define yemekteOlanFilozof 2      /*                        Yemek yiyen filozoflar için önceden sayı tanımlaması gerçekleştiriyorum.*/

// Ödevin ilk gereksinimini karşılamak üzere bir filozof yapısı kuruyorum.
typedef struct {
    pthread_mutex_t *filozofMut;                            // Daha sonrasında filozofları rahatlıkla yönetebilmek için bir mutex tanımlıyorum.
    pthread_cond_t *filozofKosul[MAKSIMUMTHREADSAYISI];    // Filozofların durumları için (yemek yiyen, aç olan ve düşünen) bir koşul tanımlıyorum.
    int filozofKontrol[MAKSIMUMTHREADSAYISI];             // Maksimum thread yani maksimum filozof sayısı kadar bir kontrol dizisi oluşturuyorum.
    int filozofSayisi;                                   // Yapıya filozof sayısı ekliyorum.
} Filozof;

typedef struct catallar {
    pthread_mutex_t *kilit[MAKSIMUMTHREADSAYISI];
} Catallar;

//filozofYapi structını parametre alan çatal alma fonksiyonunu oluşturuyorum.
void cataliAl(filozofYapi *filozofYapiP) {
    Filozof *filozofP;   // Filozof tipinde bir filozofP pointerı tanımlıyorum.
    Catallar *catalP;     // Çatal tipinde bir catalP pointerı tanımlıyorum.
    int filozofSayisi; // Filozof sayısını tanımlıyorum.

    filozofP = (Filozof *) filozofYapiP->erisimTanimi;     // filozofP değişkenini fonksiyonun parametresindeki filozofYapi'dan çekiyorum.
    // Bu sayede filozofYapi'ya erişim sağlamış oluyorum.
    filozofSayisi = filozofP->filozofSayisi;             // filozofP pointerını kullanarak filozof sayısını çekiyorum.
    catalP = (Catallar *) filozofYapiP->erisimTanimi;

    //3. gereksinim için filozof sayısının 2 ile modunun kontrolüne göre çatalları alıyor.
    if (filozofYapiP->filozofID % 2 == 1) {
        pthread_mutex_lock(catalP->kilit[filozofYapiP->filozofID]);                         // Filozof için sol çatal kilitleniyor.
        printf("%d. filozof sol catali aldi.", filozofYapiP->filozofID);                   // Konsola gerekli çıktıları gönderiyorum.
        pthread_mutex_lock(catalP->kilit[(filozofYapiP->filozofID + 1) % filozofSayisi]); // Filozof için sağ çatal kilitleniyor.
        printf("%d. filozof sag catali aldi.", filozofYapiP->filozofID);                 // Konsola gerekli çıktıları gönderiyorum.
    } else {
        pthread_mutex_lock(catalP->kilit[(filozofYapiP->filozofID + 1) % filozofSayisi]); // Filozof için sağ çatal kilitleniyor.
        printf("%d. filozof sag catali aldi.", filozofYapiP->filozofID);                 // Konsola gerekli çıktıları gönderiyorum.
        pthread_mutex_lock(catalP->kilit[filozofYapiP->filozofID]);                     // Filozof için sol çatal kilitleniyor.
        printf("%d. filozof sol catali aldi.", filozofYapiP->filozofID);               // Konsola gerekli çıktıları gönderiyorum.
    }

    pthread_mutex_lock(filozofP->filozofMut);                             // filozofP pointerından faydalanarak filozof mutexini kilitliyorum.
    filozofP->filozofKontrol[filozofYapiP->filozofID] = karniAcFilozof;  // filozofP pointerından faydalanarak filozofKontrol dizisinin filozofID. filozofunu karnı aç olarak tanımlıyorum.

    while (filozofP->filozofKontrol[(filozofYapiP->filozofID + (filozofSayisi-1)) % filozofSayisi] == yemekteOlanFilozof ||
           filozofP->filozofKontrol[(filozofYapiP->filozofID + 1) % filozofSayisi] == yemekteOlanFilozof) { // filozofKontrol dizisinin koşulları yemekteOlan'a eşit olanlar için bir döngü başlatıyorum.
        pthread_cond_wait(filozofP->filozofKosul[filozofYapiP->filozofID], filozofP->filozofMut);           // Bu döngü süresince filozof yemekteyse bekleme komutu alıyor.
    }
    filozofP->filozofKontrol[filozofYapiP->filozofID] = yemekteOlanFilozof; // Yemekte olmayan filozofların durumları filozofKontrol üzerinden yemekteOlanFilozof olarak değiştiriliyor.
    pthread_mutex_unlock(filozofP->filozofMut);                            // Kilitlenen mutexi serbest bırakıyorum.
}

//filozofYapi structını parametre alan çatal bırakma fonksiyonunu oluşturuyorum.
void cataliBirak(filozofYapi *filozofYapiP) {
    Filozof *filozofP;    // Filozof tipinde bir filozofP pointerı tanımlıyorum.
    Catallar *catalP;    // Çatal tipinde bir catalP pointerı tanımlıyorum.
    int filozofSayisi;  // Filozof sayısını tanımlıyorum.

    filozofP = (Filozof *) filozofYapiP->erisimTanimi;    // filozofP değişkenini fonksiyonun parametresindeki filozofYapi'dan çekiyorum.
    // Bu sayede filozofYapi'ya erişim sağlamış oluyorum.
    filozofSayisi = filozofP->filozofSayisi;            // filozofP pointerını kullanarak filozof sayısını çekiyorum.
    catalP = (Catallar *) filozofYapiP->erisimTanimi;

    //3. gereksinim için filozof sayısının 2 ile modunun kontrolüne göre çatalları geri bırakıyor.
    if (filozofYapiP->filozofID % 2 == 1) {
        pthread_mutex_unlock(catalP->kilit[(filozofYapiP->filozofID + 1) % filozofSayisi]); // Sağ çatal için kilitlenen mutexi serbest bırakıyorum.
        printf("%d. filozof sag catali birakti.", filozofYapiP->filozofID);                // Konsola gerekli çıktıları gönderiyorum.
        pthread_mutex_unlock(catalP->kilit[filozofYapiP->filozofID]);                     // Sol çatal için kilitlenen mutexi serbest bırakıyorum.
        printf("%d. filozof sol catali birakti.", filozofYapiP->filozofID);              // Konsola gerekli çıktıları gönderiyorum.
    } else {
        pthread_mutex_unlock(catalP->kilit[filozofYapiP->filozofID]);                         // Sol çatal için kilitlenen mutexi serbest bırakıyorum.
        printf("%d. filozof sol catali birakti.", filozofYapiP->filozofID);                  // Konsola gerekli çıktıları gönderiyorum.
        pthread_mutex_unlock(catalP->kilit[(filozofYapiP->filozofID + 1) % filozofSayisi]); // Sağ çatal için kilitlenen mutexi serbest bırakıyorum.
        printf("%d. filozof sag catali birakti.", filozofYapiP->filozofID);                // Konsola gerekli çıktıları gönderiyorum.
    }

    pthread_mutex_lock(filozofP->filozofMut);                              // filozofP pointerından faydalanarak filozof mutexini kilitliyorum.
    filozofP->filozofKontrol[filozofYapiP->filozofID] = dusunenFilozof;   // filozofP pointerından faydalanarak filozofKontrol dizisinin filozofID. filozofunu düşünen olarak tanımlıyorum.

    if (filozofP->filozofKontrol[(filozofYapiP->filozofID + (filozofSayisi-1)) % filozofSayisi] == karniAcFilozof) { // filozofKontrol dizisinin koşulları karniAcFilozof'a eşit olanlar için bir kontrol süreci başlatıyorum.
        pthread_cond_signal(filozofP->filozofKosul[(filozofYapiP->filozofID + (filozofSayisi-1)) % filozofSayisi]); // Eğer karniAcFilozof durumuna eşitse süreç için sinyal gönderiyorum.
    }

    if (filozofP->filozofKontrol[(filozofYapiP->filozofID + 1) % filozofSayisi] == karniAcFilozof) { // filozofKontrol dizisinin koşulları karniAcFilozof'a eşit olanlar için bir kontrol süreci başlatıyorum.
        pthread_cond_signal(filozofP->filozofKosul[(filozofYapiP->filozofID + 1) % filozofSayisi]); // Eğer karniAcFilozof durumuna eşitse süreç için sinyal gönderiyorum.
    }
    pthread_mutex_unlock(filozofP->filozofMut);         // Kilitlenen mutexi serbest bırakıyorum.
}

//Filozof sayısını parametre olarak alan yemek süreci fonksiyonunu oluşturuyorum.
void *yemekSureciTanimlamalari(int filozofSayisi) {
    Filozof *filozofP;   // Filozof tipinde bir filozofP pointerı tanımlıyorum.
    Catallar *catalP;   // Çatal tipinde bir catalP pointerı tanımlıyorum.
    int i;             // Döngüler için önceden bir i değişkeni tanımlıyorum. Daha sonrasında farklı farklı değişkenlerden kaçınmak için değer atamıyorum.

    filozofP = (Filozof *) malloc(sizeof(Filozof)*filozofSayisi);  // Filozof sayısı kadar bellekte yer ayırıyorum.
    catalP = (Catallar *) malloc(sizeof(Catallar));               // Çatal sayısı kadar bellekte yer ayırıyorum.
    filozofP->filozofSayisi = filozofSayisi;                     // filozofP pointerından faydalanarak filozof sayısına erişiyorum.
    filozofP->filozofMut = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t)); //filozofP pointerından faydalanarak mutex kadar bellekte yer ayırıyorum.
    pthread_mutex_init(filozofP->filozofMut, NULL);                            // filozofMut mutexlerine null olarak atama yapıyorum.

    for (i = 0; i < filozofSayisi; i++) {  // Ödevin 1. gereksinimi için istenen n-1 koşulunu sağlayacak döngüyü başlatıyorum.
        filozofP->filozofKosul[i] = (pthread_cond_t *) malloc(sizeof(pthread_cond_t));  // Koşullar için bellekte gerekli yer tahsis ediyorum.
        catalP->kilit[i] = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));        // Her bir çatal mutexi için bellekte yer ayırıyorum.
        pthread_cond_init(filozofP->filozofKosul[i], NULL);  // filozofKosul dizisinin i. elemanını NULL olarak atıyorum. Bu sayede program her açılışta sıfırdan başlangıç yapabilecek.
        pthread_mutex_init(catalP->kilit[i], NULL);         // Daha önce yer ayırdığım mutexleri eşliyorum.
        filozofP->filozofKontrol[i] = dusunenFilozof;      // Filozof durumlarını öncelikle düşünen olarak ayarlıyorum.
    }

    return (void *) filozofP; // Fonksiyonun dönütünü filozofP olarak atıyorum.
}

// _Noreturn anahtarı ile sürecin bitmesinin ardından geri dönme noktasını ortadan kaldırıyorum.
// Bu şekilde olası endless loop ve benzeri hataların önüne geçiyorum.
_Noreturn void *filozofSureci(void *catalTanim) {
    filozofYapi *filozofP; // Filozof tipinde bir filozofP pointerı tanımlıyorum.
    long rastgeleSayi;    // Önceden long tipinde rastgeleSayi değişkenini tanımlıyorum.
    long rastgeleSure;   // Önceden long tipinde rastgeleSure değişkenini tanımlıyorum

    filozofP = (filozofYapi *) catalTanim;  // filozofP pointerını fonksiyonun parametresini kullanarak tanımlıyorum.

    while(1) {
        // Filozof için rastgele düşünme süresi atıyorum.
        rastgeleSayi = (rand() % filozofP->maksimumIslemSuresi) + 1; // Düşünme süresi için rastgele sayı üretiyorum.

        printf("%d. filozof %d saniye boyunca dusundu.\n", filozofP->filozofID, rastgeleSayi); // Konsola gerekli çıktıyı veriyorum.
        fflush(stdout); // Önceki akışı temizlemek için kullanıyorum.
        sleep(rastgeleSayi); // Sistemi üretilen düşünme süresi kadar uyutuyorum.

        // Uyanan ve acıkan filozof için çatalı alma fonksiyonunu çağırıyorum.
        printf("%d. filozof artik dusunmuyor.\n", filozofP->filozofID); // Konsola gerekli çıktıyı veriyorum.
        fflush(stdout); // Önceki akışı temizlemek için kullanıyorum.
        rastgeleSure = time(0); // Bu süreç için rastgele bir süre oluşturuyorum.
        cataliAl(filozofP);    // filozofP pointerını parametre girerek cataliAl() fonksiyonunu çağırıyorum.
        pthread_mutex_lock(filozofP->blokajSuresi); // filozofP için mutexi kilitliyorum.
        filozofP->filozofBlokajSuresi[filozofP->filozofID] += (time(0) - rastgeleSure); // blokajSuresi dizisine önceden oluşturduğum süreyi atıyorum.
        pthread_mutex_unlock(filozofP->blokajSuresi); // Daha önceden filozofP için kilitlediğim mutexi serbest bırakıyorum.

        // Çatalı alma süreci tamamlandıktan sonra filozof yemek yeme durumuna geçiyor.
        rastgeleSayi = (rand()%filozofP->maksimumIslemSuresi) + 1; // Bu işlem için de tekrar rastgele bir sayı oluşturuyorum.
        printf("%d. filozof %d saniye boyunca yemek yedi.\n", filozofP->filozofID, rastgeleSayi); // Konsola gerekli çıktıyı veriyorum.
        fflush(stdout);          // Önceki akışı temizlemek için kullanıyorum.
        sleep(rastgeleSayi);    // Sistemi üretilen düşünme süresi kadar uyutuyorum.

        // Yemek yeme süreci biten filozof için çatalı bırakma fonksiyonunu çağırıyorum.
        printf("%d. filozof artik yemek yemiyor.\n", filozofP->filozofID); // Konsola gerekli çıktıyı veriyorum.
        fflush(stdout);         // Önceki akışı temizlemek için kullanıyorum.
        cataliBirak(filozofP); // filozofP pointerını parametre girerek cataliBirak() fonksiyonunu çağırıyorum.
    }
}

// Dışarıdan filozof sayısı ve maksimum bekleme süresini parametre olarak aldıktan sonra
// yemek yeme sürecini ve diğer süreçleri başlatıyorum.
int main(argc, argv) int argc; char **argv; {
    int i;                      // Döngüler için önceden bir i değişkeni tanımlıyorum. Daha sonrasında farklı farklı değişkenlerden kaçınmak için değer atamıyorum.
    int *filozofBlokajSuresiM; // Blokaj süresi için integer bir pointer tanımlıyorum.
    int filozofSayisi;        // filozofSayisi değişkenini tanımlıyorum.
    int toplam;              // toplam değişkeninin tanımlıyorum.

    long baslangicZamaniM; // Başlangıç zamanı için bir değişken tanımlıyorum.

    void *erisimTanimiM; // erisimTanimi için bir pointer tanımlıyorum.

    char gecici[500];  // Char tipinde geçici bir dizi tanımlıyorum.
    char *anlikD;     // Char tipinde

    pthread_t threads[MAKSIMUMTHREADSAYISI];         // Maksimum filozof sayısı kadar thread oluşturuyorum.
    filozofYapi filozofYapiP[MAKSIMUMTHREADSAYISI]; // filozofYapi tipinde maksimum filozof sayısı kadar yapı oluşturuyorum.
    pthread_mutex_t *blokajSuresiM;                // Blokaj süresi için mutexi tanımlıyorum.

    if (argc != 3) {
        fprintf(stderr, "Eksik parametre: <Filozof Sayısı> <Maksimum Bekleme Süresi>\n"); // Eksik parametre girildiği için konsola hata mesajı gönderiyorum.
        exit(1); // Eksik parametreden dolayı programı sonlandırıyorum.
    }

    srand(time(0)); // Rastgele bir süre oluşturuyorum.
    filozofSayisi = atoi(argv[1]); // Programa girilen 2. parametreyi integer'a çeviriyorum.

    if(filozofSayisi > MAKSIMUMTHREADSAYISI) { // Önceden programa parametre olarak girilen filozof sayısı için bir kontrol yapısı oluşturuyorum.
        filozofSayisi = MAKSIMUMTHREADSAYISI; // Eğer girilen filozof sayısı önceden tanımlı maksimum değerden yüksekse filozof sayısını maksimum değere eşitliyorum.
    }

    erisimTanimiM = yemekSureciTanimlamalari(filozofSayisi); // erisimTanimi için yemek süreci fonksiyonunu çağırıyorum.
    baslangicZamaniM = time(0);                             // Başlangıç zamanı değerinin atamasını gerçekleştiriyorum.
    filozofBlokajSuresiM = (int *) malloc(sizeof(int)*filozofSayisi);       // Bellekte filozof işlemlerinin blokaj süresi kadar yer ayırıyorum.
    blokajSuresiM = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));   // Bellekte blokaj süresi kadar yer ayırıyorum.
    pthread_mutex_init(blokajSuresiM, NULL);    // Blokaj süresi mutexini NULL olarak atıyorum.

    for (i = 0; i < filozofSayisi; i++) { // 1. gereksinimden ötürü n-1 kadar döngü başlatıyorum.
        filozofBlokajSuresiM[i] = 0; // Filozof işlemlerinin blokaj sürelerini 0 olarak atıyorum.
    }

    for (i = 0; i < filozofSayisi; i++) { // 1. gereksinimden ötürü n-1 kadar döngü başlatıyorum.
        filozofYapiP[i].filozofID = i;  // Her bir filozof için filozofID tanımlamasını yapıyorum.
        filozofYapiP[i].baslangicZamani = baslangicZamaniM; // Her bir filozof için baslangicZamani tanımlamasını yapıyorum.
        filozofYapiP[i].erisimTanimi = erisimTanimiM;      // Her bir filozof için erisimTanimi tanımlamasını yapıyorum.
        filozofYapiP[i].maksimumIslemSuresi = atoi(argv[2]);  // Her bir filozof için maksimum işlem süresini integer'a dönüştürülmüş haliyle tanımlamasını yapıyorum.
        filozofYapiP[i].filozofBlokajSuresi = filozofBlokajSuresiM; // Her bir filozof için filozofun işlemlerinin blokaj sürelerinin tanımlamasını yapıyorum.
        filozofYapiP[i].blokajSuresi = blokajSuresiM;              // Her bir filozof için blokaj sürelerinin tanımlamasını yapıyorum.
        pthread_create(threads+i, NULL, filozofSureci, (void *) (filozofYapiP+i));  // Her bir filozof için thread oluşturma işlemini yapıyorum.
    }

    while(1) {
        pthread_mutex_lock(blokajSuresiM); // Blokaj süresi mutexini kilitliyorum.
        anlikD = gecici; // anlikD değerini gecici olarak değiştiriyorum.
        toplam = 0;     // toplam değişkenini oluşturuyorum.

        for(i=0; i < filozofSayisi; i++) { // 1. gereksinimden ötürü n-1 kadar döngü başlatıyorum.
            toplam += filozofBlokajSuresiM[i]; // Toplam değerini blokaj süresinin i. elemanıyla topluyorum.
        }
        anlikD = gecici + strlen(gecici); // anlikD değerini gecici değerleriyle birleştiriyorum.

        for(i=0; i < filozofSayisi; i++) { // 1. gereksinimden ötürü n-1 kadar döngü başlatıyorum.
            anlikD = gecici + strlen(gecici);                // anlikD değerini gecici değerleriyle birleştiriyorum.
        }

        pthread_mutex_unlock(blokajSuresiM); // Blokaj süresi için kilitlediğim mutexi serbest bırakıyorum.
        fflush(stdout); // Önceki akışı temizlemek için kullanıyorum.
        sleep(10); // Sistemi 10 ms boyunca uyutuyorum.
    }
}
#pragma clang diagnostic pop // IDE'nin önceden kaydettiği tanı durumunu geri yüklemeyi sağlar.