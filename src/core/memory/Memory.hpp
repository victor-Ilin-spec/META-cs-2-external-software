#ifndef _PPROCESS_HPP_
#define _PPROCESS_HPP_

#include <vector>
#include <math.h>
#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <iostream>
#include <Psapi.h> 

typedef NTSTATUS(WINAPI* pNtReadVirtualMemory)(HANDLE ProcessHandle, PVOID BaseAddress, PVOID Buffer, SIZE_T NumberOfBytesToRead, PSIZE_T NumberOfBytesRead);
typedef NTSTATUS(WINAPI* pNtWriteVirtualMemory)(HANDLE ProcessHandle, PVOID BaseAddress, PVOID Buffer, SIZE_T NumberOfBytesToWrite, PSIZE_T NumberOfBytesWritten);

inline pNtReadVirtualMemory NtReadFn()
{
    static const auto fn = reinterpret_cast<pNtReadVirtualMemory>(
        GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtReadVirtualMemory"));
    return fn;
}

inline pNtWriteVirtualMemory NtWriteFn()
{
    static const auto fn = reinterpret_cast<pNtWriteVirtualMemory>(
        GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtWriteVirtualMemory"));
    return fn;
}

struct ProcessModule
{
	uintptr_t base = 0;
	uintptr_t size = 0;
};

class pProcess
{
public:
	DWORD		  pid_ = 0;
	HANDLE		  handle_ = nullptr;
	HWND		  hwnd_ = nullptr;
	ProcessModule base_client_{};

public:
	bool AttachProcess(const char* process_name);
	bool AttachWindow(const char* window_name);
	bool UpdateHWND();
	void Close();

public:
	ProcessModule GetModule(const char* module_name);
	LPVOID		  Allocate(size_t size_in_bytes);
	uintptr_t	  FindCodeCave(uint32_t length_in_bytes);
	uintptr_t     FindSignature(std::vector<uint8_t> signature);
	uintptr_t     FindSignature(ProcessModule target_module, std::vector<uint8_t> signature);

	template<class T>
	uintptr_t ReadOffsetFromSignature(std::vector<uint8_t> signature, uint8_t offset) // offset example: "FF 05 ->22628B01<-" offset is 2
	{
		uintptr_t pattern_address = this->FindSignature(signature);
		if (!pattern_address)
			return 0x0;

		T offset_value = this->read<T>(pattern_address + offset);
		return pattern_address + offset_value + offset + sizeof(T);
	}

	bool read_raw(uintptr_t address, void* buffer, size_t size)
	{
		SIZE_T bytesRead = 0;
		auto fn = NtReadFn();
		if (!fn || !this->handle_ || !buffer || !size)
			return false;

		NTSTATUS status = fn(this->handle_, (PVOID)(address), buffer, size, &bytesRead);
		return status == 0x00000000 && bytesRead == size;
	}

	template<class T>
	void write(uintptr_t address, T value)
	{
		auto fn = NtWriteFn();
		if (!fn || !handle_)
			return;
		SIZE_T bytesWritten = 0;
		fn(handle_, (void*)address, &value, sizeof(T), &bytesWritten);
	}

	template<class T>
	T read(uintptr_t address)
	{
		T buffer{};
		auto fn = NtReadFn();
		if (!fn || !handle_)
			return buffer;
		SIZE_T bytesRead = 0;
		fn(handle_, (void*)address, &buffer, sizeof(T), &bytesRead);
		return buffer;
	}

	void write_bytes(uintptr_t addr, std::vector<uint8_t> patch)
	{
		auto fn = NtWriteFn();
		if (!fn || !handle_ || patch.empty())
			return;
		SIZE_T bytesWritten = 0;
		fn(handle_, (void*)addr, patch.data(), patch.size(), &bytesWritten);
	}

	uintptr_t read_multi_address(uintptr_t ptr, std::vector<uintptr_t> offsets)
	{
		uintptr_t buffer = ptr;
		for (int i = 0; i < offsets.size(); i++)
			buffer = this->read<uintptr_t>(buffer + offsets[i]);

		return buffer;
	}

	template <typename T>
	T read_multi(uintptr_t base, std::vector<uintptr_t> offsets) {
		uintptr_t buffer = base;
		for (int i = 0; i < offsets.size() - 1; i++)
		{
			buffer = this->read<uintptr_t>(buffer + offsets[i]);
		}
		return this->read<T>(buffer + offsets.back());
	}

private:
	uint32_t FindProcessIdByProcessName(const char* process_name);
	uint32_t FindProcessIdByWindowName(const char* window_name);
	HWND GetWindowHandleFromProcessId(DWORD ProcessId);
};
#endif