---------------------------------------Bucket.h
#ifndef BUCKET_H
#define BUCKET_H

#define B 7             // broj baketa
#define STEP 1          // fiksan korak
#define BUCKET_SIZE 3   // faktor baketiranja

#define WITH_HEADER 1
#define WITHOUT_HEADER 0
#define WITH_KEY 1
#define WITHOUT_KEY 0

#define RECORD_FOUND 0

#define CODE_LEN 7+1
#define DATE_LEN 10+1

typedef enum { EMPTY = 0, ACTIVE, DELETED } RECORD_STATUS;

typedef struct {//ovo bi trebalo da je slog
    int key;//i kljuc po kom trazi
    char code[CODE_LEN];
    char date[DATE_LEN];
    RECORD_STATUS status; //ovo uvek ide
} Record;

typedef struct {
    Record records[BUCKET_SIZE];
} Bucket;

typedef struct {
    int ind1;           // indikator uspesnosti trazenja, 0 - uspesno, 1 - neuspesno
    int ind2;           // indikator postojanja slobodnih lokacija, 0 - nema, 1 - ima
    Bucket bucket;      
    Record record;
    int bucketIndex;    // indeks baketa sa nadjenim slogom
    int recordIndex;    // indeks sloga u baketu
} FindRecordResult;

int transformKey(int key);
int nextBucketIndex(int currentIndex);
void printRecord(Record record, int header);
void printBucket(Bucket bucket);
Record scanRecord(int withKey);
int scanKey();

#endif
----------------------------------------------Bucket.C
#include "bucket.h"

#include <stdio.h>

int transformKey(int id) {
    return id % B;
}

int nextBucketIndex(int currentIndex) {
    return (currentIndex + STEP) % B;
}

void printHeader() {
    printf("status \t key \t code \t date\n");//ovde treba se sto se isipisuje
}

void printRecord(Record record, int header) {
    if (header) printHeader();
    printf("%d \t %d \t %s \t %s\n", record.status, record.key, record.code, record.date);//i ovde
}

void printBucket(Bucket bucket) {
    int i;
    printHeader();
    for (i = 0; i < BUCKET_SIZE; i++) {
        printRecord(bucket.records[i], WITHOUT_HEADER);
    }
}

Record scanRecord(int withKey) {
    Record record;

    printf("\nUnesite slog: \n");
    
    if (withKey) {//je 1 .ovde bi trebalo da nunosis sve parametre sto traze record.statitreba
        printf("key = ");
        scanf("%d", &record.key);
    }

    printf("code = ");
    scanf("%s", record.code);
    printf("date = ");
    scanf("%s", record.date);

    return record;
}

int scanKey() {
    int key;
    printf("key = ");
    scanf("%d", &key);
    return key;
}
--------------------------------------------------------hash.h
#ifndef HASHFILE_H
#define HASHFILE_H

#define LOGICAL

#include <stdio.h>
#include "bucket.h"

int createHashFile(FILE *pFile);
int initHashFile(FILE *pFile, FILE *pFilein);

FindRecordResult findRecord(FILE *pFile, int key);
int insertRecord(FILE *pFile, Record record);
int modifyRecord(FILE *pFile, Record record);
int removeRecord(FILE *pFile, int key);
void printContent(FILE *pFile);

#endif
---------------------------------------------------------hash.c
#include "hash_file.h"

#include <stdio.h>
#include <stdlib.h>

#define OVERFLOW_FILE_NAME "overflow.dat"

int createHashFile(FILE *pFile) {
    Bucket *emptyContent = calloc(B, sizeof(Bucket));               // calloc inicializuje zauzeti
    fseek(pFile, 0, SEEK_SET);                                      // memorijski prostor nulama
    int retVal = fwrite(emptyContent, sizeof(Bucket), B, pFile);
    free(emptyContent);
    return retVal;
}

int saveBucket(FILE *pFile, Bucket* pBucket, int bucketIndex) {
    fseek(pFile, bucketIndex * sizeof(Bucket), SEEK_SET);
    int retVal = fwrite(pBucket, sizeof(Bucket), 1, pFile) == 1;
    fflush(pFile);
    return retVal;
}

int readBucket(FILE *pFile, Bucket *pBucket, int bucketIndex) {
    fseek(pFile, bucketIndex * sizeof(Bucket), SEEK_SET);
    return fread(pBucket, sizeof(Bucket), 1, pFile) == 1;
}

int readRecordFromSerialFile(FILE *pFile, Record *pRecord) {
    return fread(pRecord, sizeof(Record), 1, pFile);
}

int saveRecordToOverflowFile(FILE *pFile, Record *pRecord) {
    return fwrite(pRecord, sizeof(Record), 1, pFile);
}

int isBucketFull(Bucket bucket) {
    return bucket.records[BUCKET_SIZE - 1].status != EMPTY;
}

