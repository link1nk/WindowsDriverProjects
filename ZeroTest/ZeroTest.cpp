#include <Windows.h>
#include <iostream>

int Error(const char* err)
{
	std::cout << "[ERRO] " << err << " - (" << GetLastError() << ")\n";
	return 1;
}

int main(void)
{
	HANDLE hZero = CreateFile(L"\\\\.\\Zero", GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);

	if (hZero == INVALID_HANDLE_VALUE)
	{
		return Error("Não foi possivel abrir o device object");
	}

	BYTE buffer[64];

	for (int i{}; i < 64; i++)
	{
		buffer[i] = i;
	}

	/// Testando Read
	DWORD returned;

	BOOL success = ReadFile(hZero, buffer, sizeof buffer, &returned, nullptr);

	if (!success)
	{
		return Error("Falha ao ler do Device Object");
	}

	if (returned != sizeof buffer)
	{
		return Error("Numero invalido de bytes");
	}

	for (BYTE b : buffer)
	{
		if (b != 0)
		{
			std::cout << "Wrong Data!\n";
			break;
		}
	}

	/// Testando Write
	BYTE buffer2[1024]{0};

	success = WriteFile(hZero, buffer2, sizeof(buffer2), &returned, nullptr);

	if (!success)
	{
		return Error("Falha ao escrever");
	}

	if (returned != sizeof buffer2)
	{
		printf("Contagem errada de bytes\n");
	}
	CloseHandle(hZero);

}