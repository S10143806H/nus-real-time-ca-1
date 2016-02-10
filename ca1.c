#include "apples.h"
#include <stdio.h>
#include <time.h>

int main () {
    // start test. function should be called before any function in apples API
    /* start_test(); */
    
    while (counter < 10) {
    /* while (more_apples()) { */
        wait_until_apple_under_camera();
        time_t time_start;
        time_start =  time(&time_start);
        PHOTO photo = take_photo();
        QUALITY quality = process_photo(photo);
        time_t now;
        now = time(&now);
        double time_elapsed;
        time_elapsed = difftime(now, time_start);
        printf("%f\n", time_elapsed);
        counter++;
    }
    /* end_test(); */
    return 0;
}


