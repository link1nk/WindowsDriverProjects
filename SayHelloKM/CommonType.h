#pragma once

#define DEVICE_SAY_HELLO 0x8123

#define IOCTL_SAYHELLO_WRITE_NAME CTL_CODE(DEVICE_SAY_HELLO, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

struct Person
{
	char Name[64];
	int Milliseconds;
};