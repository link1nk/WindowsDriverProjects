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
	if (argc < 2)
	{
		return Error("Usage: .\\SayHelloUM.exe <name>");
	}

	const char* name = argv[1];

	HANDLE hSayHello = CreateFile(L"\\\\.\\SayHello", GENERIC_WRITE | GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, NULL);

	if (hSayHello == INVALID_HANDLE_VALUE)
	{
		return Error("Não foi possivel abrir um handle para o Device SayHello");
	}

	PERSON Person;

	strcpy_s(Person.Name, sizeof Person.Name, name);

	DWORD returned;

	BOOL ok = DeviceIoControl(hSayHello, IOCTL_SAYHELLO_WRITE_NAME, &Person, sizeof (Person), nullptr, 0, &returned, nullptr);

	if (!ok)
	{
		return Error("Falha ao fazer a IOCL");
	}

	std::cout << "Tudo ok!\n";

	CloseHandle(hSayHello);
}