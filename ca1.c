#include "apples.h"
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/msg.h>


int NUM_APPLES = 25;    // Number of Apples to Process, to shorten the test
int TIME2ACT = 5;       // Time to Act between taking photo and discarding in seconds

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

double get_time_elapsed_msec(struct timeval *t0_p) {
    struct timeval now = get_time_now();
    double time_elapsed_msec = timevaldiff(t0_p, &now);
    return time_elapsed_msec;
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
        struct timeval time_start = get_time_now();
        
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
        printf("Waiting for photo receive\n");
        msgrcv(PHOTO_QID, &mbuf, sizeof(struct photo_msgbuf), PHOTO_TYPE, 0);
        printf("Received photo!\n");
        double time_elapsed_msec = get_time_elapsed_msec(&mbuf.mdata.time_start);

        // we still have time to act and process
        QUALITY quality;
        if (time_elapsed_msec <= TIME2ACT*1000) {
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
    printf("### Processor Dead\n");
}

void *manage_actuator(void *p) {
    int num_apples = NUM_APPLES;
    while (more_apples() && num_apples > 0) {
    /* while (more_apples()) { */    
        // pop from the quality message queue
        struct quality_msgbuf mbuf;
        msgrcv(QUALITY_QID, &mbuf, sizeof(struct quality_msgbuf), QUALITY_TYPE, 0);

        double time_elapsed_msec = get_time_elapsed_msec(&mbuf.mdata.time_start);

        printf("        time elapsed: %f\n", time_elapsed_msec);
        
        // if time_elapsed_msec is negative, there was some weird shit with msg queue and we should ignore it
        if (time_elapsed_msec >= 0) {
            double time_to_wait_msec = TIME2ACT*1000 - time_elapsed_msec;
            printf("        %f\n", time_to_wait_msec);

            if ((mbuf.mdata.quality == BAD && time_elapsed_msec <= TIME2ACT*1000) || mbuf.mdata.quality == UNKNOWN) {
                if (time_to_wait_msec >= 0) {
                    printf("        Sleeping %f msecs...\n", time_to_wait_msec);
                    usleep(time_to_wait_msec * 1000);
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
