#ifndef _APPLES_H_
#define _APPLES_H_

typedef int PHOTO;
typedef enum {GOOD, BAD, UNKNOWN} QUALITY;

void start_test(void);
int more_apples(void);
void wait_until_apple_under_camera(void);
PHOTO take_photo(void);
QUALITY process_photo(PHOTO photo);
void discard_apple(void);
void end_test(void);

#endif