int initHashFile(FILE *pFile, FILE *pInputSerialFile) {
    if (feof(pFile)) {
        createHashFile(pFile);
    }

    FILE *pOverflowFile = fopen(OVERFLOW_FILE_NAME, "wb+");
    Record r;

    while(readRecordFromSerialFile(pInputSerialFile, &r)) {
        int h = transformKey(r.key);

        Bucket bucket;
        readBucket(pFile, &bucket, h);
        if (isBucketFull(bucket)) {                             // ukoliko nema mesta u maticnom baketu
            saveRecordToOverflowFile(pOverflowFile, &r);        // slog se smesta u privremenu
        } else {                                                // serijsku datoteku prekoracilaca
            insertRecord(pFile, r);
        }
    }

    fclose(pInputSerialFile);
    rewind(pOverflowFile);

    while(readRecordFromSerialFile(pOverflowFile, &r)) {         // upis prekoracilaca
        insertRecord(pFile, r);
    }

    fclose(pOverflowFile);                                       // zatvaranje i brisanje privremene
    remove(OVERFLOW_FILE_NAME);                                  // serijske datoteke prekoracilaca

    return 0;
}

FindRecordResult findRecord(FILE *pFile, int key) {//trazi u fajlu po kljucu
    int bucketIndex = transformKey(key);                            // transformacija kljuca u redni broj baketa
    int initialIndex = bucketIndex;                                 // redni broj maticnog baketa
    Bucket bucket;
    FindRecordResult retVal;

    retVal.ind1 = 99;                                               // indikator uspesnosti trazenja
    retVal.ind2 = 0;                                                // indikator postojanja slobodnih lokacija

    while (retVal.ind1 == 99) {
        int q = 0;                                                  // brojac slogova unutar baketa
        readBucket(pFile, &bucket, bucketIndex);                    // citanje baketa
        retVal.bucket = bucket;
        retVal.bucketIndex = bucketIndex;

        while (q < BUCKET_SIZE && retVal.ind1 == 99) {
            Record record = bucket.records[q];
            retVal.record = record;
            retVal.recordIndex = q;

            if (key == record.key && record.status != EMPTY) {
                retVal.ind1 = 0;                                    // uspesno trazenje
            } else if (record.status == EMPTY) {
                retVal.ind1 = 1;                                    // neuspesno trazenje
            } else {
                q++;                                                // nastavak trazenja
            }
        }

        if (q >= BUCKET_SIZE) {
            bucketIndex = nextBucketIndex(bucketIndex);             // prelazak na sledeci baket

            if (bucketIndex == initialIndex) {
                retVal.ind1 = 1;                                    // povratak na maticni baket
                retVal.ind2 = 1;
            }
        }
    }

    return retVal;
}

int alreadyExistsForInsert(FindRecordResult findResult) {
    if (findResult.ind1 == 0) {                             // da li je bilo uspesno trazenje
        #ifdef LOGICAL                                      /* za verziju sa logickim brisanjem */
        if (findResult.record.status == ACTIVE) {           // da li je slog aktivan
            return 1;                                       // slog sa istim kljucem vec postoji
        }
        #else                                               /* za verziju sa fizickim brisanjem */
        return 1;                                           // slog sa istim kljucem vec postoji
        #endif
    }

    return 0;
}

int insertRecord(FILE *pFile, Record record) {//dodavanje
    record.status = ACTIVE;
    FindRecordResult findResult = findRecord(pFile, record.key);

    if (alreadyExistsForInsert(findResult)) {                           // ukoliko slog sa datim kljucem vec postoji
        return -1;
    }

    if (findResult.ind2 == 1) {
        puts("Unos nemoguc. Datoteka popunjena.");
        return -1;
    }

    findResult.bucket.records[findResult.recordIndex] = record;         // upis sloga u baket na mesto gde je neuspesno zavrseno trazenje
                                                                        // ili aktivacija pothodno logicki obrisanog sloga sa istim kljucem

    if(saveBucket(pFile, &findResult.bucket, findResult.bucketIndex)) { // upis baketa u datoteku
        return findResult.bucketIndex;                                  // ukoliko je unos uspesan, povratna vrednost je redni broj baketa
    } else {
        return -2;
    }
}

int alreadyExistsForModify(FindRecordResult findResult) {
    if (findResult.ind1) {                              // da li je bilo neuspesno trazenje
        return 0;                                       // slog nije pronadjen
    }

    #ifdef LOGICAL                                      /* za verziju sa logickim brisanjem */
    if (findResult.record.status == DELETED) {          // da li pronadjeni slog logicki obrisan
        return 0;                                       // slog nije pronadjen
    }
    #endif

    return 1;
}

