/* gcc test_video.c -o test_video.out */

#include <stdio.h>
#include <stdlib.h>
#define SIZE 307200 // number of pixels (640x480 for my webcam)

int main() {
    FILE *camera, *grab;
    camera=fopen("/dev/video0", "rb");
    grab=fopen("grab.raw", "wb");
    float data[SIZE];
    fread(data, sizeof(data[0]), SIZE, camera);
    fwrite(data, sizeof(data[0]), SIZE, grab);
    fclose(camera);
    fclose(grab); 
    return 0;
}
