#include <cstdlib>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

// Function to relaunch current executable and exit
// Returns true if successful (all though it will never reach the return but ¯\_(ツ)_/¯), false otherwise
bool restartApplication() {
#if defined(_WIN32)
	char path[MAX_PATH];
	if (!GetModuleFileNameA(NULL, path, MAX_PATH)) return false;

	std::string cmd = std::string("\"") + path + "\"";
	STARTUPINFOA si; PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	// Create the new process
	std::vector<char> buf(cmd.begin(), cmd.end());				// CreateProcess modifies the buffer, so we give it writable memory
	buf.push_back('\0');

	if (CreateProcessA(NULL, buf.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		ExitProcess(0);
		return true;
	}
	return false;
#else
	char exePath[1024] = { 0 };
#endif
#if defined(__linux__)
	ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
	if (len > 0) {
		exePath[len] = '\0';
		execl(exePath, exePath, (char*)NULL);
		return true;
	}

	return false;
#endif
}