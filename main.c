#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <string.h>

//below Book struck is located it hold info
struct Book{
    int PublisherId;
    int PublisherThreadId;
    char BookName[32];
};
typedef struct Book Book;

//below Publisher struck is located it hold info
struct Publisher{
    int PublisherId;
    char PublisherIdStr[10];
    pthread_mutex_t BufferLock;

    Book *BookBuffer;
    int LastPublishedBookNumber;
    int BufferIndex;
    int BufferSize;
    int FinishedThreadCount;

    pthread_t *PublisherThreads;
};
typedef struct Publisher Publisher;

//below PublisherThreadMetadata struck is located it hold related info
struct PublisherThreadMetadata{
    int ThreadId;
    int IsThreadFinished;
    Publisher *ParentPublisher;
};
typedef struct PublisherThreadMetadata PublisherThreadMetadata;


//below Package struck is located it hold related info
struct Package{
    int BufferIndex;
    Book* BookPackageBuffer;
};
typedef struct Package Package;


//below Packager struck is located it hold related info
struct Packager{
    int PackagerId;
    int PackageIndex;
    Package* PackageListBuffer;
};
typedef struct Packager Packager;

//common buffer and printf mutex are defined as global variable
Publisher *PublisherList;
pthread_mutex_t PrintFMutex = PTHREAD_MUTEX_INITIALIZER;


int PublisherTypeCount;
int PublisherThreadCount;
int PackagerCount;
int NumOfBookEachPublisher;
int PackageSize;
int InitialBufferSize;

//publisher threads go into here
void* PublisherRunnerFunction(void* args)   //this function runs the publisher threads
{
    //input is assigned
    PublisherThreadMetadata* metadata = (PublisherThreadMetadata* )args;
    Publisher *parentPublisher = (metadata->ParentPublisher);
    int threadId = metadata->ThreadId;

    int bookCountToPublish = NumOfBookEachPublisher;
    char* bookName = malloc(32 * sizeof(char));


    for (int i = 0; i < bookCountToPublish; ++i)
    {
        pthread_mutex_lock(&(metadata->ParentPublisher->BufferLock) ); // LOCK
        //book label is started here
        Book book;
        book.PublisherId = parentPublisher->PublisherId;
        book.PublisherThreadId = threadId;

        // BOOK NAME JOB START //////////////////////////////
        for (int j = 0; j < 32; ++j)
        {
            bookName[j] = '\0';
        }

        strcat(bookName, "Book");
        strcat(bookName, parentPublisher->PublisherIdStr);
        strcat(bookName,"_");

        char bookNumberStr[5];
        sprintf(bookNumberStr, "%d", parentPublisher->LastPublishedBookNumber++ );
        strcat(bookName, bookNumberStr);
        strcpy(book.BookName , bookName);
        // BOOK NAME JOB END //////////////////////////////

        // CHECK BUFFER SIZE START //////////////////////////////
        if(parentPublisher->BufferSize == parentPublisher->BufferIndex) // If buffer is full then enter this if statement
        {
            pthread_mutex_lock(&PrintFMutex); // PRINTF MUTEX LOCK
            printf("Publisher %d of type %d - Buffer is full. Resizing the buffer.\n",threadId,parentPublisher->PublisherId);
            pthread_mutex_unlock(&PrintFMutex); // PRINTF MUTEX Unlock

            int oldBufferSize = parentPublisher->BufferSize;
            int newBufferSize = oldBufferSize * 2;

            //create new buffer with double size
            Book* newBuffer = (Book*) malloc(newBufferSize* sizeof(Book));
            Book* oldBuffer = parentPublisher->BookBuffer;

            //old buffer values loaded into new one
            for (int j = 0; j < oldBufferSize; ++j)
            {
                newBuffer[j] = oldBuffer[j];
            }

            parentPublisher->BookBuffer = newBuffer;
            parentPublisher->BufferSize = newBufferSize;

            free(oldBuffer);
        }
        // CHECK BUFFER SIZE END //////////////////////////////

        //publisher produce book and assign related place
        parentPublisher->BookBuffer[parentPublisher->BufferIndex] = book;
        parentPublisher->BufferIndex++;

        pthread_mutex_lock(&PrintFMutex); // PRINTF MUTEX LOCK
        printf("Publisher %d of type %d" , metadata->ThreadId, parentPublisher->PublisherId);
        printf(" %s is published and put into the buffer %d\n" , book.BookName, parentPublisher->PublisherId);
        if(i == bookCountToPublish - 1) // If this thread is going to finish it's job.
        {
            printf("Publisher %d of type %d" , metadata->ThreadId, parentPublisher->PublisherId);
            printf(" - Finished publishing %d books. Exiting the system.\n" , bookCountToPublish);
            metadata->IsThreadFinished = 1;
            parentPublisher->FinishedThreadCount++;
        }
        pthread_mutex_unlock(&PrintFMutex); // PRINTF MUTEX UNLOCK

        pthread_mutex_unlock(&(metadata->ParentPublisher->BufferLock) ); // UNLOCK
    }

    free(bookName);
    pthread_exit(0);
}


