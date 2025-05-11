#include <Windows.h>
#include <iostream>
#include "..\NamedEventListenerKM\CustomData.h"

int Error(const char* msg)
{
	std::cout << "[ERROR] " << msg << " - code (" << GetLastError() << ")\n";
	return 1;
}

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		std::cout << "Uso: .\\ListenerUM <milliseconds>\n";
		return 0;
	}

	HANDLE hListener = CreateFile(L"\\\\.\\Listener", GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);

	if (hListener == INVALID_HANDLE_VALUE)
	{
		return Error("Não foi possivel abrir o Device Driver");
	}

	ULONG msec = atoi(argv[1]);

	CUSTOM_DATA cData{ msec };

	DWORD returned;

	BOOL success = WriteFile(hListener, &cData, sizeof CUSTOM_DATA, &returned, nullptr);

	if (!success)
	{
		return Error("Falha ao se comunicar com o driver");
	}

	std::cout << "[INFO] Listen mode iniciado com sucesso!\n";

	CloseHandle(hListener);

	return 0;
}