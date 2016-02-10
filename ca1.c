#include "apples.h"
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#include <sys/ipc.h>
#include <sys/msg.h>


int num_apples = 10;


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

/* message queues */
int PHOTO_TYPE = 1;
int QUALITY_TYPE = 1;
int qid;
void mq_init(void) {
    // key is a unique identifier
    int key = 1;
    qid = msgget(key,IPC_CREAT|0666);
}


/* thread managers and thread processes */
pthread_t manager1;
pthread_t manager2;
pthread_t manager3;

void *manage_photo_taking(void *p) {
    while (more_apples() && num_apples >= 0) {
        num_apples--;
        printf("Taker!\n");
        
        // wait til you can take the photo
        wait_until_apple_under_camera();
        
        // timeStart
        time_t time_start;
        time_start =  time(&time_start);
        
        // take the photo
        PHOTO photo = take_photo();
    
        // store the photo + timeStart in the photo Message Queue
        struct photo_msgbuf mbuf = {PHOTO_TYPE, {photo, time_start}};
        msgsnd(qid, &mbuf, sizeof(struct photo_msgbuf), IPC_NOWAIT);

        // Wait until apple has passed so we can get the next apple!
        usleep(500 * 1000); // depreciated, values were selected by trial and error
    }
}

void *manage_photo_processing(void *p) {
    printf("   Processor!\n");
    while (more_apples() && num_apples >= 0) {

        // test if it worked
        struct photo_msgbuf mbuf_test; 
        msgrcv(qid, &mbuf_test, sizeof(struct photo_msgbuf), PHOTO_TYPE, IPC_NOWAIT);
        time_t now;
        now = time(&now);
        double time_elapsed;
        time_elapsed = difftime(now, mbuf_test.mdata.time_start);
        printf("%f\n", time_elapsed);
        // if (now - timeStart) < 5 seconds (still actionable)
        // send the photo to the Image Processing Unit
        // get the quality back
        // store the quality + timeStart in the quality message queue
    }    
}

void *manage_actuator(void *p) {
    printf("      Actuator!\n");
    while (more_apples() && num_apples >= 0) {
        int filler = 0;
        // if quality is bad and theres still time to discard, discard
        // if quality is bad but theres no time, let it go
        // if its good let it be
        // if its unknown, do some random discarding
    }
}

int main () {
    start_test();
    
    mq_init();
    
    int num_apples = 10;
    pthread_create(&manager1, NULL, manage_photo_taking, NULL);    
    pthread_create(&manager2, NULL, manage_photo_processing, NULL);    
    pthread_create(&manager3, NULL, manage_actuator, NULL);    
    // wait for all of them to complete
    pthread_join(manager1, NULL);
    pthread_join(manager2, NULL);
    pthread_join(manager3, NULL);
    
    end_test();
    return 0;
}