//packager threads go into here
void* PackagerRunnerFunction(void* args)
{
    //packager is assigned random publisher pointer selected
    Packager* packager = (Packager*)args;
    int randomPublisher = rand() % PublisherTypeCount;
    Publisher* currentPublisher = &(PublisherList[randomPublisher]);
    int noBooksLeft = 0;

    packager->PackageListBuffer->BufferIndex = 0;
    packager->PackageListBuffer->BookPackageBuffer = (Book*) malloc(sizeof(Book) * PackageSize);

    //this loop goes until no book is left
    while(noBooksLeft == 0) {

        pthread_mutex_lock(&(currentPublisher->BufferLock));//LOCK

        if(currentPublisher->BufferIndex == 0)
        {
            if(currentPublisher->FinishedThreadCount == PublisherThreadCount)
            {
                // There are no books left, check if any other publisher got books
                int gotNoBooks = 1;
                for (int i = 0; i < PublisherTypeCount; ++i)
                {
                    if(PublisherList[i].BufferIndex != 0 || PublisherList[i].FinishedThreadCount != PublisherThreadCount) // If got books to publish
                    {
                        gotNoBooks = 0;
                        break;
                    }
                }

                if(gotNoBooks == 0)
                {
                    // There are still books to publish, change publisher
                    pthread_mutex_unlock( &(currentPublisher->BufferLock) );
                    randomPublisher = rand() % PublisherTypeCount;
                    currentPublisher = &(PublisherList[randomPublisher]);
                }
                else
                {
                    // All the books are published globally, exit.
                    pthread_mutex_lock(&PrintFMutex); // PRINTF MUTEX LOCK
                    printf("Packager %d", packager->PackagerId);

                    int packagedBookCount = packager->PackageListBuffer[packager->PackageIndex].BufferIndex;
                    printf(" - There are no publisher left in the system. \nOnly %d of %d number of books could be packaged.\n", packagedBookCount,PackageSize);
                    printf("These books are;");
                    Package package = packager->PackageListBuffer[packager->PackageIndex];
                    for (int i = 0; i < package.BufferIndex; ++i) {
                        printf(" %s",package.BookPackageBuffer[i].BookName);
                        printf((i == package.BufferIndex -1) ? "." : "," );
                    }
                    printf("\n");
                    printf("Now exiting the system.\n");
                    pthread_mutex_unlock(&PrintFMutex); // PRINTF MUTEX UNLOCK

                    noBooksLeft = 1;
                    pthread_mutex_unlock( &(currentPublisher->BufferLock) );
                }

            }
            else
            {
                // There are still books left on this publisher to publish, so just wait.
                pthread_mutex_unlock( &(currentPublisher->BufferLock) );
            }

        }
        else
        {

            Package* currentPackage = &(packager->PackageListBuffer[packager->PackageIndex]);
            if(currentPackage->BufferIndex == PackageSize)
            {
                pthread_mutex_lock(&PrintFMutex); // PRINTF MUTEX LOCK
                printf("Packager %d - Finished preparing one package, package contains;\n", packager->PackagerId);
                for (int i = 0; i < PackageSize; ++i) {
                    printf("%s",currentPackage->BookPackageBuffer[i].BookName);
                    printf((i == PackageSize -1) ? "." : ", " );
                }
                printf("\n");
                pthread_mutex_unlock(&PrintFMutex); // PRINTF MUTEX UNLOCK


                // Buffer is full, create new package;
                Package* newBuffer = (Package*) malloc( ((packager->PackageIndex) + 2) * sizeof(Package));
                Package* oldBuffer = packager->PackageListBuffer;

                for (int i = 0; i < (packager->PackageIndex+1); ++i)
                {
                    newBuffer[i] = oldBuffer[i];
                }
                packager->PackageIndex++;
                free(oldBuffer);
                packager->PackageListBuffer = newBuffer;

                Package* newPackage = &(newBuffer[packager->PackageIndex]);
                newPackage->BufferIndex = 0;
                newPackage->BookPackageBuffer = (Book*) malloc(PackageSize*sizeof(Book));

                currentPackage = newPackage;
            }

            //book is call to package
            Book bookToPackage = currentPublisher->BookBuffer[(currentPublisher->BufferIndex)-1];

            pthread_mutex_lock(&PrintFMutex); // PRINTF MUTEX LOCK
            printf("Packager %d  Put %s into the package\n", packager->PackagerId, bookToPackage.BookName);
            pthread_mutex_unlock(&PrintFMutex); // PRINTF MUTEX UNLOCK

            //book assigned related package
            currentPackage->BookPackageBuffer[currentPackage->BufferIndex] = bookToPackage;

            currentPackage->BufferIndex++;
            currentPublisher->BufferIndex--;

            pthread_mutex_unlock( &(currentPublisher->BufferLock) );//UNLOCK
            randomPublisher = rand() % PublisherTypeCount;
            currentPublisher = &(PublisherList[randomPublisher]);

        }
    }

    pthread_exit(0);
}



