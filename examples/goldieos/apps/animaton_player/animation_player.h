#ifndef INCLUDE_ANIMATION_PLAYER_
#define INCLUDE_ANIMATION_PLAYER_
#include "pic_90x90/frame0.h"
#include "pic_90x90/frame1.h"
#include "pic_90x90/frame2.h"
#include "pic_90x90/frame3.h"
#include "pic_90x90/frame4.h"
#include "pic_90x90/frame5.h"
#include "pic_90x90/frame6.h"

#define MAX_ANIMATION_PICS_COUNT 10

typedef struct _Animation_Container{
	int count;
	const unsigned char* pic_list[MAX_ANIMATION_PICS_COUNT];
}Animation_Container;

#endif