int modifyRecord(FILE *pFile, Record record) {
    record.status = ACTIVE;
    FindRecordResult findResult = findRecord(pFile, record.key);

    if (!alreadyExistsForModify(findResult)) {                          // ukoliko slog nije pronadjen ili je logicki obrisan
        return -1;
    }

    findResult.bucket.records[findResult.recordIndex] = record;         // upis sloga u baket na mesto gde je pronadjen

    if(saveBucket(pFile, &findResult.bucket, findResult.bucketIndex)) { // upis baketa u datoteku
        return findResult.bucketIndex;                                  // ukoliko je modifikacija uspesna, povratna vrednost je redni broj baketa
    } else {
        return -2;
    }
}

int removeRecordLogically(FILE *pFile, int key) {
    FindRecordResult findResult = findRecord(pFile, key);

    if (findResult.ind1) {
        return -1;                                                      // slog nije pronadjen
    }

    findResult.bucket.records[findResult.recordIndex].status = DELETED; // logicko brisanje sloga

    if(saveBucket(pFile, &findResult.bucket, findResult.bucketIndex)) { // upis baketa u datoteku
        return findResult.bucketIndex;                                  // ukoliko je brisanje uspesno, povratna vrednost je redni broj baketa
    } else {
        return -2;
    }
}

void testCandidate(Record record, int bucketIndex,                      // pomocna funkcija za proveru da li se odredjeni slog moze
                    int targetBucketIndex, int *pFound) {               // prebaciti u ciljni baket kako bi bio blize maticnom baketu
    int m = transformKey(record.key);

    if (bucketIndex > targetBucketIndex) {
        if (m > bucketIndex || m <= targetBucketIndex) {
            *pFound = 1;
        }
    } else {
        if (m > bucketIndex && m <= targetBucketIndex) {
            *pFound = 1;
        }
    }
}

int removeRecordPhysically(FILE *pFile, int key) {
    FindRecordResult findResult = findRecord(pFile, key);

    if (findResult.ind1) {
        return -1;                                                          // slog nije pronadjen
    }

    int q = findResult.recordIndex;                                         // indeks sloga za brisanje u baketu
    Bucket bucket = findResult.bucket;
    int bucketIndex = findResult.bucketIndex;

    int move = 1;                                                           // indikator pomeranja slogova

    while (move) {
        while (q < BUCKET_SIZE - 1 && bucket.records[q].status != EMPTY) {
            bucket.records[q] = bucket.records[q + 1];                      // pomeranje slogova unutar baketa (ulevo)
            q++;
        }

        Bucket targetBucket = bucket;                                       // ciljni baket u koji ce se (eventualno) vrsiti
        int targetBucketIndex = bucketIndex;                                // premestanje slogova prekoracilaca iz narednih baketa

        if (bucket.records[q].status == EMPTY) {
            move = 0;                                                       // baket nije bio pun
        } else {
            int found = 0;

            while (!found && move) {
                if (q == BUCKET_SIZE - 1) {
                    bucketIndex = nextBucketIndex(bucketIndex);             // ukoliko se doslo do kraja baketa
                    readBucket(pFile, &bucket, bucketIndex);                // dobavlja se naredni baket
                    q = -1;
                }

                q++;

                if (bucket.records[q].status != EMPTY && bucketIndex != targetBucketIndex) {
                    testCandidate(bucket.records[q], bucketIndex, targetBucketIndex, &found);
                } else {
                    move = 0;                                               // pomeranje se zaustavlja bilo da se
                }                                                           // naidje na prazan slog ili ciljni baket
            }

            if (found) {
                targetBucket.records[BUCKET_SIZE - 1] = bucket.records[q];  // nadjeni prekoracilac se prebacuje na kraj ciljnog baketa
            } else {
                targetBucket.records[BUCKET_SIZE - 1].status = EMPTY;       // poslednji slog se proglasava praznim
            }
        }

        saveBucket(pFile, &targetBucket, targetBucketIndex);                // ciljni baket se smesta u datoteku
        // ukoliko je nadjen i premesten odgovorajuci prekoracilac,
        // ceo postupak brisanja se ponavlja sad sa njegove prethodne pozicije
        // iz baketa u kojem je nadjen i time se ostavlja prostor da eventualno
        // neki drugi prekoracilac zauzme njegovo prethodno mesto, sto znaci da
        // imamo lancano pomeranje prekoracilaca prema maticnom baketu
    }

    return 0;
}

int removeRecord(FILE *pFile, int key) {
    #ifdef LOGICAL
    return removeRecordLogically(pFile, key);
    #else
    return removeRecordPhysically(pFile, key);
    #endif
}

void printContent(FILE *pFile) {
    int i;
    Bucket bucket;

    for (i = 0; i < B; i++) {
        readBucket(pFile, &bucket, i);
        printf("\n####### BUCKET - %d #######\n", i+1);
        printBucket(bucket);
    }
}
-----------------------------------main.c
#include <stdio.h>
#include <stdlib.h>