int main(int argc, char *argv[]) {

    
    int n1=atoi(argv[2]);
    int n2=atoi(argv[3]);
    int n3=atoi(argv[4]);
    int b1=atoi(argv[6]);
    int s1=atoi(argv[8]);
    int s2=atoi(argv[9]);
    
    printf("n1=%d n2=%d, n3=%d, b1=%d, s1=%d, s2=%d\nProgram starts now.\n\n",n1,n2,n3,b1,s1,s2);

    //inputs are assigned
    PublisherTypeCount = n1;
    PublisherThreadCount = n2;
    PackagerCount = n3;
    NumOfBookEachPublisher = b1;
    PackageSize = s1;
    InitialBufferSize = s2;

    //buffer initialized
    PublisherList = (Publisher*) malloc(PublisherTypeCount * sizeof(Publisher));
    pthread_t *PackagerThreads =(pthread_t*) malloc(PackagerCount * sizeof(pthread_t));

    printf("Thread-type and ID --------- Output\n");


    for (int i = 0; i < PublisherTypeCount; ++i)
    {
        //below inputs are assigned properly
        PublisherList[i].PublisherId = i;

        char publisherIdStr[10];
        sprintf(publisherIdStr, "%d", i);
        strcpy(PublisherList[i].PublisherIdStr, publisherIdStr);

        PublisherList[i].BufferSize = InitialBufferSize;
        PublisherList[i].BookBuffer = (Book*) malloc(InitialBufferSize * sizeof(Book));
        PublisherList[i].BufferIndex = 0;
        PublisherList[i].LastPublishedBookNumber = 0;
        PublisherList[i].FinishedThreadCount = 0;

        pthread_mutex_init(&(PublisherList[i].BufferLock), NULL);
        PublisherList[i].PublisherThreads = (pthread_t*) malloc(PublisherThreadCount * sizeof(pthread_t) );

        for (int j = 0; j < PublisherThreadCount; ++j)
        {
            //meta deta is loaded and sended with thread as struck
            PublisherThreadMetadata *metadata = (PublisherThreadMetadata*) malloc(sizeof(PublisherThreadMetadata));
            metadata->ThreadId = j;
            metadata->ParentPublisher = &(PublisherList[i]) ;
            metadata->IsThreadFinished = 0;

            //publisher threads created
            pthread_create(&(PublisherList[i].PublisherThreads[j]), NULL, &PublisherRunnerFunction, (void*)metadata);
        }
    }

    for (int i = 0; i < PackagerCount; ++i)
    {
        //packager values are loaded and sended with thread as struck
        Packager *packager = (Packager*)malloc(sizeof (Packager));
        packager->PackageIndex = 0;
        packager->PackageListBuffer = (Package*) malloc(sizeof(Package));
        packager->PackagerId = i;

        //packager threads created
        pthread_create(&(PackagerThreads[i]), NULL, &PackagerRunnerFunction, (void*)packager);
    }

    for (int i = 0; i < PublisherTypeCount; ++i)
    {
        for (int j = 0; j < PublisherThreadCount; ++j)
        {
            //publisher threads waited
            pthread_join( (PublisherList[i].PublisherThreads[j]),NULL );
        }
    }

    for (int i = 0; i < PublisherTypeCount; ++i)
    {
        //packager threads waited
        pthread_join( (PackagerThreads[i]),NULL );
    }

    printf("Finished!!\n");
    return 0;
}
