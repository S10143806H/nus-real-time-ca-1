#include "apples.h"
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#include <sys/ipc.h>
#include <sys/msg.h>


int NUM_APPLES = 50;    // Number of Apples to Process, to shorten the test
int TIME2ACT = 5;       // Time to Act between taking photo and discarding in seconds

struct photo_msgbuf {
    long mtype;
    struct photo_and_time {
        PHOTO photo;
        time_t time_start;
    } mdata;
};

struct quality_msgbuf {
    long mtype;
    struct quality_and_time {
        QUALITY quality;
        time_t time_start; 
    } mdata; 
};

/* timing helpers */
time_t get_time_now() {
    time_t now;
    now = time(&now);
    return now;
}

double get_time_elapsed(time_t t0) {
    time_t now = get_time_now();
    double time_elapsed = difftime(now, t0);
    return time_elapsed;
}

/* message queues */
int PHOTO_TYPE = 1;
int QUALITY_TYPE = 1;
int PHOTO_QID;
int QUALITY_QID;
void mq_init(void) {
    // key is a unique identifier
    PHOTO_QID = msgget(PHOTO_TYPE,IPC_CREAT|0666);
    QUALITY_QID = msgget(QUALITY_TYPE,IPC_CREAT|0666);
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
        time_t time_start = get_time_now();
        
        // take the photo
        PHOTO photo = take_photo();
    
        // push to photo message queue
        struct photo_msgbuf mbuf = {PHOTO_TYPE, {photo, time_start}};
        msgsnd(PHOTO_QID, &mbuf, sizeof(struct photo_msgbuf), IPC_NOWAIT);

        printf("%d Taker!\n", num_apples);
        // Wait until apple has passed so we can get the next apple!
        usleep(750 * 1000); // 500ms 

        num_apples--;
    }
    return;
}

void *manage_photo_processing(void *p) {
    int num_apples = NUM_APPLES;
    /* while (more_apples() && num_apples > 0) { */
    while (more_apples()) {
        // pop from the photo message queue
        struct photo_msgbuf mbuf; 
        msgrcv(PHOTO_QID, &mbuf, sizeof(struct photo_msgbuf), PHOTO_TYPE, 0);
        double time_elapsed = get_time_elapsed(mbuf.mdata.time_start);

        /* printf("%d   Processor!    %f \n", num_apples, time_elapsed); */
        // we still have time to act and process
        if (time_elapsed < TIME2ACT) {
            // send the photo to the Image Processing Unit
            QUALITY quality = process_photo(mbuf.mdata.photo);

            // push to quality message queue
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
            printf("%d   ||| blocked ||| \n", num_apples);
        }
        num_apples--;
    }
    return; 
}

void *manage_actuator(void *p) {
    int num_apples = NUM_APPLES;
    /* while (more_apples() && num_apples > 0) { */
    while (more_apples()) {    
        // pop from the quality message queue
        struct quality_msgbuf mbuf;
        msgrcv(QUALITY_QID, &mbuf, sizeof(struct quality_msgbuf), QUALITY_TYPE, 0);

        double time_elapsed = get_time_elapsed(mbuf.mdata.time_start);
        /* printf("%d      Actuator! %f \n", num_apples, time_elapsed); */

        if (mbuf.mdata.quality == BAD && time_elapsed <= 5) {
            printf("%d      Discard BAD\n", num_apples);
            double time_to_wait = 5 - get_time_elapsed(mbuf.mdata.time_start);
            usleep(time_to_wait * 1000 * 1000);
            discard_apple();
        }
        else if (mbuf.mdata.quality == UNKNOWN) {
            printf("%d      Discard UNKNOWN\n", num_apples);
            double time_to_wait = 5 - get_time_elapsed(mbuf.mdata.time_start);
            usleep(time_to_wait * 1000 * 1000);
            discard_apple();
        }
        else {
           // do nothing
           printf("%d      let it go! let it go!\n", num_apples);
        }
        num_apples--;
    }
    return;
}

int main () {
    start_test();
    
    mq_init();
    
    pthread_create(&manager1, NULL, manage_photo_taking, NULL);    
    pthread_create(&manager2, NULL, manage_photo_processing, NULL);    
    pthread_create(&manager3, NULL, manage_actuator, NULL);    
    // wait for all of them to complete
    pthread_join(manager1, NULL);
    pthread_join(manager2, NULL);
    pthread_join(manager3, NULL);
    printf("Test Completed");    
    end_test();
    return 0;
}
