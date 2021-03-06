#include "apples.h"
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/msg.h>


int NUM_APPLES = 300;    // Number of Apples to Process, to shorten the test
int TIME2ACT = 5 * 1000;// Time to Act between taking photo and discarding in milliseconds

struct photo_msgbuf {
    long mtype;
    struct photo_and_time {
        PHOTO photo;
        struct timeval time_start;
    } mdata;
};

struct quality_msgbuf {
    long mtype;
    struct quality_and_time {
        QUALITY quality;
        struct timeval time_start; 
    } mdata; 
};

/* timing helpers */
struct timeval get_time_now() {
    struct timeval now;
    gettimeofday(&now, NULL);
    return now;
}

double timevaldiff(struct timeval *starttime, struct timeval *finishtime)
{
    double msec;
    msec=(finishtime->tv_sec-starttime->tv_sec)*1000;
    msec+=(finishtime->tv_usec-starttime->tv_usec)/1000;
    return msec;
}

double get_time_elapsed(struct timeval t0) {
    struct timeval now = get_time_now();
    double time_elapsed = timevaldiff(&t0, &now);
    return time_elapsed;
}

/* message queues */
int PHOTO_TYPE = 1;
int QUALITY_TYPE = 2;
int PHOTO_QID;
int QUALITY_QID;
void mq_init(void) {
    // key is a unique identifier
    PHOTO_QID = msgget(PHOTO_TYPE,IPC_CREAT|0666);
    QUALITY_QID = msgget(QUALITY_TYPE,IPC_CREAT|0666);
}

void mq_destroy(void) {
    msgctl(PHOTO_QID, IPC_RMID, NULL);
    msgctl(QUALITY_QID, IPC_RMID, NULL);
}

int mq_size(int msqid) {
    struct msqid_ds buf;
    msgctl(PHOTO_QID, IPC_STAT, &buf);
    return buf.msg_qnum;
}

/* thread managers and thread processes */
pthread_t manager1;
pthread_t manager2;
pthread_t manager3;

void *manage_photo_taking(void *p) {
    int num_apples = NUM_APPLES;
    /* while (more_apples() && num_apples > 0) { */
    while (more_apples()) {    
        // wait til you can take the photo
        wait_until_apple_under_camera();
        
        // timeStart
        struct timeval time_start = get_time_now();
        
        // take the photo
        PHOTO photo = take_photo();
    
        // push to photo message queue
        struct photo_msgbuf mbuf = {PHOTO_TYPE, {photo, time_start}};
        msgsnd(PHOTO_QID, &mbuf, sizeof(struct photo_msgbuf), IPC_NOWAIT);

        printf("%d Taker!\n", num_apples);
        // Wait until apple has passed so we can get the next apple!
        usleep(750 * 1000);

        num_apples--;
    }
    printf("### Taker Dead\n");
}

void *manage_photo_processing(void *p) {
    int num_apples = NUM_APPLES;
    /* while (more_apples() && num_apples > 0) { */
    while (more_apples()) {
        // pop from the photo message queue
        if (mq_size(PHOTO_QID) > 0) {
            struct photo_msgbuf mbuf;
            printf("Waiting for receive\n");
            msgrcv(PHOTO_QID, &mbuf, sizeof(struct photo_msgbuf), PHOTO_TYPE, 0);
            printf("Received!\n");
            
            double time_elapsed = get_time_elapsed(mbuf.mdata.time_start);

            /* printf("%d   Processor!    %f \n", num_apples, time_elapsed); */
            // we still have time to act and process
            QUALITY quality;
            if (time_elapsed <= TIME2ACT) {
                // send the photo to the Image Processing Unit
                quality = process_photo(mbuf.mdata.photo);
                // push the quality to the message queue
                struct quality_msgbuf mbuf_quality = {
                    QUALITY_TYPE,
                    {quality, mbuf.mdata.time_start}
                };
                msgsnd(QUALITY_QID, &mbuf_quality, sizeof(struct quality_msgbuf), IPC_NOWAIT);
                if (quality == GOOD) {
                    printf("%d   Detected: GOOD\n", num_apples);
                }
                else if (quality == BAD) {
                    printf("%d   Detected: BAD\n", num_apples);
                }
                else {
                    printf("%d   Detected: UNKNOWN\n", num_apples);
                }

            }
            else {
                printf("     | not fast enough |\n"); 
            }
            num_apples--;
        }
    }
    printf("### Processor Dead\n");
}

void *manage_actuator(void *p) {
    int num_apples = NUM_APPLES;
    /* while (more_apples() && num_apples > 0) { */
    while (more_apples()) {    
        if (mq_size(QUALITY_QID) > 0) {
            // pop from the quality message queue
            struct quality_msgbuf mbuf;
            msgrcv(QUALITY_QID, &mbuf, sizeof(struct quality_msgbuf), QUALITY_TYPE, 0);

            double time_elapsed = get_time_elapsed(mbuf.mdata.time_start);
            /* printf("%d      Actuator! %f \n", num_apples, time_elapsed); */

            printf("        time elapsed: %f\n", time_elapsed);
            double time_to_wait = TIME2ACT - time_elapsed;
            useconds_t time_to_wait_useconds = time_to_wait * 1000;
            printf("        %f\n", time_to_wait);

            if (mbuf.mdata.quality == BAD && time_elapsed <= TIME2ACT) {
                if (time_to_wait >= 0) {
                    printf("        Sleeping...\n");
                    usleep(time_to_wait_useconds);
                    printf("        Wake up...\n");
                    discard_apple();
                    printf("%d      Discard BAD\n", num_apples);
                }
            }
            else if (mbuf.mdata.quality == UNKNOWN) {
                if (time_to_wait >= 0) {
                    printf("        Sleeping...\n");
                    usleep(time_to_wait_useconds);
                    printf("        Wake up...\n");
                    discard_apple();
                    printf("%d      Discard UNKNOWN\n", num_apples);
                }
            }
            else {
               // do nothing
               printf("%d      let it go! let it go!\n", num_apples);
            }
            num_apples--;
        }
    }
    printf("### Actuator Dead\n");
}

int main () {
    start_test();
    // destroy any system message queues, in case they are left over
    mq_destroy();
    // then initialize again!
    mq_init();
    
    pthread_create(&manager1, NULL, manage_photo_taking, NULL);    
    pthread_create(&manager2, NULL, manage_photo_processing, NULL);    
    pthread_create(&manager3, NULL, manage_actuator, NULL);    
    // wait for all of them to complete
    pthread_join(manager1, NULL);
    pthread_join(manager2, NULL);
    pthread_join(manager3, NULL);
    
    // clean up this message queue we just created.
    mq_destroy();
    printf("Test Completed");    
    end_test();
    return 0;
}
