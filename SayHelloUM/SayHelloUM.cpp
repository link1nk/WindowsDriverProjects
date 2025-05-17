#include <Windows.h>
#include <iostream>
#include "..\SayHelloKM\CommonType.h"

int Error(const char* msg)
{
	std::cerr << "[ERROR] " << msg << " (" << GetLastError() << ")\n";
	return 1;
}

int main(int argc, char* argv[])
{
	if (argc < 3)
	{
		return Error("Usage: .\\SayHelloUM.exe <milliseconds> <name>");
	}

	int milliseconds = atoi(argv[1]);
	const char* name = argv[2];

	HANDLE hSayHello = CreateFile(L"\\\\.\\SayHello", GENERIC_WRITE | GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, NULL);

	if (hSayHello == INVALID_HANDLE_VALUE)
	{
		return Error("Não foi possivel abrir um handle para o Device SayHello");
	}

	Person person;

	person.Milliseconds = milliseconds;
	strcpy_s(person.Name, sizeof person.Name, name);

	DWORD returned;

	BOOL ok = DeviceIoControl(hSayHello, IOCTL_SAYHELLO_WRITE_NAME, &person, sizeof person, nullptr, 0, &returned, nullptr);

	if (!ok)
	{
		return Error("Falha ao fazer a IOCL");
	}

	std::cout << "Tudo ok!\n";

	CloseHandle(hSayHello);
}