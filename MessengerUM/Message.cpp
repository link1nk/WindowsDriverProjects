#include <Windows.h>
#include <iostream>
#include "../MessengerKM/MsgType.h"

int Error(const char* message)
{
	std::cout << message << " error (" << std::hex << GetLastError() << ")\n";
	return 1;
}

int main(int argc, char* argv[])
{
	if (argc < 3)
	{
		std::cout << "Usage: .\\Message <id> <message>\n";
		return 0;
	}

	int id = atoi(argv[1]);
	const char* message = argv[2];

	HANDLE hMessenger = CreateFile(L"\\\\.\\Messenger", GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);

	if (hMessenger == INVALID_HANDLE_VALUE)
	{
		return Error("Falha ao abrir o dispositivo");
	}

	MsgType Msg{};

	Msg.MsgId = id;
	strcpy_s(Msg.Message, (sizeof Msg.Message) - 1, message);

	DWORD returned;

	BOOL success = WriteFile(hMessenger, &Msg, sizeof Msg, &returned, nullptr);

	if (!success)
	{
		return Error("Falha ao mandar a mensagem!");
	}

	std::cout << "Mensagem enviada com sucesso!\n";

	CloseHandle(hMessenger);

	return 0;
}