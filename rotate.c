/* -*-  tab-width:4; c-basic-offset:4  -*- 
 * rotate.c -- determine Freerunner orientation.
 * Author   -- Chris Ball <cjb@laptop.org>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define EVENT_PATH "/dev/input/event3"
#define ORIENTATION_NORMAL    0
#define ORIENTATION_LEFT      1
#define ORIENTATION_RIGHT     2
#define ORIENTATION_INVERTED  3

void set_rotation (int rotation) {
    printf("In set_rotation, rotation = %d\n", rotation);
    switch (rotation)
    {
    case 0:
        system("xrandr -o normal");
        break;
    case 1:
        system("xrandr -o left");
        break;
    case 2:
        system("xrandr -o right");
        break;
    case 3:
        system("xrandr -o inverted");
        break;
    default:
        break;
    }
    /* Sleep for two seconds to give randr a chance to catch up. */
    sleep(2);
}

int process_packet (FILE *eventfp) {
    int state;
    int curline;
    unsigned short buffer[8 * 7];
    int i;
    unsigned short x;
    unsigned short y;
    unsigned short z;

    /* The raw data looks like:
     *  
     * portrait
     * 2ca3 48ab 43bf 000d 0000 0000 0000 0000 # start of packet
     * 2ca3 48ab 687b 000d 0002 0000 005a 0000 # X
     * 2ca3 48ab 68a8 000d 0002 0001 fc34 ffff # Y
     * 2ca3 48ab 68c1 000d 0002 0002 0048 0000 # Z
     * 2ca3 48ab 68c6 000d 0000 0000 0000 0000 # start of packet
     *
     * landscape
     * 2ca5 48ab 0f8f 0008 0002 0000 03cc 0000 # X
     * 2ca5 48ab 0fc4 0008 0002 0001 0168 0000 # Y
     * 2ca5 48ab 0fde 0008 0002 0002 005a 0000 # Z
     */

    /* We have to read enough to guarantee a full packet, since if
     * we try to read one line at a time we'll end up missing lines. */
    if (!read (fileno(eventfp), buffer, sizeof (buffer)))
    {
        fprintf(stderr, "fread failed\n");
        exit(1);
    }

    state = 0;
    /* We get 7 packets at once, to ensure that we have 3 good ones. 
     * Each of those has 8*2 bytes inside it. */
    for (i = 0; i <= 6; i++) {
        curline = i * 8;
        switch (state) {
            /* State machine:
             * 0: Find a new packet
             * 1: record X (jump to 0 on error)
             * 2: record Y (jump to 0 on error)
             * 3: record Z (jump to 0 on error), process packet, reset
             */
        case 0:
            if (buffer[curline + 4] == 0x0 && buffer[curline + 5] == 0x0) {
                x = y = z = 0;
                state = 1;
            }
            break;
        case 1:
            if (!(buffer[curline + 4] == 0x2 && buffer[curline + 5] == 0x0)) {
                printf("EXPECTED: 2 0: %x %x\n",
                       buffer[curline + 4], buffer[curline + 5]);
                state = 0;
                break;
            }
            x = buffer[curline + 7];
            state++;
            break;
        case 2:
            if (!(buffer[curline + 4] == 0x2 && buffer[curline + 5] == 0x1)) {
                printf("EXPECTED: 2 1: %x %x\n",
                       buffer[curline + 4], buffer[curline + 5]);
                state = 0;
                break;
            }
            y = buffer[curline + 7];
            state++;
            break;
        case 3:
            if (!(buffer[curline + 4] == 0x2 && buffer[curline + 5] == 0x2)) {
                printf("EXPECTED: 2 2: %x %x\n",
                       buffer[curline + 4], buffer[curline + 5]);
                state = 0;
                break;
            }
            z = buffer[curline + 7];
            /*
             * We finished a packet.  Process it. 
             * We test the final 4 bytes for:
             * ffff/ffff -- portrait
             * 0000/ffff -- portrait
             * 0000/0000 -- landscape
             * ffff/0000 -- landscape
             *
             * We might do better by using the previous eight bytes and
             * diagonal quadrants instead of the final four bytes, but
             * this seems to work out well for now.
             */
            if (y == 0xffff && (x == 0xffff || x == 0x0))
                return ORIENTATION_NORMAL;
            else if (x == 0x0 && y == 0x0)
                return ORIENTATION_LEFT;
            else if (x == 0xffff && y == 0x0)
                return ORIENTATION_RIGHT;
            else {
                fprintf(stderr, "Unhandled orientation\n");
                return -1;
            }
            break;
        default:
            /* Shouldn't get here. */
            fprintf(stderr, "We hit the default case; bailing out.\n");
            exit(1);
        }
    }

    /* If we get here, we didn't manage to get a complete packet. */
    fprintf(stderr, "Didn't process a full packet.\n");
    return -1;

}

int main (int argc, char *argv[]) {
    FILE *eventfp;
    int rotation = -1;
    int oldrotation = -1;

    /* Does this always hold on OpenMoko distros? */
    putenv("DISPLAY=:0");

    eventfp = fopen(EVENT_PATH, "r");
    if (eventfp == NULL) {
        fprintf(stderr, "Couldn't open file.\n");
        exit(1);
    }

    while (1) {
        sleep(1);
        if (rotation != -1)
            oldrotation = rotation;
		
        rotation = process_packet(eventfp);
        if (rotation != oldrotation && rotation != -1)
            set_rotation(rotation);
    }
}
