#include "apples.h"
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#include <sys/ipc.h>
#include <sys/msg.h>


int NUM_APPLES = 25;    // Number of Apples to Process, to shorten the test
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
int QUALITY_TYPE = 2;
int PHOTO_QID;
int QUALITY_QID;
void mq_init(void) {
    // key is a unique identifier
    PHOTO_QID = msgget(PHOTO_TYPE,IPC_CREAT|0666);
    QUALITY_QID = msgget(QUALITY_TYPE,IPC_CREAT|0666);
}


/* thread managers and thread processes */
pthread_t manager1; pthread_t manager2;
pthread_t manager3;

void *manage_photo_taking(void *p) {
    int num_apples = NUM_APPLES;
    while (more_apples() && num_apples > 0) {
    /* while (more_apples()) { */    
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
        usleep(500 * 1000); // 500ms 

        num_apples--;
    }
    printf("### Taker Dead\n");
}

void *manage_photo_processing(void *p) {
    int num_apples = NUM_APPLES;
    while (more_apples() && num_apples > 0) {
    /* while (more_apples()) { */
        // pop from the photo message queue
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
            printf("     | not fast enough |"); 
        }
        num_apples--;
    }
    printf("### Processor Dead\n");
}

void *manage_actuator(void *p) {
    int num_apples = NUM_APPLES;
    while (more_apples() && num_apples > 0) {
    /* while (more_apples()) { */    
        // pop from the quality message queue
        struct quality_msgbuf mbuf;
        msgrcv(QUALITY_QID, &mbuf, sizeof(struct quality_msgbuf), QUALITY_TYPE, 0);

        double time_elapsed = get_time_elapsed(mbuf.mdata.time_start);
        /* printf("%d      Actuator! %f \n", num_apples, time_elapsed); */

        printf("        time elapsed: %f\n", time_elapsed);
        double time_to_wait = 5.0 - time_elapsed;
        printf("        %f\n", time_to_wait);

        if ((mbuf.mdata.quality == BAD || mbuf.mdata.quality == UNKNOWN)
                && time_elapsed <= 5) {
            if (time_to_wait >= 0) {
                printf("        Sleeping %f seconds...\n", time_to_wait);
                usleep(time_to_wait * 1000 * 1000);
                printf("        Wake up...\n");
                discard_apple();
                printf("%d      Discard!\n", num_apples);
            }
        }
        else {
           // do nothing
           printf("%d      let it go! let it go!\n", num_apples);
        }
        num_apples--;
    }
    printf("### Actuator Dead\n");
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
