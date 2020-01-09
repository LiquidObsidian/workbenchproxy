// native module to inject and communicate with the workbench proxy

#undef UNICODE
#include <napi.h>

#include <Windows.h>
#include <psapi.h>
#include <direct.h>

#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

std::string path_start;

DWORD GetWorkbenchPID() {
  const auto hwnd = FindWindowA( NULL, "Workbench - DayZ" );
  if (!hwnd) return 0;
  
  uint32_t proc_id;
	GetWindowThreadProcessId( hwnd, reinterpret_cast< LPDWORD >( &proc_id ) );

  return proc_id;
}

bool IsProxyAlreadyLoaded(DWORD pid) {
  if (pid == 0) return false;

  HANDLE proc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, pid);
  if (!proc) return false;

  HMODULE modules[1024];
  DWORD needed;
  if (!EnumProcessModules(proc, modules, sizeof(modules), &needed))
    return false;

  for (auto i = 0; i < (needed / sizeof(HMODULE)); i++) {
    TCHAR module_name[MAX_PATH];
    GetModuleFileNameEx(proc, modules[i], module_name, sizeof(module_name) / sizeof(TCHAR));
    
    if (strcmp(module_name, "WorkbenchProxy.dll") == 0) {
      return true;
    }
  }

  return false;
}

bool Inject(DWORD pid) {
	const auto hndl = OpenProcess( PROCESS_ALL_ACCESS, false, pid );
	if ( !hndl ) return false;

  const auto path = path_start + "\\WorkbenchProxy\\x64\\Release\\WorkbenchProxy.dll";

	const auto address = VirtualAllocEx( hndl, nullptr, path.size() + 1, MEM_COMMIT, PAGE_READWRITE );
	WriteProcessMemory( hndl, address, path.data(), path.size() + 1, nullptr );

	const auto loadlibrarya = GetProcAddress( LoadLibrary( "kernel32" ), "LoadLibraryA" );

	uint32_t thread_id;
	const auto thread_hndl = CreateRemoteThread(
		hndl, nullptr, 0, 
		reinterpret_cast< LPTHREAD_START_ROUTINE >( loadlibrarya ), 
		address, 
		0, reinterpret_cast< LPDWORD >( &thread_id ) );

	WaitForSingleObject( thread_hndl, 1000 );

	VirtualFreeEx( hndl, address, path.size(), MEM_RELEASE );

  return true;
}

Napi::Boolean Connect(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1) {
    return Napi::Boolean::New(env, false);
  }

  path_start = info[0].As<Napi::String>().Utf8Value();

  const auto pid = GetWorkbenchPID();
  if (!pid) {
    Napi::Error::New(env, "DayZ Workbench is not running.").ThrowAsJavaScriptException();
    return Napi::Boolean::New(env, false);
  }

  const auto loaded = IsProxyAlreadyLoaded(pid);
  if (!loaded) {
    Inject(pid);
  }

  return Napi::Boolean::New(env, true);
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set(Napi::String::New(env, "connectInternal"), Napi::Function::New(env, Connect));
  return exports;
}

NODE_API_MODULE(main, Init)