#include "hash_file.h"
#include "bucket.h"

#define LEN 30
#define DATA_DIR "data"                    // poseban direktorijum za datoteke
#define LS_CMD "dir "DATA_DIR""            // komanda za prikaz svih datoteka
#define DEFAULT_FILENAME "test.dat" 
#define DEFAULT_INFILENAME "in.dat"

int menu();
FILE *safeFopen(char filename[]);
void handleResult(int returnCode);
void handleFindResult(FindRecordResult findResult);
void fromTxtToSerialFile(FILE *pFtxt, FILE *pFbin);

int main() {
    FILE *pFile = safeFopen(DEFAULT_FILENAME), *pInputSerialFile, *pInputTxtFile;//otvara
    char filename[LEN];
    int option = -1, key;
    Record record;

    while (option) {
        option = menu();//menu
        switch (option) {
        case 1:
            puts("Postojece datoteke:");
            system(LS_CMD);
            if (pFile != NULL) fclose(pFile);
            printf("\nUnesite naziv datoteke: ");
            scanf("%s", filename);
            pFile = safeFopen(filename);//otvara
            break;

        case 2:
            record = scanRecord(WITH_KEY);
            handleResult(insertRecord(pFile, record));
            break;

        case 3:
            key = scanKey();
            FindRecordResult findResult = findRecord(pFile, key);

            if (findResult.ind1 != RECORD_FOUND || findResult.record.status != ACTIVE) {
                puts("Neupesno trazenje.");
            } else {
                printRecord(findResult.record, WITH_HEADER);
                record = scanRecord(WITHOUT_KEY);
                record.key = key;
                handleResult(modifyRecord(pFile, record));
            }

            break;

        case 4:
            key = scanKey();
            handleResult(removeRecord(pFile, key));
            break;

        case 5:
            printf("key = ");
            scanf("%d", &key);
            handleFindResult(findRecord(pFile, key));
            break;

        case 6:
            printContent(pFile);
            break;

        case 7:
            pInputTxtFile = fopen("in.txt", "r");
            pInputSerialFile = fopen(DEFAULT_INFILENAME, "wb+");
            fromTxtToSerialFile(pInputTxtFile, pInputSerialFile);
            rewind(pInputSerialFile);
            initHashFile(pFile, pInputSerialFile);
            fclose(pInputSerialFile);
            remove(DEFAULT_INFILENAME);
            break;

        default:
            break;
        }
    }

    if (pFile != NULL) fclose(pFile);

    return 0;
}

int menu() {
    int option;

    puts("\n\nIzaberite opciju:");
    puts("\t1. Otvaranje datoteke");//1
    puts("\t2. Unos novog sloga");
    puts("\t3. Modifikacija sloga");
    puts("\t4. Brisanje sloga");
    puts("\t5. Trazenje sloga");
    puts("\t6. Prikaz sadrzaja datoteke");
    puts("\t7. Formiranje u dva prolaza");
    puts("\t0. Kraj programa");

    printf(">>");

    scanf("%d", &option);

    return option;
}

FILE *safeFopen(char filename[]) {
    FILE *pFile;
    char fullFilename[2*LEN];
    sprintf(fullFilename, "%s/%s", DATA_DIR, filename);
    puts(fullFilename);

    pFile = fopen(fullFilename, "rb+");

    if (pFile == NULL) {                        // da li datoteka sa tim imenom vec postoji
        pFile = fopen(fullFilename, "wb+");     // ako ne, otvara se za pisanje
        createHashFile(pFile);                  // i kreira prazna rasuta organizacija
        puts("Kreirana prazna datoteka.");
    } else {
        puts("Otvorena postojeca datoteka.");   // ako da, koristi se postojeca datoteka
    }

    if (pFile == NULL) {
        printf("Nije moguce otvoriti/kreirati datoteku: %s.\n", filename);
    }

    return pFile;
}

void handleResult(int returnCode) {
    if (returnCode < 0) {
        puts("Operacija neuspesna.");
    } else {
        // u zavisnosti od operacija ovde se moze ispisati i
        // detaljnija poruka o razlogu neuspesnosti
        puts("Operacija uspesna.");
    }
}

void handleFindResult(FindRecordResult findResult) {
    if (findResult.ind1 != RECORD_FOUND) {
        puts("Neuspesno trazenje.");
    } else {
        printRecord(findResult.record, WITH_HEADER);
    }
}

void fromTxtToSerialFile(FILE *pInputTxtFile, FILE *pOutputSerialFile) {
    Record r;
    while(fscanf(pInputTxtFile, "%d%s%s", &r.key, r.code, r.date) != EOF) {
        fwrite(&r, sizeof(Record), 1, pOutputSerialFile);
    }
}


